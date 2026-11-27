#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "doomrpg_log.h"
#include "esp_map_runtime.h"
#include "esp_native_plane_renderer.h"
#include "esp_render_geometry_primitives.h"
#include "esp_native_raster_contract.h"
#if DOOMRPG_RES60_P27_METADATA_WINDOWS && defined(ARDUINO)
#include "esp_res60_profile.h"
#endif

#define MAPPINGS_HEADER_BYTES 16U
#define MAPPING_PAIR_BYTES 8U
#define PALETTES_HEADER_BYTES 4U
#define WALL_TEXEL_HEADER_BYTES 4U
#define PLANE_TEXTURE_BYTES 2048U
#define PLANE_PALETTE_COLORS 16U
#define MAX_PLANE_TEXTURES 24U
#define PLANE_HEAP_CACHE_SLOTS 6U
#if DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
#define PLANE_CACHE_SLOTS 9U
#else
#define PLANE_CACHE_SLOTS 6U
#endif
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
    EspNativePlaneRenderStats stats;
} PlaneWork;

static EspNativePlaneRenderStats planeStats;

#if DOOMRPG_RES60_P28_DESCRIPTOR_LEASE
/* Compact metadata only. All texel bytes remain transient. The exact-size
 * allocation is owned by the preview map lease, never Classic gameplay. */
typedef struct PlaneDescriptorLease_s {
    const uint8_t* planeMap;
    uint32_t arenaFNV, sourceCRC;
    EspAssetPackEntry mappings, palettes, wall;
    uint16_t count;
    PlaneTextureDesc desc[];
} PlaneDescriptorLease;
static PlaneDescriptorLease* descriptorLease;
static size_t descriptorBytes(uint16_t count) {
    return sizeof(PlaneDescriptorLease)+(size_t)count*sizeof(PlaneTextureDesc);
}
void EspNativePlaneRenderer_resetRes60Metadata(void) {
    if (descriptorLease != NULL) {
        DRPG_LOGI("[RES60P28] RELEASE bytes=%u\n",
                  (unsigned)descriptorBytes(descriptorLease->count));
        free(descriptorLease);
        descriptorLease=NULL;
    }
}
static int sameEntry(const EspAssetPackEntry* a,const EspAssetPackEntry* b) {
    return a->nameHash==b->nameHash && a->offset==b->offset &&
           a->size==b->size && a->crc32==b->crc32 && a->flags==b->flags;
}
static int restoreDescriptors(PlaneWork* work,const EspAssetPackEntry* mappings,
                              const EspAssetPackEntry* palettes) {
    const PlaneDescriptorLease* c=descriptorLease;
    if (c==NULL || c->planeMap!=work->runtime->planeMap ||
        c->arenaFNV!=work->runtime->arenaFNV1a || c->sourceCRC!=work->runtime->sourceCrc32 ||
        c->count!=work->textureCount || c->count>MAX_PLANE_TEXTURES ||
        !sameEntry(&c->mappings,mappings) || !sameEntry(&c->palettes,palettes) ||
        !sameEntry(&c->wall,&work->wallTexels)) return 0;
    for (uint16_t i=0;i<c->count;++i)
        if (c->desc[i].logicalId!=work->textures[i].logicalId) return 0;
    memcpy(work->textures,c->desc,(size_t)c->count*sizeof(PlaneTextureDesc));
    DRPG_LOGI("[RES60P28] META cache=HIT bytes=%u textures=%u\n",
              (unsigned)descriptorBytes(c->count),(unsigned)c->count);
    return 1;
}
static void retainDescriptors(const PlaneWork* work,const EspAssetPackEntry* mappings,
                              const EspAssetPackEntry* palettes) {
    PlaneDescriptorLease* c;
    const size_t bytes=descriptorBytes(work->textureCount);
    if (descriptorLease!=NULL) return;
    c=(PlaneDescriptorLease*)malloc(bytes);
    if (c==NULL) {
        DRPG_LOGI("[RES60P28] META cache=FALLBACK bytes=%u\n",(unsigned)bytes);
        return;
    }
    c->planeMap=work->runtime->planeMap;
    c->arenaFNV=work->runtime->arenaFNV1a;
    c->sourceCRC=work->runtime->sourceCrc32;
    c->mappings=*mappings;
    c->palettes=*palettes;
    c->wall=work->wallTexels;
    c->count=work->textureCount;
    memcpy(c->desc,work->textures,(size_t)c->count*sizeof(PlaneTextureDesc));
    descriptorLease=c;
    DRPG_LOGI("[RES60P28] META cache=MISS bytes=%u textures=%u\n",
              (unsigned)bytes,(unsigned)c->count);
}
#endif

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

#if DOOMRPG_RES60_P27_METADATA_WINDOWS
/* Three independent 512-byte entry-relative windows occupy 1536 bytes of
 * the FIRST EXISTING 2048-byte texel lease before it holds any texel.
 * No additional payload allocation; the LRU is still invalid at this point. */
typedef struct PlaneMetadataWindow_s {
    uint32_t base, length, reads, hits;
} PlaneMetadataWindow;
static int readMetadata(const EspAssetPackEntry* entry, uint32_t offset,
                        void* dst, size_t length, PlaneMetadataWindow* w,
                        uint8_t* scratch) {
    uint32_t base, available;
    if (offset > entry->size || length > entry->size - offset) return 0;
    if (scratch == NULL) return EspAssetPack_readRange(entry, offset, dst, length);
    if (w->length && offset >= w->base &&
        offset-w->base <= w->length && length <= w->length-(offset-w->base)) {
        memcpy(dst, scratch+offset-w->base, length);
        ++w->hits;
        return 1;
    }
    base = offset & ~511U;
    if (length > 512U-(offset-base)) base=offset;
    available=entry->size-base;
    if (available>512U) available=512U;
    w->length=0U;
    if (length>available-(offset-base) ||
        !EspAssetPack_readRange(entry,base,scratch,available)) return 0;
    ++w->reads;
    w->base=base;
    w->length=available;
    memcpy(dst,scratch+offset-base,length);
    return 1;
}
#endif

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
#if DOOMRPG_RES60_P27_METADATA_WINDOWS
    PlaneMetadataWindow idWindow={0}, pairWindow={0}, paletteWindow={0};
    uint8_t* scratch = work != NULL ? work->cache[0].texels : NULL;
#endif

    if (work == NULL || !EspAssetPack_isOpen() ||
        !EspAssetPack_findEntry("mappings.bin", &mappings) ||
        !EspAssetPack_findEntry("palettes.bin", &palettes) ||
        !EspAssetPack_findEntry("wtexels.bin", &work->wallTexels) ||
        mappings.size < MAPPINGS_HEADER_BYTES ||
        palettes.size < PALETTES_HEADER_BYTES ||
        work->wallTexels.size < WALL_TEXEL_HEADER_BYTES) return 0;
#if DOOMRPG_RES60_P28_DESCRIPTOR_LEASE
    if (scratch != NULL && descriptorLease != NULL) {
        /* P13 has already checked the immutable arena on warm frames.
         * Recheck all three PAK entry identities before using cached metadata. */
        if (!restoreDescriptors(work,&mappings,&palettes)) {
            DRPG_LOGE("[RES60P28] FAIL CLOSED stage=descriptor-identity\n");
            return 0;
        }
        return 1;
    }
#endif
    if (!EspAssetPack_readRange(&mappings, 0U, mappingHeader,
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
#if DOOMRPG_RES60_P27_METADATA_WINDOWS
            !readMetadata(&mappings, textureIdBase + (uint32_t)out->logicalId*2U,
                          idBytes,sizeof(idBytes),&idWindow,scratch)) return 0;
#else
            !EspAssetPack_readRange(&mappings,
                                    textureIdBase +
                                        (uint32_t)out->logicalId * 2U,
                                    idBytes, sizeof(idBytes))) return 0;
#endif

        actual = readLe16(idBytes);
        if (actual >= texelPairs || actual > UINT16_MAX ||
#if DOOMRPG_RES60_P27_METADATA_WINDOWS
            !readMetadata(&mappings,MAPPINGS_HEADER_BYTES+actual*MAPPING_PAIR_BYTES,
                          pair,sizeof(pair),&pairWindow,scratch ? scratch+512U : NULL)) return 0;
#else
            !EspAssetPack_readRange(&mappings,
                                    MAPPINGS_HEADER_BYTES +
                                        actual * MAPPING_PAIR_BYTES,
                                    pair, sizeof(pair))) return 0;
#endif

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

#if DOOMRPG_RES60_P27_METADATA_WINDOWS
        if (!readMetadata(&palettes,PALETTES_HEADER_BYTES+(uint32_t)paletteOffset*2U,
                          paletteRaw,sizeof(paletteRaw),&paletteWindow,
                          scratch ? scratch+1024U : NULL)) return 0;
#else
        if (!EspAssetPack_readRange(
                &palettes,
                PALETTES_HEADER_BYTES + (uint32_t)paletteOffset * 2U,
                paletteRaw, sizeof(paletteRaw))) return 0;
#endif

        out->actualId = (uint16_t)actual;
        out->sourceTexelOffset = (uint32_t)sourceOffset;
        for (p = 0U; p < PLANE_PALETTE_COLORS; ++p) {
            out->paletteRgb565[p] =
                sourceToFramebuffer565(readLe16(&paletteRaw[p * 2U]));
        }
    }
#if DOOMRPG_RES60_P27_METADATA_WINDOWS
    if (scratch != NULL) {
        DRPG_LOGI("[RES60P27] META reads=%u/%u/%u hits=%u/%u/%u scratch=existing-lease\n",
                  (unsigned)idWindow.reads,(unsigned)pairWindow.reads,(unsigned)paletteWindow.reads,
                  (unsigned)idWindow.hits,(unsigned)pairWindow.hits,(unsigned)paletteWindow.hits);
    }
#endif
#if DOOMRPG_RES60_P28_DESCRIPTOR_LEASE
    if (scratch != NULL) retainDescriptors(work,&mappings,&palettes);
#endif
    return 1;
}

/* Up to six independent 2048-byte leases preserve the hardware-proven LRU
 * without requiring one contiguous 12288-byte heap block. Six slots are a
 * performance target, not a rendering invariant: under later gameplay owners
 * (for example a dialog topology rollback snapshot), keep every lease already
 * obtained and degrade the cache width instead of failing the whole frame.
 * One slot is sufficient for exact sampling; fewer slots only increase PAK
 * reads/evictions. */
static void releaseCache(PlaneWork* work) {
    uint32_t i;
    if (work == NULL) return;
    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
#if DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
        /* Slots 6..8 are borrowed, never freed by this owner. */
        if (i < PLANE_HEAP_CACHE_SLOTS)
#endif
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
    for (i = 0U; i < PLANE_HEAP_CACHE_SLOTS; ++i) {
        work->cache[i].texels = (uint8_t*)malloc(PLANE_TEXTURE_BYTES);
        if (work->cache[i].texels == NULL) {
            if (i == 0U) {
                DRPG_LOGE("[NATIVEPLANE] CACHE-FAILED slots=0/%u leaseBytes=%u\n",
                       (unsigned int)PLANE_CACHE_SLOTS,
                       (unsigned int)PLANE_TEXTURE_BYTES);
                return 0;
            }
            DRPG_LOGI("[NATIVEPLANE] CACHE-FALLBACK slots=%u/%u leaseBytes=%u totalLeaseBytes=%u\n",
                   (unsigned int)i,
                   (unsigned int)PLANE_CACHE_SLOTS,
                   (unsigned int)PLANE_TEXTURE_BYTES,
                   (unsigned int)(i * PLANE_TEXTURE_BYTES));
            break;
        }
    }
    return work->cache[0].texels != NULL;
}

static int acquireTexture(PlaneWork* work,
                          const PlaneTextureDesc* source,
                          const uint8_t** outTexels) {
    PlaneCacheSlot* target = NULL;
    uint32_t oldest = UINT32_MAX;
    uint32_t i;
    uint32_t readOffset;

    if (outTexels != NULL) *outTexels = NULL;
    if (work == NULL || source == NULL || outTexels == NULL) return 0;

    ++work->cacheClock;
    if (work->cacheClock == 0U) work->cacheClock = 1U;

    for (i = 0U; i < PLANE_CACHE_SLOTS; ++i) {
        PlaneCacheSlot* slot = &work->cache[i];
        if (slot->texels == NULL) break;
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
        if (slot->texels == NULL) break;
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
        PLANE_TEXTURE_BYTES > work->wallTexels.size - readOffset ||
        !EspAssetPack_readRange(&work->wallTexels, readOffset,
                                target->texels, PLANE_TEXTURE_BYTES)) return 0;

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

static int renderPlanes(struct Render_s* renderBase, int res60, void* scratch, size_t scratchBytes) {
#if !DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
    (void)scratch;
    (void)scratchBytes;
#endif
    Render_t* render = (Render_t*)renderBase;
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    PlaneWork* work;
    int y;
    int ok = 0;

    EspNativePlaneRenderer_reset();
    if (render == NULL || runtime == NULL ||
        !EspMapRuntime_isLoaded() || !EspAssetPack_isOpen() ||
        render->pixels == NULL || render->framebuffer == NULL ||
        (!res60 && (render->screenWidth != 160 || render->screenHeight != 80)) ||
        (res60 && (!DOOMRPG_RES60_P25_TEXTURED_PLANES ||
                   render->screenWidth != 192 || render->screenHeight != 96)) ||
        render->screenLeft != 0 || render->screenTop != 0 ||
        render->screenRight != render->screenWidth ||
        render->screenBottom != render->screenHeight ||
        runtime->planeMap == NULL ||
        runtime->planeMapBytes != EXPECTED_PLANE_MAP_BYTES) {
        return 0;
    }

    /* PlaneWork is roughly a KiB because it carries the bounded descriptor
     * table, six LRU slot descriptors and render stats. Keeping that object on
     * the already-deep loopTask renderer stack crossed the real CYD canary on
     * valid Entrance views. Lease it only for this render call: no permanent
     * owner/BSS growth, and the existing six independent 2048-byte texel leases
     * retain their proven allocation policy. */
    work = (PlaneWork*)malloc(sizeof(*work));
    if (work == NULL) {
        DRPG_LOGE("[NATIVEPLANE] FAILED transient workspace allocation bytes=%u\n",
               (unsigned int)sizeof(*work));
        return 0;
    }
    memset(work, 0, sizeof(*work));
    work->render = render;
    work->runtime = runtime;

#if DOOMRPG_RES60_P27_METADATA_WINDOWS
    if (res60) {
        if (!collectTextures(work) || !initCache(work)) goto done;
#if defined(ARDUINO)
        const uint32_t metaAt=EspRes60Profile_nowUs();
#endif
        if (!resolveTextures(work)) goto done;
#if DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
        /* P27 metadata used slot zero only; the P17 loan is independent.
         * Append only to a full six-slot heap cache: preserve contiguous
         * valid slot scan and existing allocation-pressure fallback. */
        if (scratch != NULL && scratchBytes == 3U*PLANE_TEXTURE_BYTES &&
            work->cache[PLANE_HEAP_CACHE_SLOTS-1U].texels != NULL) {
            for (uint32_t i=PLANE_HEAP_CACHE_SLOTS;i<PLANE_CACHE_SLOTS;++i)
                work->cache[i].texels=(uint8_t*)scratch+(i-PLANE_HEAP_CACHE_SLOTS)*PLANE_TEXTURE_BYTES;
            DRPG_LOGI("[RES60P29] CACHE heapSlots=6 borrowedSlots=3 bytes=6144\n");
        } else {
            DRPG_LOGI("[RES60P29] CACHE borrowedSlots=0 fallback=1\n");
        }
#endif
#if defined(ARDUINO)
        DRPG_LOGI("[RES60P27] RESOLVE us=%lu\n",
                  (unsigned long)(EspRes60Profile_nowUs()-metaAt));
#endif
    } else
#endif
    if (!collectTextures(work) || !resolveTextures(work) || !initCache(work)) {
        goto done;
    }

    work->stats.uniqueLogicalTextures = work->textureCount;
    for (y = 0; y < EspNativeRenderGeometry_halfScreenHeight(render->screenHeight); ++y) {
        if (!drawPlaneRow(work, y,
                          work->runtime->planeMap + ESP_MAP_PLANE_CELL_COUNT)) {
            goto done;
        }
        ++work->stats.rowsRendered;
    }
    for (; y < render->screenHeight; ++y) {
        if (!drawPlaneRow(work, y, work->runtime->planeMap)) goto done;
        ++work->stats.rowsRendered;
    }

    if (work->stats.rowsRendered != (uint32_t)render->screenHeight ||
        work->stats.pixelsRendered !=
            (uint32_t)render->screenWidth * (uint32_t)render->screenHeight) {
        goto done;
    }

    work->stats.rendered = 1U;
    work->stats.active = 1U;
    ok = 1;

done:
    releaseCache(work);
    if (ok) planeStats = work->stats;
    free(work);

    if (ok) {
        DRPG_LOGT("[NATIVEPLANE] rows=%u pixels=%u textures=%u cache=%uH/%uM/%uE reads=%uB\n",
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
        DRPG_LOGE("[NATIVEPLANE] FAILED textured floor/ceiling reconstruction\n");
    }
    return ok;
}
 
int EspNativePlaneRenderer_render(struct Render_s* renderBase) {
    return renderPlanes(renderBase, 0, NULL, 0U);
}

#if DOOMRPG_RES60_P25_TEXTURED_PLANES
static int renderRes60Core(uint16_t* framebuffer, size_t bytes,
                                      int x, int y, int angle, int viewZ,
                                      void* scratch, size_t scratchBytes) {
    EspNativeRasterWorldViewport viewport;
    Render_t render;
    EspNativePlaneRenderer_reset();
    if (framebuffer == NULL || x < 0 || x >= 2048 ||
        y < 0 || y >= 2048 || viewZ < 0 || viewZ > 64 ||
        (angle != 0 && angle != 64 && angle != 128 && angle != 192) ||
        !EspNativeRasterContract_prepareWorld(
            ESP_NATIVE_RASTER_RES60, bytes, 384, 192, 96,
            ESP_NATIVE_RASTER_COLUMN_CAPACITY, &viewport)) return 0;
    memset(&render, 0, sizeof(render));
    render.framebuffer = (uint8_t*)framebuffer;
    render.pixels = (short*)(framebuffer + viewport.worldFirstPixel);
    render.pitch = (int)viewport.raster.pitchPixels * 2;
    render.screenWidth = render.screenRight = viewport.raster.worldWidth;
    render.screenHeight = render.screenBottom = viewport.raster.worldHeight;
    render.viewX = x;
    render.viewY = y;
    render.viewZ = viewZ;
    render.viewCos = angle == 0 ? 65536 : angle == 128 ? -65536 : 0;
    render.viewSin = angle == 64 ? 65536 : angle == 192 ? -65536 : 0;
#if DOOMRPG_RES60_P28_DESCRIPTOR_LEASE
    const int ok=renderPlanes(&render, 1, scratch, scratchBytes);
    if (!ok) EspNativePlaneRenderer_resetRes60Metadata();
    return ok;
#else
    return renderPlanes(&render, 1, scratch, scratchBytes);
#endif
}
int EspNativePlaneRenderer_renderRes60(uint16_t* framebuffer, size_t bytes,
                                      int x, int y, int angle, int viewZ) {
    return renderRes60Core(framebuffer,bytes,x,y,angle,viewZ,NULL,0U);
}
#if DOOMRPG_RES60_P29_BORROW_PLANE_SCRATCH
int EspNativePlaneRenderer_renderRes60WithScratch(uint16_t* framebuffer, size_t bytes,
                      int x, int y, int angle, int viewZ, void* scratch, size_t scratchBytes) {
    /* Reject a bad or overlapping loan without touching either buffer. */
    const uintptr_t f=(uintptr_t)framebuffer, s=(uintptr_t)scratch;
    if (framebuffer==NULL || scratch==NULL || scratchBytes!=6144U ||
        bytes!=55296U || f>UINTPTR_MAX-bytes || s>UINTPTR_MAX-scratchBytes ||
        (f<s+scratchBytes && s<f+bytes)) {
        EspNativePlaneRenderer_reset();
        return 0;
    }
    return renderRes60Core(framebuffer,bytes,x,y,angle,viewZ,scratch,scratchBytes);
}
#endif
#endif
