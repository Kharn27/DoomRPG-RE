#include <SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_first_frame.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_junction_sprite_renderer.h"
#include "native_junction_sprite_census_probe.h"
#include "native_junction_sprite_overlay_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

/* Keep ESP-IDF headers after the legacy boolean definition. */
#include <esp_heap_caps.h>

#define EXPECTED_BASE_FRAME_FNV 0x8910c2edU
#define EXPECTED_TOPOLOGY_FNV 0xd6e8df7dU
#define EXPECTED_DIRECT_CATALOG_FNV 0x969d5a77U
#define EXPECTED_TEXTURE_FNV 0x2dd5dfcfU
#define EXPECTED_DIRECT_SPRITE_FNV 0xcfd036cfU
#define EXPECTED_DIRECT_TEXTURES 30U
#define EXPECTED_DIRECT_SPRITES 16U
#define EXPECTED_DIRECT_STORAGE 1840U
#define EXPECTED_CLOSED_SPRITES 17U
#define EXPECTED_CLOSED_STORAGE 1880U
#define EXPECTED_DEPENDENCY_ID 136U
#define MAX_CATALOG_INCREMENT_OVERHEAD 64U

#define EXPECTED_BSP_CANDIDATES 21U
#define EXPECTED_BSP_REJECTED 27U
#define EXPECTED_MODE0_OBJECTS 14U
#define EXPECTED_MODE7_OBJECTS 7U
#define EXPECTED_BASE_MODE7_PIXELS 311U
#define EXPECTED_BASE_DRAWS 21U
#define EXPECTED_BASE_SPANS 219U
#define EXPECTED_BASE_PIXELS 1828U
#define EXPECTED_BASE_WALL_OCCLUDED 62U
#define EXPECTED_BASE_FRAME_LOADS 21U
#define EXPECTED_BASE_UNIQUE_LOGICAL 9U
#define EXPECTED_BASE_FRAME_BYTES 12251U
#define EXPECTED_BASE_MAX_FRAME 1020U
#define EXPECTED_ORDER_FNV 0xf16737cbU
#define EXPECTED_GLOW_COMPANIONS 7U

#define EXPECTED_DEPTH_NODES 39U
#define EXPECTED_DEPTH_LEAVES 12U
#define EXPECTED_DEPTH_NODE_CULL 8U
#define EXPECTED_DEPTH_LINES 62U
#define EXPECTED_DEPTH_BACKFACE 20U
#define EXPECTED_DEPTH_CLIP 8U
#define EXPECTED_DEPTH_OCCLUDER 0U
#define EXPECTED_DEPTH_SPRITE_SPAN 0U

static struct {
    int attempted;
    int done;
} probeState;

static uint32_t fnv1a(const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t fnvAppend(uint32_t hash, const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t frameFNV(const Render_t* render) {
    uint32_t bytes = DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                     (uint32_t)sizeof(uint16_t);
    if (render == NULL || render->framebuffer == NULL ||
        Esp32PlatformVideo_framebuffer() != render->framebuffer ||
        Esp32PlatformVideo_framebufferSizeBytes() != bytes) {
        return 0U;
    }
    return fnv1a(render->framebuffer, bytes);
}

static uint32_t heap8(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static int legacyClear(const Render_t* render) {
    return render != NULL && render->lines == NULL && render->nodes == NULL &&
           render->mapSprites == NULL && render->mediaTexelOffsets == NULL &&
           render->mediaBitShapeOffsets == NULL &&
           render->mediaTexturesIds == NULL && render->mediaSpriteIds == NULL &&
           render->shapeData == NULL && render->mediaTexels == NULL &&
           render->mapTextureTexels == NULL && render->mapSpriteTexels == NULL;
}

static int depthMatchesFirstFrame(const EspNativeJunctionSpriteStats* stats) {
    return stats != NULL &&
           stats->depthNodes == EXPECTED_DEPTH_NODES &&
           stats->depthLeaves == EXPECTED_DEPTH_LEAVES &&
           stats->depthNodeCulled == EXPECTED_DEPTH_NODE_CULL &&
           stats->depthLines == EXPECTED_DEPTH_LINES &&
           stats->depthBackfaceCulled == EXPECTED_DEPTH_BACKFACE &&
           stats->depthClipCulled == EXPECTED_DEPTH_CLIP &&
           stats->depthOccluders == EXPECTED_DEPTH_OCCLUDER &&
           stats->depthSpriteSpans == EXPECTED_DEPTH_SPRITE_SPAN;
}

static uint32_t directSpriteFNV(const EspNativeGraphicsCatalogView* catalog) {
    uint32_t hash = 2166136261U;
    uint16_t i;
    uint16_t count = 0U;
    if (catalog == NULL || catalog->sprites == NULL) return 0U;
    for (i = 0U; i < catalog->spriteCount; ++i) {
        if (catalog->sprites[i].resourceId == EXPECTED_DEPENDENCY_ID) continue;
        hash = fnvAppend(hash, &catalog->sprites[i],
                         (uint32_t)sizeof(catalog->sprites[i]));
        ++count;
    }
    return count == EXPECTED_DIRECT_SPRITES ? hash : 0U;
}

void Esp32JunctionSpriteOverlayProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32JunctionSpriteOverlayProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionSpriteOverlayProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    Render_t* render;
    const EspMapSpriteTopologyView* topology;
    const EspNativeGraphicsCatalogView* catalog;
    EspNativeJunctionSpriteStats stats;
    EspNativeGraphicsCatalogStatus closureStatus;
    EspNativeGraphicsCatalogStatus repeatStatus;
    uint32_t before;
    uint32_t afterClosure;
    uint32_t after;
    uint32_t topologyFNV;
    uint32_t directCatalogFNV;
    uint32_t closedCatalogFNV;
    uint32_t textureFNV;
    uint32_t directSpriteHash;
    uint32_t catalogHeapBefore;
    uint32_t catalogHeapAfter;
    uint32_t catalogLargestBefore;
    uint32_t catalogLargestAfter;
    uint32_t catalogIncrement;
    uint32_t renderHeapBefore;
    uint32_t renderHeapAfter;
    uint32_t renderLargestBefore;
    uint32_t renderLargestAfter;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionSpriteCensusProbe_isDone() ||
        !EspNativeFirstFrame_isReady()) {
        return;
    }
    probeState.attempted = 1;

    render = doomRpg != NULL ? doomRpg->render : NULL;
    topology = EspMapSpriteTopology_view();
    catalog = EspNativeGraphicsCatalog_view();
    before = frameFNV(render);

    if (render == NULL || topology == NULL || catalog == NULL ||
        before != EXPECTED_BASE_FRAME_FNV ||
        topology->stateFNV1a != EXPECTED_TOPOLOGY_FNV ||
        catalog->stateFNV1a != EXPECTED_DIRECT_CATALOG_FNV ||
        catalog->textureCount != EXPECTED_DIRECT_TEXTURES ||
        catalog->spriteCount != EXPECTED_DIRECT_SPRITES ||
        catalog->storageBytes != EXPECTED_DIRECT_STORAGE ||
        EspNativeGraphicsCatalog_findSprite(EXPECTED_DEPENDENCY_ID) != NULL ||
        !legacyClear(render) || EspAssetPack_isOpen()) {
        printf("[JUNCTIONGLOW] FAILED boundary frame=%08x topology=%08x catalog=%08x counts=%u/%u storage=%u dep136=%p legacyClear=%d pack=%d\n",
               (unsigned int)before,
               (unsigned int)(topology ? topology->stateFNV1a : 0U),
               (unsigned int)(catalog ? catalog->stateFNV1a : 0U),
               catalog ? (unsigned int)catalog->textureCount : 0U,
               catalog ? (unsigned int)catalog->spriteCount : 0U,
               catalog ? (unsigned int)catalog->storageBytes : 0U,
               (const void*)EspNativeGraphicsCatalog_findSprite(
                   EXPECTED_DEPENDENCY_ID),
               legacyClear(render), EspAssetPack_isOpen());
        return;
    }

    topologyFNV = topology->stateFNV1a;
    directCatalogFNV = catalog->stateFNV1a;
    catalogHeapBefore = heap8();
    catalogLargestBefore = largest8();

    printf("\n=== Doom RPG ESP32-native Junction dependency-closed sprite+glow frame ===\n");
    printf("[JUNCTIONGLOW] CONTRACT preserve direct sparse catalog predecessor 30T/16S/1840B/969d5a77, atomically close renderer dependency 135/140->136 from native PAK, then render exactly in legacy view-sprite order with each mode7 glow immediately after its parent; shared compact BSP visibility owns leaf admission and wall depth; no legacy graphics pools, world/entity/input/turn mutation or runtime ZIP\n");

    closureStatus = EspNativeGraphicsCatalog_expandSpriteDependencies();
    catalog = EspNativeGraphicsCatalog_view();
    afterClosure = frameFNV(render);
    catalogHeapAfter = heap8();
    catalogLargestAfter = largest8();
    catalogIncrement = catalogHeapBefore >= catalogHeapAfter
                           ? catalogHeapBefore - catalogHeapAfter
                           : UINT32_MAX;

    if (closureStatus != ESP_NATIVE_GRAPHICS_CATALOG_OK || catalog == NULL ||
        afterClosure != before ||
        catalog->textureCount != EXPECTED_DIRECT_TEXTURES ||
        catalog->spriteCount != EXPECTED_CLOSED_SPRITES ||
        catalog->storageBytes != EXPECTED_CLOSED_STORAGE ||
        catalog->stateFNV1a == 0U ||
        catalog->stateFNV1a == EXPECTED_DIRECT_CATALOG_FNV ||
        EspNativeGraphicsCatalog_findSprite(EXPECTED_DEPENDENCY_ID) == NULL ||
        EspNativeGraphicsCatalog_findSprite(144U) != NULL ||
        catalogIncrement < sizeof(EspNativeGraphicsCatalogRecord) ||
        catalogIncrement > sizeof(EspNativeGraphicsCatalogRecord) +
                               MAX_CATALOG_INCREMENT_OVERHEAD ||
        EspAssetPack_isOpen()) {
        printf("[JUNCTIONGLOW] FAILED closure status=%d frame=%08x->%08x counts=%u/%u storage=%u fnv=%08x dep136=%p dep144=%p heap=%u->%u increment=%u largest=%u->%u pack=%d\n",
               (int)closureStatus,
               (unsigned int)before,
               (unsigned int)afterClosure,
               catalog ? (unsigned int)catalog->textureCount : 0U,
               catalog ? (unsigned int)catalog->spriteCount : 0U,
               catalog ? (unsigned int)catalog->storageBytes : 0U,
               catalog ? (unsigned int)catalog->stateFNV1a : 0U,
               (const void*)EspNativeGraphicsCatalog_findSprite(136U),
               (const void*)EspNativeGraphicsCatalog_findSprite(144U),
               (unsigned int)catalogHeapBefore,
               (unsigned int)catalogHeapAfter,
               (unsigned int)catalogIncrement,
               (unsigned int)catalogLargestBefore,
               (unsigned int)catalogLargestAfter,
               EspAssetPack_isOpen());
        return;
    }

    textureFNV = fnv1a(catalog->textures,
                        (uint32_t)catalog->textureCount *
                            (uint32_t)sizeof(EspNativeGraphicsCatalogRecord));
    directSpriteHash = directSpriteFNV(catalog);
    closedCatalogFNV = catalog->stateFNV1a;
    repeatStatus = EspNativeGraphicsCatalog_expandSpriteDependencies();
    catalog = EspNativeGraphicsCatalog_view();
    if (textureFNV != EXPECTED_TEXTURE_FNV ||
        directSpriteHash != EXPECTED_DIRECT_SPRITE_FNV ||
        repeatStatus != ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE ||
        catalog == NULL || catalog->stateFNV1a != closedCatalogFNV ||
        frameFNV(render) != before || EspAssetPack_isOpen()) {
        printf("[JUNCTIONGLOW] FAILED closure integrity textureFNV=%08x directSpriteFNV=%08x repeat=%d state=%08x/%08x frame=%08x pack=%d\n",
               (unsigned int)textureFNV,
               (unsigned int)directSpriteHash,
               (int)repeatStatus,
               (unsigned int)closedCatalogFNV,
               (unsigned int)(catalog ? catalog->stateFNV1a : 0U),
               (unsigned int)frameFNV(render), EspAssetPack_isOpen());
        return;
    }

    renderHeapBefore = heap8();
    renderLargestBefore = largest8();
    memset(&stats, 0, sizeof(stats));
    if (!EspNativeJunctionSprite_render(render, &stats)) {
        printf("[JUNCTIONGLOW] FAILED renderer objects=%u bsp=%u/%u unsupported=%u baseModes=%u/%u baseDraws=%u basePixels=%u glows=%u/%u glowPixels=%u deferred=%u reads=%u\n",
               (unsigned int)stats.objects,
               (unsigned int)stats.bspCandidates,
               (unsigned int)stats.bspRejected,
               (unsigned int)stats.unsupported,
               (unsigned int)stats.mode0Objects,
               (unsigned int)stats.mode7Objects,
               (unsigned int)stats.draws,
               (unsigned int)stats.pixelsDrawn,
               (unsigned int)stats.glowCompanions,
               (unsigned int)stats.glowDraws,
               (unsigned int)stats.glowPixels,
               (unsigned int)stats.glowDeferred,
               (unsigned int)stats.packReads);
        return;
    }

    renderHeapAfter = heap8();
    renderLargestAfter = largest8();
    after = frameFNV(render);
    topology = EspMapSpriteTopology_view();
    catalog = EspNativeGraphicsCatalog_view();

    if (after == 0U || after == before || stats.objects != 48U ||
        stats.hidden != 0U || stats.unsupported != 0U ||
        stats.bspCandidates != EXPECTED_BSP_CANDIDATES ||
        stats.bspRejected != EXPECTED_BSP_REJECTED ||
        stats.mode0Objects != EXPECTED_MODE0_OBJECTS ||
        stats.mode7Objects != EXPECTED_MODE7_OBJECTS ||
        stats.mode7Pixels != EXPECTED_BASE_MODE7_PIXELS ||
        stats.draws != EXPECTED_BASE_DRAWS ||
        stats.nearCulled != 0U || stats.clipCulled != 0U ||
        stats.spanRuns != EXPECTED_BASE_SPANS ||
        stats.pixelsDrawn != EXPECTED_BASE_PIXELS ||
        stats.wallOccludedColumns != EXPECTED_BASE_WALL_OCCLUDED ||
        stats.frameLoads != EXPECTED_BASE_FRAME_LOADS ||
        stats.uniqueLogical != EXPECTED_BASE_UNIQUE_LOGICAL ||
        stats.frameBytes != EXPECTED_BASE_FRAME_BYTES ||
        stats.maxFrameBytes != EXPECTED_BASE_MAX_FRAME ||
        stats.orderFNV1a != EXPECTED_ORDER_FNV ||
        stats.glowCompanions != EXPECTED_GLOW_COMPANIONS ||
        stats.glowDeferred != 0U || stats.glowDraws == 0U ||
        stats.glowPixels == 0U || stats.glowFrameLoads == 0U ||
        !depthMatchesFirstFrame(&stats) ||
        renderHeapAfter != renderHeapBefore ||
        renderLargestAfter != renderLargestBefore ||
        topology == NULL || topology->stateFNV1a != topologyFNV ||
        catalog == NULL || catalog->stateFNV1a != closedCatalogFNV ||
        !legacyClear(render) || EspAssetPack_isOpen()) {
        printf("[JUNCTIONGLOW] FAILED post frame=%08x->%08x base=%u/%u/%u/%u/%u/%u/%u/%u order=%08x glows=%u draws=%u near=%u clip=%u spans=%u pixels=%u occ=%u frames=%u bytes=%u max=%u deferred=%u depth=%u/%u/%u/%u/%u/%u/%u/%u heap=%u->%u largest=%u->%u topology=%08x/%08x catalog=%08x/%08x legacyClear=%d pack=%d\n",
               (unsigned int)before, (unsigned int)after,
               (unsigned int)stats.draws,
               (unsigned int)stats.spanRuns,
               (unsigned int)stats.pixelsDrawn,
               (unsigned int)stats.mode7Pixels,
               (unsigned int)stats.wallOccludedColumns,
               (unsigned int)stats.frameLoads,
               (unsigned int)stats.frameBytes,
               (unsigned int)stats.maxFrameBytes,
               (unsigned int)stats.orderFNV1a,
               (unsigned int)stats.glowCompanions,
               (unsigned int)stats.glowDraws,
               (unsigned int)stats.glowNearCulled,
               (unsigned int)stats.glowClipCulled,
               (unsigned int)stats.glowSpanRuns,
               (unsigned int)stats.glowPixels,
               (unsigned int)stats.glowWallOccludedColumns,
               (unsigned int)stats.glowFrameLoads,
               (unsigned int)stats.glowFrameBytes,
               (unsigned int)stats.glowMaxFrameBytes,
               (unsigned int)stats.glowDeferred,
               (unsigned int)stats.depthNodes,
               (unsigned int)stats.depthLeaves,
               (unsigned int)stats.depthNodeCulled,
               (unsigned int)stats.depthLines,
               (unsigned int)stats.depthBackfaceCulled,
               (unsigned int)stats.depthClipCulled,
               (unsigned int)stats.depthOccluders,
               (unsigned int)stats.depthSpriteSpans,
               (unsigned int)renderHeapBefore,
               (unsigned int)renderHeapAfter,
               (unsigned int)renderLargestBefore,
               (unsigned int)renderLargestAfter,
               (unsigned int)topologyFNV,
               (unsigned int)(topology ? topology->stateFNV1a : 0U),
               (unsigned int)closedCatalogFNV,
               (unsigned int)(catalog ? catalog->stateFNV1a : 0U),
               legacyClear(render), EspAssetPack_isOpen());
        return;
    }

    if (!Esp32PlatformVideo_present()) {
        printf("[JUNCTIONGLOW] FAILED present frame=%08x\n",
               (unsigned int)after);
        return;
    }

    printf("[JUNCTIONGLOWCAT] READY direct=%08x closed=%08x textures=%u sprites=%u storage=%u dependency=136 directTextureFNV=%08x directSpriteFNV=%08x heapIncrement=%u largest=%u->%u repeatAtomic=yes packClosed=yes\n",
           (unsigned int)directCatalogFNV,
           (unsigned int)closedCatalogFNV,
           catalog ? (unsigned int)catalog->textureCount : 0U,
           catalog ? (unsigned int)catalog->spriteCount : 0U,
           catalog ? (unsigned int)catalog->storageBytes : 0U,
           (unsigned int)textureFNV,
           (unsigned int)directSpriteHash,
           (unsigned int)catalogIncrement,
           (unsigned int)catalogLargestBefore,
           (unsigned int)catalogLargestAfter);
    printf("[JUNCTIONSPRITE] DEPTH nodes=%u leaves=%u nodeCull=%u lines=%u backface=%u clip=%u occluder=%u spriteSpan=%u orderFNV=%08x parity=firstFrame\n",
           (unsigned int)stats.depthNodes,
           (unsigned int)stats.depthLeaves,
           (unsigned int)stats.depthNodeCulled,
           (unsigned int)stats.depthLines,
           (unsigned int)stats.depthBackfaceCulled,
           (unsigned int)stats.depthClipCulled,
           (unsigned int)stats.depthOccluders,
           (unsigned int)stats.depthSpriteSpans,
           (unsigned int)stats.orderFNV1a);
    printf("[JUNCTIONSPRITE] BASE objects=%u bspCandidates=%u bspRejected=%u modes=0:%u/7:%u mode7Pixels=%u draws=%u nearCull=%u clipCull=%u spans=%u pixels=%u wallOccludedCols=%u frames=%u uniqueLogical=%u frameBytes=%u maxFrame=%u preserved=yes\n",
           (unsigned int)stats.objects,
           (unsigned int)stats.bspCandidates,
           (unsigned int)stats.bspRejected,
           (unsigned int)stats.mode0Objects,
           (unsigned int)stats.mode7Objects,
           (unsigned int)stats.mode7Pixels,
           (unsigned int)stats.draws,
           (unsigned int)stats.nearCulled,
           (unsigned int)stats.clipCulled,
           (unsigned int)stats.spanRuns,
           (unsigned int)stats.pixelsDrawn,
           (unsigned int)stats.wallOccludedColumns,
           (unsigned int)stats.frameLoads,
           (unsigned int)stats.uniqueLogical,
           (unsigned int)stats.frameBytes,
           (unsigned int)stats.maxFrameBytes);
    printf("[JUNCTIONGLOW] READY frame=%08x->%08x companions=%u draws=%u nearCull=%u clipCull=%u spans=%u pixels=%u wallOccludedCols=%u frames=%u frameBytes=%u maxFrame=%u packReads=%u heapDelta=0 largestDelta=0 topology=%08x catalog=%08x packClosed=yes presented=1\n",
           (unsigned int)before,
           (unsigned int)after,
           (unsigned int)stats.glowCompanions,
           (unsigned int)stats.glowDraws,
           (unsigned int)stats.glowNearCulled,
           (unsigned int)stats.glowClipCulled,
           (unsigned int)stats.glowSpanRuns,
           (unsigned int)stats.glowPixels,
           (unsigned int)stats.glowWallOccludedColumns,
           (unsigned int)stats.glowFrameLoads,
           (unsigned int)stats.glowFrameBytes,
           (unsigned int)stats.glowMaxFrameBytes,
           (unsigned int)stats.packReads,
           (unsigned int)topologyFNV,
           (unsigned int)closedCatalogFNV);
    printf("[JUNCTIONGLOW] PARK baseBillboards=yes bspVisibleOnly=yes intrinsicMode7=yes glowCompanions=yes glowPending=no depthBspParity=yes noLegacyGraphicsPools=yes noWorldMutation=yes\n");
    probeState.done = 1;
}
