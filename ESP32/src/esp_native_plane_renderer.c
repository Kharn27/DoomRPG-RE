#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_native_plane_renderer.h"

#define MAPPINGS_HEADER_BYTES 16U
#define MAPPING_PAIR_BYTES 8U
#define PALETTES_HEADER_BYTES 4U
#define WALL_TEXEL_HEADER_BYTES 4U
#define PLANE_TEXTURE_BYTES 2048U
#define PLANE_PALETTE_COLORS 16U
#define MAX_PLANE_TEXTURES 24U
#define PLANE_CACHE_SLOTS 6U
#define EXPECTED_PLANE_MAP_BYTES 2048U

typedef struct PlaneTextureDesc_s {
    uint8_t logicalId;
    uint8_t reserved0;
    uint16_t actualId;
    uint32_t sourceTexelOffset;
    uint16_t paletteRgb565[PLANE_PALETTE_COLORS];
} PlaneTextureDesc;

typedef struct PlaneCacheSlot_s {
    const PlaneTextureDesc* source;
    uint8_t* texels;
    uint32_t lastUse;
    uint8_t valid;
} PlaneCacheSlot;

typedef struct PlaneWork_s {
    Render_t* render;
    const EspMapRuntimeView* runtime;
    EspAssetPackEntry wallTexels;
    PlaneTextureDesc textures[MAX_PLANE_TEXTURES];
    uint16_t textureCount;
    PlaneCacheSlot cache[PLANE_CACHE_SLOTS];
    uint32_t cacheClock;
    uint32_t acquireMicros;
    EspNativePlaneRenderStats stats;
} PlaneWork;

static EspNativePlaneRenderStats planeStats;

static uint32_t elapsedMicros(int64_t start) {
    int64_t elapsed = esp_timer_get_time() - start;
    if (elapsed <= 0) return 0U;
    if ((uint64_t)elapsed > UINT32_MAX) return UINT32_MAX;
    return (uint32_t)elapsed;
}

static void accumulateMicros(uint32_t* total, uint32_t delta) {
    if (total == NULL) return;
    if (delta > UINT32_MAX - *total) *total = UINT32_MAX;
    else *total += delta;
}

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* Match Render_loadPalettes(): source palettes.bin words have their 5-bit
 * red/blue channels opposite to the RGB565 words used by the framebuffer. */
static uint16_t sourceToFramebuffer565(uint16_t color) {
    return (uint16_t)(((color & 0x001fU) << 11) |
                      (color & 0x07e0U) |
                      ((color & 0xf800U) >> 11));
}

static int findTextureIndex(const PlaneWork* work, uint8_t logicalId) {
    uint16_t i;
    if (work == NULL) return -1;
    for (i = 0U; i < work->textureCount; ++i) {
        if (work->textures[i].logicalId == logicalId) return (int)i;
    }
    return -1;
}

static int collectTextures(PlaneWork* work) {
    uint32_t i;
    if (work == NULL || work->runtime == NULL ||
        work->runtime->planeMap == NULL ||
        work->runtime->planeMapBytes != EXPECTED_PLANE_MAP_BYTES) return 0;

    work->textureCount = 0U;
    for (i = 0U; i < EXPECTED_PLANE_MAP_BYTES; ++i) {
        const uint8_t id = work->runtime->planeMap[i];
        if (findTextureIndex(work, id) >= 0) continue;
        if (work->textureCount >= MAX_PLANE_TEXTURES) return 0;
        memset(&work->textures[work->textureCount], 0,
               sizeof(work->textures[work->textureCount]));
        work->textures[work->textureCount].logicalId = id;
        ++work->textureCount;
    }
    return work->textureCount > 0U;
}

static int resolveTextures(PlaneWork* work) {
    EspAssetPackEntry mappings;
    EspAssetPackEntry palettes;
    uint8_t mappingHeader[MAPPINGS_HEADER_BYTES];
    uint8_t paletteHeader[PALETTES_HEADER_BYTES];
    uint32_t texelPairs;
    uint32_t bitShapePairs;
    uint32_t textureIdCount;
    uint32_t spriteIdCount;
    uint32_t paletteBytes;
    uint32_t paletteEntries;
    uint32_t textureIdBase;
    uint64_t expectedMappingsBytes;
    uint8_t wallHeader[WALL_TEXEL_HEADER_BYTES];
    uint32_t wallDataBytes;
    uint16_t i;

    if (work == NULL || !EspAssetPack_isOpen() ||
        !EspAssetPack_findEntry("mappings.bin", &mappings) ||
        !EspAssetPack_findEntry("palettes.bin", &palettes) ||
        !EspAssetPack_findEntry("wtexels.bin", &work->wallTexels) ||
        mappings.size < MAPPINGS_HEADER_BYTES ||
        palettes.size < PALETTES_HEADER_BYTES ||
        work->wallTexels.size < WALL_TEXEL_HEADER_BYTES ||
        !EspAssetPack_readRange(&mappings, 0U, mappingHeader,
                                sizeof(mappingHeader)) ||
        !EspAssetPack_readRange(&palettes, 0U, paletteHeader,
                                sizeof(paletteHeader)) ||
        !EspAssetPack_readRange(&work->wallTexels, 0U, wallHeader,
                                sizeof(wallHeader))) return 0;

    texelPairs = readLe32(mappingHeader);
    bitShapePairs = readLe32(mappingHeader + 4U);
    textureIdCount = readLe32(mappingHeader + 8U);
    spriteIdCount = readLe32(mappingHeader + 12U);
    paletteBytes = readLe32(paletteHeader);
    wallDataBytes = readLe32(wallHeader);

    expectedMappingsBytes =
        (uint64_t)MAPPINGS_HEADER_BYTES +
        (uint64_t)texelPairs * MAPPING_PAIR_BYTES +
        (uint64_t)bitShapePairs * MAPPING_PAIR_BYTES +
        (uint64_t)textureIdCount * 2U +
        (uint64_t)spriteIdCount * 2U;

    if (texelPairs == 0U || bitShapePairs == 0U || textureIdCount == 0U ||
        texelPairs > 4096U || bitShapePairs > 4096U ||
        textureIdCount > 4096U || spriteIdCount > 4096U ||
        expectedMappingsBytes != mappings.size ||
        (paletteBytes & 1U) != 0U ||
        paletteBytes + PALETTES_HEADER_BYTES != palettes.size ||
        wallDataBytes + WALL_TEXEL_HEADER_BYTES != work->wallTexels.size) {
        return 0;
    }

    paletteEntries = paletteBytes / 2U;
    textureIdBase = MAPPINGS_HEADER_BYTES +
                    texelPairs * MAPPING_PAIR_BYTES +
                    bitShapePairs * MAPPING_PAIR_BYTES;

    for (i = 0U; i < work->textureCount; ++i) {
        PlaneTextureDesc* out = &work->textures[i];
        uint8_t idBytes[2];
        uint8_t pair[MAPPING_PAIR_BYTES];
        uint8_t paletteRaw[PLANE_PALETTE_COLORS * 2U];
        uint32_t actual;
        int32_t sourceOffset;
        int32_t paletteOffset;
        uint32_t p;

        if ((uint32_t)out->logicalId >= textureIdCount ||
            !EspAssetPack_readRange(&mappings,
                                    textureIdBase +
                                        (uint32_t)out->logicalId * 2U,
                                    idBytes, sizeof(idBytes))) return 0;

        actual = readLe16(idBytes);
        if (actual >= texelPairs || actual > UINT16_MAX ||
            !EspAssetPack_readRange(&mappings,
                                    MAPPINGS_HEADER_BYTES +
                                        actual * MAPPING_PAIR_BYTES,
                                    pair, sizeof(pair))) return 0;

        sourceOffset = (int32_t)readLe32(pair);
        paletteOffset = (int32_t)readLe32(pair + 4U);
        if (sourceOffset < 0 || (sourceOffset & 1) != 0 ||
            paletteOffset < 0 ||
            (uint32_t)paletteOffset > paletteEntries ||
            PLANE_PALETTE_COLORS >
                paletteEntries - (uint32_t)paletteOffset ||
            WALL_TEXEL_HEADER_BYTES + ((uint32_t)sourceOffset >> 1) >
                work->wallTexels.size ||
            PLANE_TEXTURE_BYTES >
                work->wallTexels.size -
                    (WALL_TEXEL_HEADER_BYTES +
                     ((uint32_t)sourceOffset >> 1))) return 0;

        if (!EspAssetPack_readRange(
                &palettes,
                PALETTES_HEADER_BYTES + (uint32_t)paletteOffset * 2U,
                paletteRaw, sizeof(paletteRaw))) return 0;

        out->actualId = (uint16_t)actual;
        out->sourceTexelOffset = (uint32_t)sourceOffset;
        for (p = 0U; p < PLANE_PALETTE_COLORS; ++p) {
            out->paletteRgb565[p] =
                sourceToFramebuffer565(readLe16(&paletteRaw[p * 2U]));
        }
    }
    return 1;
}

/* Six independent 2048-byte leases preserve the hardware-proven six-slot LRU
 * without requiring one contiguous 12288-byte heap block. This matters after
 * the resident PAK/cache owner is active: Entrance hardware reported largest8
 * below 12 KiB even though ample total 8-bit heap remained. */
static void releaseCache(PlaneWork* work) {
    uint32_t i;
    if (work == NULL) return;
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        free(work->cache[i].texels);
        work->cache[i].texels = NULL;
        work->cache[i].source = NULL;
        work->cache[i].lastUse = 0U;
        work->cache[i].valid = 0U;
    }
}

static int initCache(PlaneWork* work) {
    uint32_t i;
    if (work == NULL) return 0;
    memset(work->cache, 0, sizeof(work->cache));
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        work->cache[i].texels = (uint8_t*)malloc(PLANE_TEXTURE_BYTES);
        if (work->cache[i].texels == NULL) {
            releaseCache(work);
            return 0;
        }
    }
    return 1;
}

static int acquireTexture(PlaneWork* work,
                          const PlaneTextureDesc* source,
                          const uint8_t** outTexels) {
    PlaneCacheSlot* target = NULL;
    uint32_t oldest = UINT32_MAX;
    uint32_t i;
    uint32_t readOffset;
    int64_t readStart;
    uint32_t readMicros;

    if (outTexels != NULL) *outTexels = NULL;
    if (work == NULL || source == NULL || outTexels == NULL) return 0;

    ++work->cacheClock;
    if (work->cacheClock == 0U) work->cacheClock = 1U;

    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) return 0;
        if (slot->valid && slot->source != NULL &&
            slot->source->actualId == source->actualId &&
            slot->source->sourceTexelOffset == source->sourceTexelOffset) {
            slot->lastUse = work->cacheClock;
            ++work->stats.cacheHits;
            *outTexels = slot->texels;
            return 1;
        }
    }

    ++work->stats.cacheMisses;
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (!slot->valid) {
            target = slot;
            break;
        }
        if (slot->lastUse < oldest) {
            oldest = slot->lastUse;
            target = slot;
        }
    }
    if (target == NULL) return 0;
    if (target->valid) ++work->stats.cacheEvictions;

    readOffset = WALL_TEXEL_HEADER_BYTES + (source->sourceTexelOffset >> 1);
    if (readOffset > work->wallTexels.size ||
        PLANE_TEXTURE_BYTES > work->wallTexels.size - readOffset) return 0;

    readStart = esp_timer_get_time();
    if (!EspAssetPack_readRange(&work->wallTexels, readOffset,
                                target->texels, PLANE_TEXTURE_BYTES)) {
        accumulateMicros(&work->acquireMicros, elapsedMicros(readStart));
        return 0;
    }
    readMicros = elapsedMicros(readStart);
    accumulateMicros(&work->acquireMicros, readMicros);

    target->source = source;
    target->lastUse = work->cacheClock;
    target->valid = 1U;
    work->stats.texelReadBytes += PLANE_TEXTURE_BYTES;
    *outTexels = target->texels;
    return 1;
}

static int spanPlane(PlaneWork* work,
                     int x, int y,
                     const uint8_t* planeCells,
                     int32_t param5, int32_t param6,
                     int32_t param7, int32_t param8,
                     int count) {
    uint16_t* pixels;
    int pitch;

    if (work == NULL || work->render == NULL || planeCells == NULL ||
        count <= 0) return 0;

    pitch = work->render->pitch >> 1;
    pixels = (uint16_t*)work->render->pixels + pitch * y + x;

    while (--count >= 0) {
        const uint32_t u5 = (uint32_t)param5;
        const uint32_t u6 = (uint32_t)param6;
        const uint32_t cellIndex =
            ((u6 >> 17) & 0x3e0U) + ((u5 << 5) >> 27);
        const uint32_t texelIndex =
            ((u6 >> 10) & 0x0fc0U) + ((u5 << 10) >> 26);
        const uint8_t logicalId = planeCells[cellIndex];
        const int descIndex = findTextureIndex(work, logicalId);
        const PlaneTextureDesc* desc;
        const uint8_t* texels;
        uint8_t packed;
        uint8_t paletteIndex;

        if (cellIndex >= ESP_MAP_PLANE_CELL_COUNT || texelIndex >= 4096U ||
            descIndex < 0) return 0;
        desc = &work->textures[descIndex];
        if (!acquireTexture(work, desc, &texels)) return 0;

        packed = texels[texelIndex >> 1];
        paletteIndex = (uint8_t)((texelIndex & 1U)
                                     ? ((packed >> 4) & 0x0fU)
                                     : (packed & 0x0fU));
        *pixels++ = desc->paletteRgb565[paletteIndex];
        ++work->stats.pixelsRendered;

        param5 = (int32_t)((uint32_t)param5 + (uint32_t)param7);
        param6 = (int32_t)((uint32_t)param6 + (uint32_t)param8);
    }
    return 1;
}

static int drawPlaneRow(PlaneWork* work,
                        int y,
                        const uint8_t* planeCells) {
    Render_t* render;
    int width;
    int height;
    int32_t viewX;
    int32_t viewY;
    int32_t viewZ;
    int32_t viewSin;
    int32_t viewCos;
    int32_t doubledX;
    int32_t doubledY;
    int32_t zScale;
    int32_t distance;
    int32_t projected;
    uint32_t projectedUnsigned;
    int32_t step;
    int32_t startX;
    int32_t startY;
    int32_t stepX;
    int32_t stepY;

    if (work == NULL || planeCells == NULL || work->render == NULL) return 0;
    render = work->render;
    width = render->screenWidth;
    height = render->screenHeight;
    if (width <= 0 || height <= 0) return 0;

    viewX = (int32_t)((uint32_t)render->viewX << 16);
    viewY = (int32_t)((uint32_t)render->viewY << 16);
    viewZ = render->viewZ;
    viewCos = render->viewCos;
    viewSin = render->viewSin;
    doubledX = render->screenLeft << 1;
    doubledY = y << 1;

    if (height > doubledY) viewZ = 64 - viewZ;
    zScale = viewZ << 3;
    distance = height > doubledY ? height - doubledY : doubledY - height;
    projected = (width * zScale) / (distance + 1);
    projectedUnsigned = (uint32_t)projected;
    step = (int32_t)((((int64_t)projected << 19) /
                      (32 * width)) >> 8);

    startX = (int32_t)(
        (int64_t)viewX + (((int64_t)viewCos * projectedUnsigned) >> 3) +
        (((int64_t)step * (viewSin >> 8) *
          (doubledX - width + 1)) >> 1));
    startY = (int32_t)(
        (int64_t)viewY + (((int64_t)(-viewSin) * projectedUnsigned) >> 3) +
        (((int64_t)step * (viewCos >> 8) *
          (doubledX - width + 1)) >> 1));
    stepX = (int32_t)((int64_t)step * (viewSin >> 8));
    stepY = (int32_t)((int64_t)step * (viewCos >> 8));

    return spanPlane(work,
                     doubledX >> 1,
                     doubledY >> 1,
                     planeCells,
                     startX, startY, stepX, stepY,
                     render->screenRight);
}

void EspNativePlaneRenderer_reset(void) {
    memset(&planeStats, 0, sizeof(planeStats));
}

const EspNativePlaneRenderStats* EspNativePlaneRenderer_view(void) {
    return planeStats.active == 1U ? &planeStats : NULL;
}

int EspNativePlaneRenderer_render(struct Render_s* renderBase) {
    Render_t* render = (Render_t*)renderBase;
    PlaneWork work;
    int64_t totalStart;
    int64_t phaseStart;
    uint32_t collectMicros = 0U;
    uint32_t resolveMicros = 0U;
    uint32_t allocMicros = 0U;
    uint32_t rasterMicros = 0U;
    uint32_t releaseMicros;
    uint32_t totalMicros;
    uint32_t rasterCpuMicros;
    int y;
    int ok = 0;

    EspNativePlaneRenderer_reset();
    memset(&work, 0, sizeof(work));
    work.render = render;
    work.runtime = EspMapRuntime_view();

    if (render == NULL || work.runtime == NULL ||
        !EspMapRuntime_isLoaded() || !EspAssetPack_isOpen() ||
        render->pixels == NULL || render->framebuffer == NULL ||
        render->screenWidth != 160 || render->screenHeight != 80 ||
        render->screenLeft != 0 || render->screenTop != 0 ||
        render->screenRight != render->screenWidth ||
        render->screenBottom != render->screenHeight ||
        work.runtime->planeMap == NULL ||
        work.runtime->planeMapBytes != EXPECTED_PLANE_MAP_BYTES) {
        return 0;
    }

    totalStart = esp_timer_get_time();

    phaseStart = esp_timer_get_time();
    if (!collectTextures(&work)) {
        collectMicros = elapsedMicros(phaseStart);
        goto done;
    }
    collectMicros = elapsedMicros(phaseStart);

    phaseStart = esp_timer_get_time();
    if (!resolveTextures(&work)) {
        resolveMicros = elapsedMicros(phaseStart);
        goto done;
    }
    resolveMicros = elapsedMicros(phaseStart);

    phaseStart = esp_timer_get_time();
    if (!initCache(&work)) {
        allocMicros = elapsedMicros(phaseStart);
        goto done;
    }
    allocMicros = elapsedMicros(phaseStart);

    work.stats.uniqueLogicalTextures = work.textureCount;
    phaseStart = esp_timer_get_time();
    for (y = 0; y < render->halfScreenHeight; ++y) {
        if (!drawPlaneRow(&work, y,
                          work.runtime->planeMap + ESP_MAP_PLANE_CELL_COUNT)) {
            rasterMicros = elapsedMicros(phaseStart);
            goto done;
        }
        ++work.stats.rowsRendered;
    }
    for (; y < render->screenHeight; ++y) {
        if (!drawPlaneRow(&work, y, work.runtime->planeMap)) {
            rasterMicros = elapsedMicros(phaseStart);
            goto done;
        }
        ++work.stats.rowsRendered;
    }
    rasterMicros = elapsedMicros(phaseStart);

    if (work.stats.rowsRendered != (uint32_t)render->screenHeight ||
        work.stats.pixelsRendered !=
            (uint32_t)render->screenWidth * (uint32_t)render->screenHeight) {
        goto done;
    }

    work.stats.rendered = 1U;
    work.stats.active = 1U;
    ok = 1;

done:
    phaseStart = esp_timer_get_time();
    releaseCache(&work);
    releaseMicros = elapsedMicros(phaseStart);
    totalMicros = elapsedMicros(totalStart);
    rasterCpuMicros = rasterMicros >= work.acquireMicros
                          ? rasterMicros - work.acquireMicros
                          : 0U;

    /* All timings are captured before either diagnostic printf. `acquire`
     * covers only the 2048-byte PAK range calls made on the six-slot local LRU
     * misses; `rasterCpu` is the remaining row/pixel work. No new cache or
     * persistent owner is introduced by this profile. */
    printf("[PLANEDETAIL] total=%u collect=%u resolve=%u alloc=%u raster=%u acquire=%u rasterCpu=%u release=%u misses=%u evictions=%u ok=%u\n",
           (unsigned int)totalMicros,
           (unsigned int)collectMicros,
           (unsigned int)resolveMicros,
           (unsigned int)allocMicros,
           (unsigned int)rasterMicros,
           (unsigned int)work.acquireMicros,
           (unsigned int)rasterCpuMicros,
           (unsigned int)releaseMicros,
           (unsigned int)work.stats.cacheMisses,
           (unsigned int)work.stats.cacheEvictions,
           (unsigned int)(ok != 0));

    if (ok) {
        planeStats = work.stats;
        printf("[NATIVEPLANE] rows=%u pixels=%u textures=%u cache=%uH/%uM/%uE reads=%uB\n",
               (unsigned int)planeStats.rowsRendered,
               (unsigned int)planeStats.pixelsRendered,
               (unsigned int)planeStats.uniqueLogicalTextures,
               (unsigned int)planeStats.cacheHits,
               (unsigned int)planeStats.cacheMisses,
               (unsigned int)planeStats.cacheEvictions,
               (unsigned int)planeStats.texelReadBytes);
    }
    else {
        EspNativePlaneRenderer_reset();
        printf("[NATIVEPLANE] FAILED textured floor/ceiling reconstruction\n");
    }
    return ok;
}
