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
#define EXPECTED_CATALOG_FNV 0x969d5a77U
#define EXPECTED_MODE0_OBJECTS 30U
#define EXPECTED_MODE7_OBJECTS 18U
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
    uint32_t h = 2166136261U;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        h ^= p[i];
        h *= 16777619U;
    }
    return h;
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

static int legacyClear(const Render_t* r) {
    return r != NULL && r->lines == NULL && r->nodes == NULL &&
           r->mapSprites == NULL && r->mediaTexelOffsets == NULL &&
           r->mediaBitShapeOffsets == NULL && r->mediaTexturesIds == NULL &&
           r->mediaSpriteIds == NULL && r->shapeData == NULL &&
           r->mediaTexels == NULL && r->mapTextureTexels == NULL &&
           r->mapSpriteTexels == NULL;
}

static int depthMatchesFirstFrame(const EspNativeJunctionSpriteStats* s) {
    return s != NULL &&
           s->depthNodes == EXPECTED_DEPTH_NODES &&
           s->depthLeaves == EXPECTED_DEPTH_LEAVES &&
           s->depthNodeCulled == EXPECTED_DEPTH_NODE_CULL &&
           s->depthLines == EXPECTED_DEPTH_LINES &&
           s->depthBackfaceCulled == EXPECTED_DEPTH_BACKFACE &&
           s->depthClipCulled == EXPECTED_DEPTH_CLIP &&
           s->depthOccluders == EXPECTED_DEPTH_OCCLUDER &&
           s->depthSpriteSpans == EXPECTED_DEPTH_SPRITE_SPAN;
}

void Esp32JunctionSpriteOverlayProbe_reset(void) {
    memset(&probeState, 0, sizeof(probeState));
}

int Esp32JunctionSpriteOverlayProbe_isDone(void) {
    return probeState.done;
}

void Esp32JunctionSpriteOverlayProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* d = (DoomRPG_t*)doomRpgBase;
    Render_t* render;
    const EspMapSpriteTopologyView* topology;
    const EspNativeGraphicsCatalogView* catalog;
    EspNativeJunctionSpriteStats stats;
    uint32_t before;
    uint32_t after;
    uint32_t topologyFNV;
    uint32_t catalogFNV;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;

    if (probeState.done || probeState.attempted) return;
    if (!Esp32JunctionSpriteCensusProbe_isDone() ||
        !EspNativeFirstFrame_isReady()) {
        return;
    }
    probeState.attempted = 1;

    render = d != NULL ? d->render : NULL;
    topology = EspMapSpriteTopology_view();
    catalog = EspNativeGraphicsCatalog_view();
    before = frameFNV(render);

    if (render == NULL || topology == NULL || catalog == NULL ||
        before != EXPECTED_BASE_FRAME_FNV ||
        topology->stateFNV1a != EXPECTED_TOPOLOGY_FNV ||
        catalog->stateFNV1a != EXPECTED_CATALOG_FNV ||
        !legacyClear(render) || EspAssetPack_isOpen()) {
        printf("[JUNCTIONSPRITE] FAILED boundary frame=%08x topology=%08x catalog=%08x legacyClear=%d pack=%d\n",
               (unsigned int)before,
               (unsigned int)(topology ? topology->stateFNV1a : 0U),
               (unsigned int)(catalog ? catalog->stateFNV1a : 0U),
               legacyClear(render), EspAssetPack_isOpen());
        return;
    }

    topologyFNV = topology->stateFNV1a;
    catalogFNV = catalog->stateFNV1a;
    heapBefore = heap8();
    largestBefore = largest8();
    memset(&stats, 0, sizeof(stats));

    printf("\n=== Doom RPG ESP32-native Junction billboard overlay modes 0+7 ===\n");
    printf("[JUNCTIONSPRITE] CONTRACT logical BSP id is sparse ownership key; physical bitshape resolves through bounded mappings.bin ranges; wall depth reproduces validated compact BSP walk; intrinsic legacy mode0 plus ID136/137/144 mode7 additive RGB565 saturation; animationTime0; spawned glow companions still deferred; no resident legacy graphics pools and no world/entity mutation\n");

    if (!EspNativeJunctionSprite_render(render, &stats)) {
        printf("[JUNCTIONSPRITE] FAILED renderer objects=%u unsupported=%u mode0=%u mode7=%u mode7Pixels=%u draws=%u pixels=%u reads=%u glowDeferred=%u\n",
               (unsigned int)stats.objects,
               (unsigned int)stats.unsupported,
               (unsigned int)stats.mode0Objects,
               (unsigned int)stats.mode7Objects,
               (unsigned int)stats.mode7Pixels,
               (unsigned int)stats.draws,
               (unsigned int)stats.pixelsDrawn,
               (unsigned int)stats.packReads,
               (unsigned int)stats.glowDeferred);
        return;
    }

    heapAfter = heap8();
    largestAfter = largest8();
    after = frameFNV(render);
    topology = EspMapSpriteTopology_view();
    catalog = EspNativeGraphicsCatalog_view();

    if (after == 0U || after == before || stats.objects != 48U ||
        stats.hidden != 0U || stats.unsupported != 0U ||
        stats.mode0Objects != EXPECTED_MODE0_OBJECTS ||
        stats.mode7Objects != EXPECTED_MODE7_OBJECTS ||
        stats.mode7Pixels == 0U || stats.glowDeferred != 9U ||
        stats.draws == 0U || stats.pixelsDrawn == 0U ||
        !depthMatchesFirstFrame(&stats) || heapAfter != heapBefore ||
        largestAfter != largestBefore || topology == NULL || catalog == NULL ||
        topology->stateFNV1a != topologyFNV ||
        catalog->stateFNV1a != catalogFNV ||
        !legacyClear(render) || EspAssetPack_isOpen()) {
        printf("[JUNCTIONSPRITE] FAILED post frame=%08x->%08x objects=%u hidden=%u unsupported=%u modes=%u/%u mode7Pixels=%u glow=%u draws=%u pixels=%u depth=%u/%u/%u/%u/%u/%u/%u/%u heap8=%u->%u largest8=%u->%u topology=%08x/%08x catalog=%08x/%08x legacyClear=%d pack=%d\n",
               (unsigned int)before,
               (unsigned int)after,
               (unsigned int)stats.objects,
               (unsigned int)stats.hidden,
               (unsigned int)stats.unsupported,
               (unsigned int)stats.mode0Objects,
               (unsigned int)stats.mode7Objects,
               (unsigned int)stats.mode7Pixels,
               (unsigned int)stats.glowDeferred,
               (unsigned int)stats.draws,
               (unsigned int)stats.pixelsDrawn,
               (unsigned int)stats.depthNodes,
               (unsigned int)stats.depthLeaves,
               (unsigned int)stats.depthNodeCulled,
               (unsigned int)stats.depthLines,
               (unsigned int)stats.depthBackfaceCulled,
               (unsigned int)stats.depthClipCulled,
               (unsigned int)stats.depthOccluders,
               (unsigned int)stats.depthSpriteSpans,
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (unsigned int)largestBefore,
               (unsigned int)largestAfter,
               (unsigned int)topologyFNV,
               (unsigned int)(topology ? topology->stateFNV1a : 0U),
               (unsigned int)catalogFNV,
               (unsigned int)(catalog ? catalog->stateFNV1a : 0U),
               legacyClear(render), EspAssetPack_isOpen());
        return;
    }

    if (!Esp32PlatformVideo_present()) {
        printf("[JUNCTIONSPRITE] FAILED present frame=%08x\n",
               (unsigned int)after);
        return;
    }

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
    printf("[JUNCTIONSPRITE] READY frame=%08x->%08x objects=%u modes=0:%u/7:%u mode7Pixels=%u draws=%u nearCull=%u clipCull=%u spans=%u pixels=%u wallOccludedCols=%u frames=%u uniqueLogical=%u frameBytes=%u maxFrame=%u packReads=%u glowDeferred=%u heapDelta=0 largestDelta=0 legacyRenderStable=yes topology=%08x catalog=%08x packClosed=yes presented=1\n",
           (unsigned int)before,
           (unsigned int)after,
           (unsigned int)stats.objects,
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
           (unsigned int)stats.maxFrameBytes,
           (unsigned int)stats.packReads,
           (unsigned int)stats.glowDeferred,
           (unsigned int)topologyFNV,
           (unsigned int)catalogFNV);
    printf("[JUNCTIONSPRITE] PARK baseBillboards=yes intrinsicMode7=yes depthBspParity=yes glowPending=yes noLegacyGraphicsPools=yes noWorldMutation=yes\n");
    probeState.done = 1;
}
