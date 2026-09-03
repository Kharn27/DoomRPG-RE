#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_native_first_frame.h"
#include "esp_native_graphics_catalog.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define MAPPINGS_HEADER_BYTES 16U
#define MAPPING_PAIR_BYTES 8U
#define PALETTES_HEADER_BYTES 4U
#define BSP_HEADER_BYTES 33U
#define WALL_TEXEL_HEADER_BYTES 4U
#define WALL_PACKED_BYTES 2048U
#define WALL_WIDTH 64
#define WALL_HEIGHT 64
#define MAX_RESOLVED_TEXTURES 64U
#define WALL_CACHE_SLOTS 3U
#define MAX_BSP_DEPTH 64U
#define MAX_SCREEN_WIDTH 160U
#define FIRST_FRAME_ANIMATION_TIME 0U

#define LINE_FLAG_TWO_SIDED 0x00000001UL
#define LINE_FLAG_SPRITE_SPAN 0x00000002UL
#define LINE_FLAG_AXIS_X 0x00000008UL
#define LINE_FLAG_AXIS_NEGATIVE 0x00000010UL
#define LINE_FLAG_Y_NUDGE 0x00000100UL
#define LINE_FLAG_X_NUDGE 0x00000200UL
#define LINE_FLAG_REVERSE_TEX 0x00008000UL
#define LINE_FLAG_OCCLUDER_ONLY 0x20000000UL

#define COLUMN_SCALE_INIT_LOCAL MAXINT

typedef char EspNativeFirstFrameState_must_be_48_bytes[
    sizeof(EspNativeFirstFrameState) == 48U ? 1 : -1];

typedef struct ResolvedWallTexture_s {
    uint16_t logicalId;
    uint16_t actualId;
    uint16_t paletteSourceOffset;
    uint16_t reserved;
    uint32_t sourceTexelOffset;
    uint16_t paletteRgb565[ESP_NATIVE_GRAPHICS_PALETTE_COLORS];
} ResolvedWallTexture;

typedef struct WallCacheSlot_s {
    const ResolvedWallTexture* source;
    uint8_t* texels;
    uint32_t lastUse;
    int valid;
} WallCacheSlot;

typedef struct FirstFrameWork_s {
    Render_t* render;
    const EspMapRuntimeView* runtime;
    ResolvedWallTexture resolved[MAX_RESOLVED_TEXTURES];
    uint16_t resolvedCount;
    WallCacheSlot cache[WALL_CACHE_SLOTS];
    uint32_t cacheClock;
    EspAssetPackEntry wallTexels;
    uint32_t wallTexelDataBytes;

    uint32_t lineCandidates;
    uint32_t leafNodes;
    uint32_t wallRequests;
    uint32_t wallDraws;
    uint32_t spanCalls;
    uint32_t pixelsDrawn;
    uint32_t cacheHits;
    uint32_t cacheMisses;
    uint32_t cacheEvictions;
    uint32_t backfaceCulled;
    uint32_t clipCulled;
    uint32_t occluderOnly;
    uint32_t spriteSpanSkipped;
    uint32_t nodeCulled;
} FirstFrameWork;

typedef struct RenderScratch_s {
    int viewCos_;
    int viewSin_;
    int viewTransX;
    int viewSin;
    int viewCos;
    int viewTransY;
    int viewX;
    int viewY;
    int viewZ;
    int viewAngle;
    int lineCount;
    int lineRasterCount;
    int nodeCount;
    int nodeRasterCount;
    int spriteCount;
    int spriteRasterCount;
    int screenLeft;
    int screenTop;
    int screenRight;
    int screenBottom;
    int numLines;
    byte spanMode;
    short* pixels;
    int columnScale[MAX_SCREEN_WIDTH];
} RenderScratch;

typedef enum FirstFrameFailureCode_e {
    FIRST_FRAME_FAIL_NONE = 0,
    FIRST_FRAME_FAIL_SPAN_SCREEN = 1,
    FIRST_FRAME_FAIL_SPAN_NEGATIVE = 2,
    FIRST_FRAME_FAIL_SPAN_OOB = 3,
    FIRST_FRAME_FAIL_WALL_DEPTH = 4,
    FIRST_FRAME_FAIL_LINE_FETCH = 5,
    FIRST_FRAME_FAIL_TEXTURE_MISSING = 6,
    FIRST_FRAME_FAIL_WALL_LOAD = 7,
    FIRST_FRAME_FAIL_WALL_RASTER = 8,
    FIRST_FRAME_FAIL_WALK = 9
} FirstFrameFailureCode;

typedef struct FirstFrameFailure_s {
    uint32_t code;
    uint32_t lineIndex;
    uint32_t logicalId;
    uint32_t actualId;
    uint32_t flags;
    uint32_t sourceOffset;
    int32_t value0;
    int32_t value1;
    int32_t value2;
    int32_t value3;
} FirstFrameFailure;

typedef struct LegacyWallGuard_s {
    uint16_t logicalId;
    uint16_t actualId;
    uint16_t successorActualId;
    uint8_t packedByte;
    uint8_t valid;
    uint32_t sourceOffset;
    uint32_t successorSourceOffset;
} LegacyWallGuard;

#define RECORD_FRAME_FAILURE(code_, line_, logical_, actual_, flags_, source_, v0_, v1_, v2_, v3_) \
    do { \
        if (frameFailure.code == FIRST_FRAME_FAIL_NONE) { \
            frameFailure.code = (uint32_t)(code_); \
            frameFailure.lineIndex = (uint32_t)(line_); \
            frameFailure.logicalId = (uint32_t)(logical_); \
            frameFailure.actualId = (uint32_t)(actual_); \
            frameFailure.flags = (uint32_t)(flags_); \
            frameFailure.sourceOffset = (uint32_t)(source_); \
            frameFailure.value0 = (int32_t)(v0_); \
            frameFailure.value1 = (int32_t)(v1_); \
            frameFailure.value2 = (int32_t)(v2_); \
            frameFailure.value3 = (int32_t)(v3_); \
        } \
    } while (0)

static EspNativeFirstFrameState frameState;
/* renderFrame() is reachable below gameplay/combat. Its large bounded work and
 * render-save scratch must not live on loopTask stack. Single-threaded native
 * rendering permits one non-reentrant BSS workspace. */
static FirstFrameWork frameWorkScratch;
static RenderScratch frameRenderScratch;
static uint8_t frameRenderBusy;
static uint8_t frameScratchLogged;
/* Failure-only BSS witness, printed after renderer work has unwound. */
static FirstFrameFailure frameFailure;
/* One exact packed byte from the next legacy compact wall block. It is resolved
 * only after an OOB witness has unwound the renderer stack, then reused by the
 * bounded sampler on the retry. */
static LegacyWallGuard legacyWallGuard;

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t fnv1a32(const void* data, uint32_t length) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((((uint16_t)r >> 3) << 11) |
                      (((uint16_t)g >> 2) << 5) |
                      ((uint16_t)b >> 3));
}

static uint16_t sourceBgr565ToRgb565(uint16_t color) {
    return (uint16_t)(((color & 0x001fU) << 11) |
                      (color & 0x07e0U) |
                      ((color & 0xf800U) >> 11));
}

static uint32_t framebufferFNV(const Render_t* render) {
    const uint32_t expectedBytes =
        DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT * (uint32_t)sizeof(uint16_t);
    if (render == NULL || render->framebuffer == NULL ||
        Esp32PlatformVideo_framebuffer() != render->framebuffer ||
        Esp32PlatformVideo_framebufferSizeBytes() != expectedBytes) {
        return 0U;
    }
    return fnv1a32(render->framebuffer, expectedBytes);
}

static const char* frameFailureName(uint32_t code) {
    switch ((FirstFrameFailureCode)code) {
    case FIRST_FRAME_FAIL_SPAN_SCREEN: return "SPAN_SCREEN";
    case FIRST_FRAME_FAIL_SPAN_NEGATIVE: return "SPAN_NEGATIVE";
    case FIRST_FRAME_FAIL_SPAN_OOB: return "SPAN_OOB";
    case FIRST_FRAME_FAIL_WALL_DEPTH: return "WALL_DEPTH";
    case FIRST_FRAME_FAIL_LINE_FETCH: return "LINE_FETCH";
    case FIRST_FRAME_FAIL_TEXTURE_MISSING: return "TEXTURE_MISSING";
    case FIRST_FRAME_FAIL_WALL_LOAD: return "WALL_LOAD";
    case FIRST_FRAME_FAIL_WALL_RASTER: return "WALL_RASTER";
    case FIRST_FRAME_FAIL_WALK: return "WALK";
    default: return "NONE";
    }
}

static void printFrameFailure(const char* route,
                              const EspPlayerViewState* playerView) {
    if (frameFailure.code == FIRST_FRAME_FAIL_NONE) return;
    printf("[NATIVEFRAME] FAILED route=%s code=%u/%s view=%d,%d,%d angle=%d line=%u logical=%u actual=%u flags=%08x source=%u v=%d,%d,%d,%d\n",
           route != NULL ? route : "?",
           (unsigned int)frameFailure.code,
           frameFailureName(frameFailure.code),
           playerView != NULL ? (int)playerView->viewX : -1,
           playerView != NULL ? (int)playerView->viewY : -1,
           playerView != NULL ? (int)playerView->viewZ : -1,
           playerView != NULL ? (int)playerView->viewAngle : -1,
           (unsigned int)frameFailure.lineIndex,
           (unsigned int)frameFailure.logicalId,
           (unsigned int)frameFailure.actualId,
           (unsigned int)frameFailure.flags,
           (unsigned int)frameFailure.sourceOffset,
           (int)frameFailure.value0,
           (int)frameFailure.value1,
           (int)frameFailure.value2,
           (int)frameFailure.value3);
}

static int prepareLegacyWallGuard(void) {
    const EspNativeGraphicsCatalogView* catalog;
    EspAssetPackEntry mappings;
    EspAssetPackEntry wallTexels;
    uint8_t mappingHeader[MAPPINGS_HEADER_BYTES];
    uint8_t idBytes[2];
    uint8_t pair[MAPPING_PAIR_BYTES];
    uint8_t packedByte = 0U;
    uint32_t texelPairs;
    uint32_t bitShapePairs;
    uint32_t textureIdCount;
    uint32_t spriteIdCount;
    uint32_t textureIdBase;
    uint32_t nextSource = UINT32_MAX;
    uint32_t nextActual = UINT32_MAX;
    uint64_t expectedMappingsBytes;
    uint16_t i;
    int currentPresent = 0;
    int ok = 0;

    memset(&legacyWallGuard, 0, sizeof(legacyWallGuard));
    if (frameFailure.code != FIRST_FRAME_FAIL_SPAN_OOB ||
        frameFailure.value0 != (int32_t)WALL_PACKED_BYTES ||
        (frameFailure.sourceOffset & 1U) != 0U ||
        EspAssetPack_isOpen() || !EspNativeGraphicsCatalog_isReady()) {
        return 0;
    }

    catalog = EspNativeGraphicsCatalog_view();
    if (catalog == NULL || catalog->textureCount == 0U) return 0;
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;

    if (!EspAssetPack_findEntry("mappings.bin", &mappings) ||
        !EspAssetPack_findEntry("wtexels.bin", &wallTexels) ||
        !EspAssetPack_readRange(&mappings, 0U, mappingHeader,
                                sizeof(mappingHeader))) {
        goto done;
    }

    texelPairs = readLe32(mappingHeader);
    bitShapePairs = readLe32(mappingHeader + 4U);
    textureIdCount = readLe32(mappingHeader + 8U);
    spriteIdCount = readLe32(mappingHeader + 12U);
    expectedMappingsBytes =
        (uint64_t)MAPPINGS_HEADER_BYTES +
        ((uint64_t)texelPairs * MAPPING_PAIR_BYTES) +
        ((uint64_t)bitShapePairs * MAPPING_PAIR_BYTES) +
        ((uint64_t)textureIdCount * 2U) +
        ((uint64_t)spriteIdCount * 2U);
    if (texelPairs == 0U || bitShapePairs == 0U || textureIdCount == 0U ||
        texelPairs > 4096U || bitShapePairs > 4096U ||
        textureIdCount > 4096U || spriteIdCount > 4096U ||
        expectedMappingsBytes != mappings.size) {
        goto done;
    }

    textureIdBase = MAPPINGS_HEADER_BYTES +
                    texelPairs * MAPPING_PAIR_BYTES +
                    bitShapePairs * MAPPING_PAIR_BYTES;

    for (i = 0U; i < catalog->textureCount; ++i) {
        uint32_t logical = catalog->textures[i].resourceId;
        uint32_t lookupLogical = logical;
        uint32_t baseActual;
        uint32_t phase;

        if (lookupLogical >= textureIdCount) lookupLogical = textureIdCount - 1U;
        if (!EspAssetPack_readRange(&mappings,
                                    textureIdBase + lookupLogical * 2U,
                                    idBytes,
                                    sizeof(idBytes))) {
            goto done;
        }
        baseActual = readLe16(idBytes);

        for (phase = 0U; phase < 4U; ++phase) {
            uint32_t actual = baseActual + phase;
            int32_t sourceOffset;
            uint32_t packedOffset;

            if (actual >= texelPairs) goto done;
            if (!EspAssetPack_readRange(
                    &mappings,
                    MAPPINGS_HEADER_BYTES + actual * MAPPING_PAIR_BYTES,
                    pair,
                    sizeof(pair))) {
                goto done;
            }
            sourceOffset = (int32_t)readLe32(pair);
            if (sourceOffset < 0 || (sourceOffset & 1) != 0) goto done;
            packedOffset = WALL_TEXEL_HEADER_BYTES +
                           (uint32_t)sourceOffset / 2U;
            if (packedOffset > wallTexels.size ||
                WALL_PACKED_BYTES > wallTexels.size - packedOffset) {
                goto done;
            }

            if (actual == frameFailure.actualId &&
                (uint32_t)sourceOffset == frameFailure.sourceOffset) {
                currentPresent = 1;
            }
            if ((uint32_t)sourceOffset > frameFailure.sourceOffset &&
                (uint32_t)sourceOffset < nextSource) {
                nextSource = (uint32_t)sourceOffset;
                nextActual = actual;
            }
        }
    }

    if (!currentPresent || nextSource == UINT32_MAX || nextActual > UINT16_MAX) {
        goto done;
    }
    if (!EspAssetPack_readRange(
            &wallTexels,
            WALL_TEXEL_HEADER_BYTES + nextSource / 2U,
            &packedByte,
            1U)) {
        goto done;
    }

    legacyWallGuard.logicalId = (uint16_t)frameFailure.logicalId;
    legacyWallGuard.actualId = (uint16_t)frameFailure.actualId;
    legacyWallGuard.successorActualId = (uint16_t)nextActual;
    legacyWallGuard.packedByte = packedByte;
    legacyWallGuard.valid = 1U;
    legacyWallGuard.sourceOffset = frameFailure.sourceOffset;
    legacyWallGuard.successorSourceOffset = nextSource;
    printf("[NATIVEFRAME] LEGACY_GUARD logical=%u actual=%u source=%u successorActual=%u successorSource=%u byte=%02x owner=BSS bytes=%u\n",
           (unsigned int)legacyWallGuard.logicalId,
           (unsigned int)legacyWallGuard.actualId,
           (unsigned int)legacyWallGuard.sourceOffset,
           (unsigned int)legacyWallGuard.successorActualId,
           (unsigned int)legacyWallGuard.successorSourceOffset,
           (unsigned int)legacyWallGuard.packedByte,
           (unsigned int)sizeof(legacyWallGuard));
    ok = 1;

done:
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    if (!ok) memset(&legacyWallGuard, 0, sizeof(legacyWallGuard));
    return ok;
}

static void saveRenderScratch(Render_t* render, RenderScratch* scratch) {
    if (render == NULL || scratch == NULL) return;
    memset(scratch, 0, sizeof(*scratch));
    scratch->viewCos_ = render->viewCos_;
    scratch->viewSin_ = render->viewSin_;
    scratch->viewTransX = render->viewTransX;
    scratch->viewSin = render->viewSin;
    scratch->viewCos = render->viewCos;
    scratch->viewTransY = render->viewTransY;
    scratch->viewX = render->viewX;
    scratch->viewY = render->viewY;
    scratch->viewZ = render->viewZ;
    scratch->viewAngle = render->viewAngle;
    scratch->lineCount = render->lineCount;
    scratch->lineRasterCount = render->lineRasterCount;
    scratch->nodeCount = render->nodeCount;
    scratch->nodeRasterCount = render->nodeRasterCount;
    scratch->spriteCount = render->spriteCount;
    scratch->spriteRasterCount = render->spriteRasterCount;
    scratch->screenLeft = render->screenLeft;
    scratch->screenTop = render->screenTop;
    scratch->screenRight = render->screenRight;
    scratch->screenBottom = render->screenBottom;
    scratch->numLines = render->numLines;
    scratch->spanMode = render->spanMode;
    scratch->pixels = render->pixels;
    if (render->columnScale != NULL && render->screenWidth <= (int)MAX_SCREEN_WIDTH) {
        memcpy(scratch->columnScale,
               render->columnScale,
               (size_t)render->screenWidth * sizeof(int));
    }
}

static void restoreRenderScratch(Render_t* render, const RenderScratch* scratch) {
    if (render == NULL || scratch == NULL) return;
    render->viewCos_ = scratch->viewCos_;
    render->viewSin_ = scratch->viewSin_;
    render->viewTransX = scratch->viewTransX;
    render->viewSin = scratch->viewSin;
    render->viewCos = scratch->viewCos;
    render->viewTransY = scratch->viewTransY;
    render->viewX = scratch->viewX;
    render->viewY = scratch->viewY;
    render->viewZ = scratch->viewZ;
    render->viewAngle = scratch->viewAngle;
    render->lineCount = scratch->lineCount;
    render->lineRasterCount = scratch->lineRasterCount;
    render->nodeCount = scratch->nodeCount;
    render->nodeRasterCount = scratch->nodeRasterCount;
    render->spriteCount = scratch->spriteCount;
    render->spriteRasterCount = scratch->spriteRasterCount;
    render->screenLeft = scratch->screenLeft;
    render->screenTop = scratch->screenTop;
    render->screenRight = scratch->screenRight;
    render->screenBottom = scratch->screenBottom;
    render->numLines = scratch->numLines;
    render->spanMode = scratch->spanMode;
    render->pixels = scratch->pixels;
    if (render->columnScale != NULL && render->screenWidth <= (int)MAX_SCREEN_WIDTH) {
        memcpy(render->columnScale,
               scratch->columnScale,
               (size_t)render->screenWidth * sizeof(int));
    }
}

static int legacyGraphicsRuntimeClear(const Render_t* render) {
    return render != NULL &&
           render->lines == NULL && render->nodes == NULL &&
           render->mapSprites == NULL &&
           render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL &&
           render->mediaSpriteIds == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL;
}

static const ResolvedWallTexture* findResolved(const FirstFrameWork* work,
                                                uint16_t logicalId) {
    uint16_t i;
    if (work == NULL) return NULL;
    for (i = 0U; i < work->resolvedCount; ++i) {
        if (work->resolved[i].logicalId == logicalId) return &work->resolved[i];
    }
    return NULL;
}

static int buildResolvedTextures(FirstFrameWork* work,
                                 const EspAssetPackEntry* mappings,
                                 const EspAssetPackEntry* palettes) {
    const EspNativeGraphicsCatalogView* catalog;
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
    uint16_t i;

    if (work == NULL || mappings == NULL || palettes == NULL ||
        !EspNativeGraphicsCatalog_isReady()) return 0;

    catalog = EspNativeGraphicsCatalog_view();
    if (catalog == NULL || catalog->textureCount == 0U ||
        catalog->textureCount > MAX_RESOLVED_TEXTURES ||
        !EspAssetPack_readRange(mappings, 0U, mappingHeader, sizeof(mappingHeader)) ||
        !EspAssetPack_readRange(palettes, 0U, paletteHeader, sizeof(paletteHeader))) {
        return 0;
    }

    texelPairs = readLe32(mappingHeader);
    bitShapePairs = readLe32(mappingHeader + 4U);
    textureIdCount = readLe32(mappingHeader + 8U);
    spriteIdCount = readLe32(mappingHeader + 12U);
    paletteBytes = readLe32(paletteHeader);

    expectedMappingsBytes =
        (uint64_t)MAPPINGS_HEADER_BYTES +
        ((uint64_t)texelPairs * MAPPING_PAIR_BYTES) +
        ((uint64_t)bitShapePairs * MAPPING_PAIR_BYTES) +
        ((uint64_t)textureIdCount * 2U) +
        ((uint64_t)spriteIdCount * 2U);

    if (texelPairs == 0U || bitShapePairs == 0U || textureIdCount == 0U ||
        texelPairs > 4096U || bitShapePairs > 4096U ||
        textureIdCount > 4096U || spriteIdCount > 4096U ||
        expectedMappingsBytes != mappings->size ||
        (paletteBytes & 1U) != 0U ||
        paletteBytes + PALETTES_HEADER_BYTES != palettes->size) {
        return 0;
    }

    paletteEntries = paletteBytes / 2U;
    textureIdBase = MAPPINGS_HEADER_BYTES +
                    texelPairs * MAPPING_PAIR_BYTES +
                    bitShapePairs * MAPPING_PAIR_BYTES;

    memset(work->resolved, 0, sizeof(work->resolved));
    work->resolvedCount = catalog->textureCount;

    for (i = 0U; i < catalog->textureCount; ++i) {
        const EspNativeGraphicsCatalogRecord* logicalRecord = &catalog->textures[i];
        ResolvedWallTexture* out = &work->resolved[i];
        uint8_t idBytes[2];
        uint8_t pair[MAPPING_PAIR_BYTES];
        uint8_t paletteRaw[ESP_NATIVE_GRAPHICS_PALETTE_COLORS * 2U];
        uint32_t logical = logicalRecord->resourceId;
        uint32_t lookupLogical = logical;
        uint32_t phase;
        uint32_t actual;
        int32_t sourceOffset;
        int32_t paletteOffset;
        uint32_t p;

        if (lookupLogical >= textureIdCount) lookupLogical = textureIdCount - 1U;
        if (!EspAssetPack_readRange(mappings,
                                    textureIdBase + lookupLogical * 2U,
                                    idBytes,
                                    sizeof(idBytes))) {
            return 0;
        }

        phase = (uint32_t)((((uint64_t)(FIRST_FRAME_ANIMATION_TIME + logical * 3U)) *
                            0x400000ULL) >> 30);
        actual = (uint32_t)readLe16(idBytes) + phase;
        if (actual >= texelPairs || actual > UINT16_MAX) return 0;

        if (!EspAssetPack_readRange(mappings,
                                    MAPPINGS_HEADER_BYTES + actual * MAPPING_PAIR_BYTES,
                                    pair,
                                    sizeof(pair))) {
            return 0;
        }

        sourceOffset = (int32_t)readLe32(pair);
        paletteOffset = (int32_t)readLe32(pair + 4U);
        if (sourceOffset < 0 || (sourceOffset & 1) != 0 ||
            paletteOffset < 0 ||
            (uint32_t)paletteOffset > paletteEntries ||
            ESP_NATIVE_GRAPHICS_PALETTE_COLORS >
                paletteEntries - (uint32_t)paletteOffset) {
            return 0;
        }

        if (!EspAssetPack_readRange(
                palettes,
                PALETTES_HEADER_BYTES + (uint32_t)paletteOffset * 2U,
                paletteRaw,
                sizeof(paletteRaw))) {
            return 0;
        }

        out->logicalId = (uint16_t)logical;
        out->actualId = (uint16_t)actual;
        out->paletteSourceOffset = (uint16_t)paletteOffset;
        out->sourceTexelOffset = (uint32_t)sourceOffset;
        for (p = 0U; p < ESP_NATIVE_GRAPHICS_PALETTE_COLORS; ++p) {
            out->paletteRgb565[p] =
                sourceBgr565ToRgb565(readLe16(&paletteRaw[p * 2U]));
        }
    }

    return 1;
}

static void releaseCache(FirstFrameWork* work) {
    uint32_t i;
    if (work == NULL) return;
    for (i = 0U; i < WALL_CACHE_SLOTS; ++i) {
        free(work->cache[i].texels);
        memset(&work->cache[i], 0, sizeof(work->cache[i]));
    }
}

static int loadWallTexels(FirstFrameWork* work,
                          const ResolvedWallTexture* source,
                          uint8_t* destination) {
    uint32_t readOffset;
    if (work == NULL || source == NULL || destination == NULL ||
        (source->sourceTexelOffset & 1U) != 0U) return 0;

    readOffset = WALL_TEXEL_HEADER_BYTES + source->sourceTexelOffset / 2U;
    if (readOffset > work->wallTexels.size ||
        WALL_PACKED_BYTES > work->wallTexels.size - readOffset) return 0;

    return EspAssetPack_readRange(&work->wallTexels,
                                  readOffset,
                                  destination,
                                  WALL_PACKED_BYTES);
}

static int acquireWall(FirstFrameWork* work,
                       const ResolvedWallTexture* source,
                       const uint8_t** outTexels) {
    WallCacheSlot* target = NULL;
    uint32_t oldest = UINT32_MAX;
    uint32_t i;

    if (outTexels != NULL) *outTexels = NULL;
    if (work == NULL || source == NULL || outTexels == NULL) return 0;

    ++work->cacheClock;
    if (work->cacheClock == 0U) work->cacheClock = 1U;

    for (i = 0U; i < WALL_CACHE_SLOTS; ++i) {
        WallCacheSlot* slot = &work->cache[i];
        if (slot->valid && slot->source != NULL &&
            slot->source->actualId == source->actualId &&
            slot->source->sourceTexelOffset == source->sourceTexelOffset) {
            slot->lastUse = work->cacheClock;
            ++work->cacheHits;
            *outTexels = slot->texels;
            return 1;
        }
    }

    ++work->cacheMisses;
    for (i = 0U; i < WALL_CACHE_SLOTS; ++i) {
        WallCacheSlot* slot = &work->cache[i];
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

    if (target->valid) {
        free(target->texels);
        memset(target, 0, sizeof(*target));
        ++work->cacheEvictions;
    }

    target->texels = (uint8_t*)malloc(WALL_PACKED_BYTES);
    if (target->texels == NULL) return 0;
    if (!loadWallTexels(work, source, target->texels)) {
        free(target->texels);
        memset(target, 0, sizeof(*target));
        return 0;
    }

    target->source = source;
    target->lastUse = work->cacheClock;
    target->valid = 1;
    *outTexels = target->texels;
    return 1;
}

static int sampleWallSpan(FirstFrameWork* work,
                          const ResolvedWallTexture* source,
                          const uint8_t* texels,
                          int x,
                          int y,
                          int texelPosition,
                          int texelStep,
                          int pixelCount) {
    Render_t* render;
    uint16_t* pixels;
    int pitch;
    int64_t localPosition;
    int remaining;

    if (work == NULL || source == NULL || texels == NULL ||
        work->render == NULL) return 0;

    render = work->render;
    ++work->spanCalls;
    if (pixelCount <= 0) return 1;
    if (x < render->screenLeft || x >= render->screenRight ||
        y < render->screenTop || y >= render->screenBottom) {
        RECORD_FRAME_FAILURE(FIRST_FRAME_FAIL_SPAN_SCREEN,
                             render->numLines,
                             source->logicalId,
                             source->actualId,
                             0U,
                             source->sourceTexelOffset,
                             x, y, pixelCount, texelStep);
        return 0;
    }

    pitch = render->pitch >> 1;
    pixels = (uint16_t*)render->pixels + pitch * y + x;
    localPosition = (int64_t)texelPosition;
    remaining = pixelCount;

    while (remaining-- > 0) {
        uint32_t packedIndex;
        uint8_t packed;
        uint32_t paletteIndex;
        int nibbleShift;

        if (localPosition < 0) {
            RECORD_FRAME_FAILURE(FIRST_FRAME_FAIL_SPAN_NEGATIVE,
                                 render->numLines,
                                 source->logicalId,
                                 source->actualId,
                                 0U,
                                 source->sourceTexelOffset,
                                 (int32_t)localPosition,
                                 texelStep,
                                 remaining,
                                 x);
            return 0;
        }
        packedIndex = (uint32_t)(localPosition >> 13);
        if (packedIndex >= WALL_PACKED_BYTES) {
            if (packedIndex == WALL_PACKED_BYTES &&
                legacyWallGuard.valid == 1U &&
                legacyWallGuard.logicalId == source->logicalId &&
                legacyWallGuard.actualId == source->actualId &&
                legacyWallGuard.sourceOffset == source->sourceTexelOffset) {
                packed = legacyWallGuard.packedByte;
            }
            else {
                RECORD_FRAME_FAILURE(FIRST_FRAME_FAIL_SPAN_OOB,
                                     render->numLines,
                                     source->logicalId,
                                     source->actualId,
                                     0U,
                                     source->sourceTexelOffset,
                                     (int32_t)packedIndex,
                                     (int32_t)localPosition,
                                     texelStep,
                                     remaining);
                return 0;
            }
        }
        else {
            packed = texels[packedIndex];
        }
        nibbleShift = (int)((localPosition >> 10) & 4);
        paletteIndex = (uint32_t)((packed >> nibbleShift) & 0x0f);
        *pixels = source->paletteRgb565[paletteIndex];
        pixels += pitch;
        localPosition += texelStep;
        ++work->pixelsDrawn;
    }
    return 1;
}

static int drawProjectedWall(FirstFrameWork* work,
                             Line_t* line,
                             const ResolvedWallTexture* source,
                             const uint8_t* texels) {
    Render_t* render;
    int i, i2, i3, i4, i5, i6, i7, i8;
    int i12, i13, i14, i15, i16, i17, zPos;

    if (work == NULL || line == NULL || source == NULL || texels == NULL ||
        work->render == NULL) return 0;
    render = work->render;

    i = line->vert2.x - line->vert1.x;
    if (i <= 0) return 1;

    ++render->lineRasterCount;
    i2 = (MAXINT / i) << 1;
    i3 = (int)DoomRPG_FixedMul((line->vert2.y - line->vert1.y), i2);
    i4 = (int)DoomRPG_FixedMul((line->vert2.z - line->vert1.z), i2);
    i5 = (line->vert1.x + 65535) >> 16;
    i6 = (line->vert2.x + 65535) >> 16;

    if (render->screenLeft > i5) i5 = render->screenLeft;
    if (render->screenRight < i6) i6 = render->screenRight;

    {
        int j = (i5 << 16) - line->vert1.x;
        i7 = line->vert1.z + DoomRPG_FixedMul(j, i4);
        i8 = line->vert1.y + DoomRPG_FixedMul(j, i3);
    }

    while (i5 < i6) {
        if (i8 <= 0) {
            RECORD_FRAME_FAILURE(FIRST_FRAME_FAIL_WALL_DEPTH,
                                 render->numLines,
                                 source->logicalId,
                                 source->actualId,
                                 (uint32_t)line->flags,
                                 source->sourceTexelOffset,
                                 i8, i5, i6, i3);
            return 0;
        }
        i12 = (0x40000000 / i8) << 2;
        i13 = ((int)(DoomRPG_FixedMul(i7, i12) >> 16)) & 63;
        i8 += i3;
        i7 += i4;

        if (render->columnScale[i5] >= i12) {
            render->columnScale[i5] = i12;
            i14 = i12 >> 3;
            i15 = (64 * i8) >> 17;
            if (line->flags & 0xC0010000) {
                if (!(line->flags & 0xC0000000)) i15 *= 2;
                zPos = 128;
            }
            else {
                zPos = 64;
            }
            /* Match the recovered fixed-version wall path exactly. */
            zPos = 64;
            i16 = render->halfScreenHeight -
                  (((zPos - render->viewZ) * i8) >> 17);
            /* The legacy renderer addressed one map-wide mediaTexels array.
             * Native wall texels are already a bounded 2048-byte cache slot,
             * so keep the raster coordinate local and avoid overflowing an
             * irrelevant global source offset before subtracting it again. */
            i17 = (i13 << 6) << 12;

            if (render->screenTop > i16) {
                i17 -= i14 * (i16 - render->screenTop);
                i15 += i16 - render->screenTop;
                i16 = render->screenTop;
            }
            if (i16 + i15 > render->screenBottom) {
                i15 = render->screenBottom - i16;
            }
            if (!sampleWallSpan(work, source, texels,
                                i5, i16, i17, i14, i15)) {
                if (frameFailure.code != FIRST_FRAME_FAIL_NONE) {
                    frameFailure.flags = (uint32_t)line->flags;
                }
                else {
                    RECORD_FRAME_FAILURE(FIRST_FRAME_FAIL_WALL_RASTER,
                                         render->numLines,
                                         source->logicalId,
                                         source->actualId,
                                         (uint32_t)line->flags,
                                         source->sourceTexelOffset,
                                         i5, i16, i17, i15);
                }
                return 0;
            }
        }
        ++i5;
    }
    return 1;
}

static void buildSourceLine(const EspMapLine* source, Line_t* out) {
    int dx;
    int dy;
    int extent;
    if (source == NULL || out == NULL) return;
    memset(out, 0, sizeof(*out));
    out->vert1.x = source->x1;
    out->vert1.y = source->y1;
    out->vert2.x = source->x2;
    out->vert2.y = source->y2;
    out->texture = (short)source->texture;
    out->flags = (int)source->flags;

    dx = out->vert2.x - out->vert1.x;
    if (dx < 0) dx = -dx;
    dy = out->vert2.y - out->vert1.y;
    if (dy < 0) dy = -dy;
    extent = dx > dy ? dx : dy;

    if ((source->flags & LINE_FLAG_REVERSE_TEX) == 0U) {
        out->vert1.z = 0;
        out->vert2.z = extent;
    }
    else {
        out->vert1.z = extent;
        out->vert2.z = 0;
    }

    if ((source->flags & LINE_FLAG_X_NUDGE) != 0U) {
        if ((source->flags & LINE_FLAG_AXIS_X) != 0U) {
            out->vert1.x += 3;
            out->vert2.x += 3;
        }
        else if ((source->flags & LINE_FLAG_AXIS_NEGATIVE) != 0U) {
            out->vert1.x -= 3;
            out->vert2.x -= 3;
        }
    }
    else if ((source->flags & LINE_FLAG_Y_NUDGE) != 0U) {
        if ((source->flags & LINE_FLAG_AXIS_X) != 0U) {
            out->vert1.y += 3;
            out->vert2.y += 3;
        }
        else if ((source->flags & LINE_FLAG_AXIS_NEGATIVE) != 0U) {
            out->vert1.y -= 3;
            out->vert2.y -= 3;
        }
    }
}

static int drawLine(FirstFrameWork* work, uint32_t lineIndex) {
    EspMapLine mapLine;
    Line_t projected;
    Vertex_t swap;
    const ResolvedWallTexture* source;
    const uint8_t* texels;

    if (work == NULL || !EspMapRuntime_getLine(lineIndex, &mapLine)) {
        RECORD_FRAME_FAILURE(FIRST_FRAME_FAIL_LINE_FETCH,
                             lineIndex, 0U, 0U, 0U, 0U,
                             0, 0, 0, 0);
        return 0;
    }
    ++work->lineCandidates;
    buildSourceLine(&mapLine, &projected);

    if (((projected.vert1.x - work->render->viewX) *
         (projected.vert2.y - projected.vert1.y)) +
        ((projected.vert1.y - work->render->viewY) *
         (-(projected.vert2.x - projected.vert1.x))) <= 0) {
        if ((projected.flags & LINE_FLAG_TWO_SIDED) == 0) {
            ++work->backfaceCulled;
            return 1;
        }
        swap = projected.vert1;
        projected.vert1 = projected.vert2;
        projected.vert2 = swap;
    }

    Render_transform2DVerts(work->render, &projected.vert1);
    Render_transform2DVerts(work->render, &projected.vert2);
    if (!Render_clipLine(work->render, &projected)) {
        ++work->clipCulled;
        return 1;
    }
    Render_projectVertex(work->render, &projected.vert1);
    Render_projectVertex(work->render, &projected.vert2);

    if ((projected.flags & LINE_FLAG_OCCLUDER_ONLY) != 0) {
        Render_occludeClippedLine(work->render, &projected);
        ++work->occluderOnly;
        return 1;
    }
    if ((projected.flags & LINE_FLAG_SPRITE_SPAN) != 0) {
        ++work->spriteSpanSkipped;
        return 1;
    }

    source = findResolved(work, mapLine.texture);
    if (source == NULL) {
        RECORD_FRAME_FAILURE(FIRST_FRAME_FAIL_TEXTURE_MISSING,
                             lineIndex,
                             mapLine.texture,
                             0U,
                             mapLine.flags,
                             0U,
                             0, 0, 0, 0);
        return 0;
    }
    projected.texture = (short)source->actualId;
    work->render->spanMode = 0;
    work->render->numLines = (int)lineIndex;
    ++work->wallRequests;

    if (!acquireWall(work, source, &texels)) {
        RECORD_FRAME_FAILURE(FIRST_FRAME_FAIL_WALL_LOAD,
                             lineIndex,
                             source->logicalId,
                             source->actualId,
                             mapLine.flags,
                             source->sourceTexelOffset,
                             0, 0, 0, 0);
        return 0;
    }
    if (!drawProjectedWall(work, &projected, source, texels)) {
        if (frameFailure.code != FIRST_FRAME_FAIL_NONE) {
            frameFailure.flags = mapLine.flags;
        }
        else {
            RECORD_FRAME_FAILURE(FIRST_FRAME_FAIL_WALL_RASTER,
                                 lineIndex,
                                 source->logicalId,
                                 source->actualId,
                                 mapLine.flags,
                                 source->sourceTexelOffset,
                                 0, 0, 0, 0);
        }
        return 0;
    }
    ++work->wallDraws;
    return 1;
}

static int walkNodes(FirstFrameWork* work) {
    uint16_t nodeStack[MAX_BSP_DEPTH + 1U];
    uint8_t depthStack[MAX_BSP_DEPTH + 1U];
    uint32_t pending = 0U;

    if (work == NULL || work->runtime == NULL || work->runtime->nodeCount == 0U ||
        work->runtime->nodeCount > (uint32_t)UINT16_MAX + 1U) {
        return 0;
    }

    nodeStack[pending] = 0U;
    depthStack[pending] = 0U;
    ++pending;

    while (pending != 0U) {
        EspMapNode compact;
        Node_t node;
        uint32_t lineStart;
        uint32_t lineCount;
        uint32_t i;
        uint32_t split;
        uint32_t first;
        uint32_t second;
        uint32_t nearNode;
        uint32_t farNode;
        uint32_t itemNode;
        uint32_t itemDepth;

        --pending;
        itemNode = nodeStack[pending];
        itemDepth = depthStack[pending];

        if (itemDepth > MAX_BSP_DEPTH ||
            itemNode >= work->runtime->nodeCount ||
            !EspMapRuntime_getNode(itemNode, &compact)) {
            return 0;
        }

        memset(&node, 0, sizeof(node));
        node.x1 = (short)compact.x1;
        node.y1 = (short)compact.y1;
        node.x2 = (short)compact.x2;
        node.y2 = (short)compact.y2;
        node.args1 = (int)compact.args1;
        node.args2 = (int)compact.args2;

        ++work->render->nodeCount;
        if (Render_cullBoundingBox(work->render, &node)) {
            ++work->nodeCulled;
            continue;
        }

        if ((compact.args1 & 0x30000U) == 0U) {
            lineStart = compact.args2 & 0xFFFFU;
            lineCount = (compact.args2 >> 16) & 0xFFFFU;
            if (lineStart > work->runtime->lineCount ||
                lineCount > work->runtime->lineCount - lineStart) {
                return 0;
            }
            ++work->leafNodes;
            ++work->render->nodeRasterCount;
            work->render->lineCount += (int)lineCount;
            for (i = 0U; i < lineCount; ++i) {
                if (!drawLine(work, lineStart + i)) return 0;
            }
            continue;
        }

        first = (compact.args2 >> 16) & 0xFFFFU;
        second = compact.args2 & 0xFFFFU;
        if (first >= work->runtime->nodeCount ||
            second >= work->runtime->nodeCount ||
            itemDepth >= MAX_BSP_DEPTH) {
            return 0;
        }

        split = compact.args1 & 0xFFFFU;
        if (((compact.args1 & 0x20000U) == 0U ||
             work->render->viewY <= (int)split) &&
            ((compact.args1 & 0x10000U) == 0U ||
             work->render->viewX <= (int)split)) {
            nearNode = first;
            farNode = second;
        }
        else {
            nearNode = second;
            farNode = first;
        }

        if (pending + 2U > MAX_BSP_DEPTH + 1U) return 0;

        /* LIFO: push the far child first so the near child is processed next,
         * exactly matching the old recursive front-to-back traversal. The
         * compact local stacks cost 195 bytes only while this render is live. */
        nodeStack[pending] = (uint16_t)farNode;
        depthStack[pending] = (uint8_t)(itemDepth + 1U);
        ++pending;
        nodeStack[pending] = (uint16_t)nearNode;
        depthStack[pending] = (uint8_t)(itemDepth + 1U);
        ++pending;
    }

    return 1;
}

static void fillBackground(Render_t* render,
                           uint16_t ceiling,
                           uint16_t floor,
                           int clearWholeFramebuffer) {
    uint16_t* fb;
    int y;
    int x;
    int half;
    int pitchPixels;

    fb = (uint16_t*)render->framebuffer;
    pitchPixels = render->pitch >> 1;
    if (clearWholeFramebuffer) {
        memset(render->framebuffer, 0,
               DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t));
    }

    half = render->screenHeight >> 1;
    for (y = 0; y < render->screenHeight; ++y) {
        uint16_t color = y < half ? ceiling : floor;
        uint16_t* row = fb + (render->screenY + y) * pitchPixels + render->screenX;
        for (x = 0; x < render->screenWidth; ++x) row[x] = color;
    }
}

static int renderFrame(Render_t* render,
                       const EspPlayerViewState* playerView,
                       EspNativeFirstFrameState* outState,
                       int clearWholeFramebuffer) {
    FirstFrameWork* work = &frameWorkScratch;
    RenderScratch* scratch = &frameRenderScratch;
    EspAssetPackEntry mappings;
    EspAssetPackEntry palettes;
    EspAssetPackEntry mapEntry;
    uint8_t wallHeader[WALL_TEXEL_HEADER_BYTES];
    uint8_t bspHeader[BSP_HEADER_BYTES];
    const EspMapLineStateView* lineState;
    const char* resourceName;
    int sin_;
    int cos_;
    int vx;
    int vy;
    int ok = 0;

    memset(&frameFailure, 0, sizeof(frameFailure));
    if (render == NULL || playerView == NULL || outState == NULL ||
        playerView->active != 1U ||
        !EspMapCatalog_isValidId(playerView->targetMapId) ||
        render->framebuffer == NULL || render->columnScale == NULL ||
        render->screenWidth <= 0 || render->screenWidth > (int)MAX_SCREEN_WIDTH ||
        render->screenHeight <= 0 ||
        render->screenX < 0 || render->screenY < 0 ||
        render->screenX + render->screenWidth > DOOMRPG_LOGICAL_WIDTH ||
        render->screenY + render->screenHeight > DOOMRPG_LOGICAL_HEIGHT ||
        !legacyGraphicsRuntimeClear(render) || !EspMapRuntime_isLoaded() ||
        !EspNativeGraphicsCatalog_isReady() || EspAssetPack_isOpen()) {
        return 0;
    }

    lineState = EspMapLineState_view();
    if (lineState == NULL || lineState->openCount != 0U ||
        frameRenderBusy != 0U) return 0;

    memset(work, 0, sizeof(*work));
    work->render = render;
    work->runtime = EspMapRuntime_view();
    resourceName = EspMapCatalog_nameForId(playerView->targetMapId);
    if (work->runtime == NULL || work->runtime->lineCount == 0U ||
        work->runtime->nodeCount == 0U || work->runtime->sourceBytes == 0U ||
        work->runtime->sourceCrc32 == 0U || resourceName == NULL) return 0;

    frameRenderBusy = 1U;
    saveRenderScratch(render, scratch);
    if (frameScratchLogged == 0U) {
        printf("[NATIVEFRAME] SCRATCH owner=BSS bytes=%u work=%u render=%u stack=bounded reentrant=no\n",
               (unsigned int)(sizeof(frameWorkScratch) + sizeof(frameRenderScratch)),
               (unsigned int)sizeof(frameWorkScratch),
               (unsigned int)sizeof(frameRenderScratch));
        frameScratchLogged = 1U;
    }

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) goto done;
    if (!EspAssetPack_findEntry("mappings.bin", &mappings) ||
        !EspAssetPack_findEntry("palettes.bin", &palettes) ||
        !EspAssetPack_findEntry("wtexels.bin", &work->wallTexels) ||
        !EspAssetPack_findEntry(resourceName, &mapEntry) ||
        mapEntry.size != work->runtime->sourceBytes ||
        mapEntry.crc32 != work->runtime->sourceCrc32 ||
        !EspAssetPack_readRange(&work->wallTexels, 0U, wallHeader, sizeof(wallHeader)) ||
        !EspAssetPack_readRange(&mapEntry, 0U, bspHeader, sizeof(bspHeader))) {
        goto done;
    }

    work->wallTexelDataBytes = readLe32(wallHeader);
    if (work->wallTexelDataBytes + WALL_TEXEL_HEADER_BYTES != work->wallTexels.size ||
        !buildResolvedTextures(work, &mappings, &palettes)) {
        goto done;
    }

    outState->ceilingRgb565 = rgb565(bspHeader[19], bspHeader[20], bspHeader[21]);
    outState->floorRgb565 = rgb565(bspHeader[16], bspHeader[17], bspHeader[18]);

    fillBackground(render, outState->ceilingRgb565, outState->floorRgb565,
                   clearWholeFramebuffer);

    sin_ = render->sinTable[playerView->viewAngle & 255];
    cos_ = render->sinTable[(playerView->viewAngle + 64) & 255];
    vx = playerView->viewX - ((16 * cos_) >> 16);
    vy = playerView->viewY + ((16 * sin_) >> 16);

    render->viewX = vx;
    render->viewY = vy;
    render->viewZ = playerView->viewZ;
    render->viewCos_ = cos_;
    render->viewSin_ = -sin_;
    render->viewTransX = -((vx * render->viewCos_) + (vy * render->viewSin_));
    render->viewSin = sin_;
    render->viewCos = cos_;
    render->viewTransY = -((vx * render->viewSin) + (vy * render->viewCos));
    render->viewAngle = playerView->viewAngle;
    render->pixels = (short*)&render->framebuffer[
        render->pitch * render->screenY + render->screenX * (int)sizeof(short)];
    render->screenLeft = 0;
    render->screenTop = 0;
    render->screenRight = render->screenWidth;
    render->screenBottom = render->screenHeight;
    render->lineCount = 0;
    render->lineRasterCount = 0;
    render->nodeCount = 0;
    render->nodeRasterCount = 0;
    render->spriteCount = 0;
    render->spriteRasterCount = 0;
    render->spanMode = 0;

    Render_initColumnScale(render);
    if (!walkNodes(work)) {
        if (frameFailure.code == FIRST_FRAME_FAIL_NONE) {
            RECORD_FRAME_FAILURE(FIRST_FRAME_FAIL_WALK,
                                 UINT32_MAX, 0U, 0U, 0U, 0U,
                                 render->nodeCount,
                                 work->lineCandidates,
                                 work->wallRequests,
                                 work->wallDraws);
        }
        goto done;
    }

    outState->lineCandidates = work->lineCandidates;
    outState->leafNodes = work->leafNodes;
    outState->wallRequests = work->wallRequests;
    outState->wallDraws = work->wallDraws;
    outState->spanCalls = work->spanCalls;
    outState->pixelsDrawn = work->pixelsDrawn;
    outState->cacheHits = work->cacheHits;
    outState->cacheMisses = work->cacheMisses;

    printf("[NATIVEFRAME] BSP map=%u resource=%s nodes=%u leaves=%u nodeCull=%u lines=%u backface=%u clip=%u occluder=%u spriteSpanDeferred=%u\n",
           (unsigned int)playerView->targetMapId,
           resourceName,
           (unsigned int)render->nodeCount,
           (unsigned int)work->leafNodes,
           (unsigned int)work->nodeCulled,
           (unsigned int)work->lineCandidates,
           (unsigned int)work->backfaceCulled,
           (unsigned int)work->clipCulled,
           (unsigned int)work->occluderOnly,
           (unsigned int)work->spriteSpanSkipped);
    printf("[NATIVEFRAME] WALL requests=%u draws=%u spans=%u pixels=%u cache=%uH/%uM/%uE resolvedTextures=%u animationTime=%u\n",
           (unsigned int)work->wallRequests,
           (unsigned int)work->wallDraws,
           (unsigned int)work->spanCalls,
           (unsigned int)work->pixelsDrawn,
           (unsigned int)work->cacheHits,
           (unsigned int)work->cacheMisses,
           (unsigned int)work->cacheEvictions,
           (unsigned int)work->resolvedCount,
           (unsigned int)FIRST_FRAME_ANIMATION_TIME);

    ok = work->wallDraws > 0U && work->pixelsDrawn > 0U;

done:
    releaseCache(work);
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    restoreRenderScratch(render, scratch);
    frameRenderBusy = 0U;
    return ok;
}

static int renderFrameWithLegacyGuardRecovery(
    Render_t* render,
    const EspPlayerViewState* playerView,
    EspNativeFirstFrameState* outState,
    int clearWholeFramebuffer) {
    if (renderFrame(render, playerView, outState, clearWholeFramebuffer)) {
        return 1;
    }
    if (frameFailure.code != FIRST_FRAME_FAIL_SPAN_OOB ||
        frameFailure.value0 != (int32_t)WALL_PACKED_BYTES ||
        !prepareLegacyWallGuard()) {
        return 0;
    }

    printf("[NATIVEFRAME] RETRY legacy compact guard after unwound SPAN_OOB line=%u actual=%u\n",
           (unsigned int)frameFailure.lineIndex,
           (unsigned int)frameFailure.actualId);
    if (!renderFrame(render, playerView, outState, clearWholeFramebuffer)) {
        return 0;
    }
    printf("[NATIVEFRAME] RECOVERED legacy compact guard actual=%u successorActual=%u source=%u->%u\n",
           (unsigned int)legacyWallGuard.actualId,
           (unsigned int)legacyWallGuard.successorActualId,
           (unsigned int)legacyWallGuard.sourceOffset,
           (unsigned int)legacyWallGuard.successorSourceOffset);
    return 1;
}

void EspNativeFirstFrame_reset(void) {
    memset(&frameState, 0, sizeof(frameState));
    memset(&frameFailure, 0, sizeof(frameFailure));
    memset(&legacyWallGuard, 0, sizeof(legacyWallGuard));
    if (frameRenderBusy == 0U) {
        memset(&frameWorkScratch, 0, sizeof(frameWorkScratch));
        memset(&frameRenderScratch, 0, sizeof(frameRenderScratch));
        frameScratchLogged = 0U;
    }
}

int EspNativeFirstFrame_isReady(void) {
    return frameState.active == 1U && frameState.rendered == 1U &&
           frameState.presented == 1U &&
           EspMapCatalog_isValidId(frameState.targetMapId);
}

const EspNativeFirstFrameState* EspNativeFirstFrame_view(void) {
    return EspNativeFirstFrame_isReady() ? &frameState : NULL;
}

EspNativeFirstFrameStatus EspNativeFirstFrame_route(
    struct Render_s* renderBase,
    const struct EspPlayerViewState_s* playerViewBase) {
    Render_t* render = (Render_t*)renderBase;
    const EspPlayerViewState* playerView = (const EspPlayerViewState*)playerViewBase;
    EspNativeFirstFrameState candidate;

    if (render == NULL || playerView == NULL) return ESP_NATIVE_FIRST_FRAME_INVALID;
    if (EspNativeFirstFrame_isReady()) return ESP_NATIVE_FIRST_FRAME_ALREADY_ACTIVE;
    if (EspAssetPack_isOpen()) return ESP_NATIVE_FIRST_FRAME_PACK_BUSY;
    if (!EspMapRuntime_isLoaded() || !EspNativeGraphicsCatalog_isReady())
        return ESP_NATIVE_FIRST_FRAME_NOT_READY;

    memset(&candidate, 0, sizeof(candidate));
    candidate.frameBeforeFNV = framebufferFNV(render);
    candidate.targetMapId = playerView->targetMapId;
    if (candidate.frameBeforeFNV == 0U) return ESP_NATIVE_FIRST_FRAME_SOURCE_INVALID;

    if (!renderFrameWithLegacyGuardRecovery(render, playerView, &candidate, 1)) {
        printFrameFailure("initial", playerView);
        return ESP_NATIVE_FIRST_FRAME_RENDER_FAILED;
    }

    candidate.frameAfterFNV = framebufferFNV(render);
    if (candidate.frameAfterFNV == 0U ||
        candidate.frameAfterFNV == candidate.frameBeforeFNV ||
        candidate.wallDraws == 0U || candidate.pixelsDrawn == 0U) {
        return ESP_NATIVE_FIRST_FRAME_RENDER_FAILED;
    }
    candidate.rendered = 1U;

    if (!Esp32PlatformVideo_present()) {
        return ESP_NATIVE_FIRST_FRAME_PRESENT_FAILED;
    }

    candidate.presented = 1U;
    candidate.active = 1U;
    frameState = candidate;
    return ESP_NATIVE_FIRST_FRAME_OK;
}

EspNativeFirstFrameStatus EspNativeFirstFrame_renderGameplayViewport(
    struct Render_s* renderBase,
    const struct EspPlayerViewState_s* playerViewBase,
    EspNativeFirstFrameState* outState) {
    Render_t* render = (Render_t*)renderBase;
    const EspPlayerViewState* playerView = (const EspPlayerViewState*)playerViewBase;
    EspNativeFirstFrameState candidate;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (render == NULL || playerView == NULL || outState == NULL) {
        return ESP_NATIVE_FIRST_FRAME_INVALID;
    }
    if (EspAssetPack_isOpen()) return ESP_NATIVE_FIRST_FRAME_PACK_BUSY;
    if (!EspMapRuntime_isLoaded() || !EspNativeGraphicsCatalog_isReady()) {
        return ESP_NATIVE_FIRST_FRAME_NOT_READY;
    }

    memset(&candidate, 0, sizeof(candidate));
    candidate.frameBeforeFNV = framebufferFNV(render);
    candidate.targetMapId = playerView->targetMapId;
    if (candidate.frameBeforeFNV == 0U) {
        return ESP_NATIVE_FIRST_FRAME_SOURCE_INVALID;
    }

    if (!renderFrameWithLegacyGuardRecovery(render, playerView, &candidate, 0)) {
        printFrameFailure("gameplay", playerView);
        return ESP_NATIVE_FIRST_FRAME_RENDER_FAILED;
    }

    candidate.frameAfterFNV = framebufferFNV(render);
    if (candidate.frameAfterFNV == 0U || candidate.wallDraws == 0U ||
        candidate.pixelsDrawn == 0U) {
        return ESP_NATIVE_FIRST_FRAME_RENDER_FAILED;
    }

    candidate.rendered = 1U;
    candidate.presented = 0U;
    candidate.active = 1U;
    *outState = candidate;
    return ESP_NATIVE_FIRST_FRAME_OK;
}
