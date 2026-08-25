#ifndef DOOMRPG_ESP32_NATIVE_JUNCTION_SPRITE_RENDERER_H
#define DOOMRPG_ESP32_NATIVE_JUNCTION_SPRITE_RENDERER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

typedef struct EspNativeJunctionSpriteStats_s {
    uint32_t objects;
    uint32_t hidden;
    uint32_t unsupported;
    uint32_t mode0Objects;
    uint32_t mode7Objects;
    uint32_t nearCulled;
    uint32_t clipCulled;
    uint32_t draws;
    uint32_t spanRuns;
    uint32_t pixelsDrawn;
    uint32_t mode7Pixels;
    uint32_t wallOccludedColumns;
    uint32_t frameLoads;
    uint32_t uniqueLogical;
    uint32_t frameBytes;
    uint32_t maxFrameBytes;
    uint32_t packReads;
    uint32_t glowDeferred;
    uint32_t depthNodes;
    uint32_t depthLeaves;
    uint32_t depthNodeCulled;
    uint32_t depthLines;
    uint32_t depthBackfaceCulled;
    uint32_t depthClipCulled;
    uint32_t depthOccluders;
    uint32_t depthSpriteSpans;
    uint32_t orderFNV1a;
} EspNativeJunctionSpriteStats;

/* Render the currently validated Junction family onto the existing native
 * walls+planes framebuffer. Supported here: visible standard billboards,
 * legacy intrinsic render modes 0 and 7, animation time 0. Logical IDs remain
 * sparse ownership keys; physical bitshape IDs are resolved by bounded PAK
 * range reads. Mode 7 follows legacy RGB565 additive saturation exactly.
 * Glow companion sprites spawned by IDs 135/140/131 remain separately deferred.
 *
 * Wall depth is reconstructed through the same compact BSP walk used by the
 * validated first native frame, not by scanning map-wide lines. The call
 * temporarily borrows Render projection scratch and restores it exactly before
 * returning. It never installs legacy runtime graphics pools.
 */
int EspNativeJunctionSprite_render(struct Render_s* render,
                                   EspNativeJunctionSpriteStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
