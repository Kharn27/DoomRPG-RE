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
    uint32_t bspCandidates;
    uint32_t bspRejected;
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
    uint32_t glowCompanions;
    uint32_t glowDraws;
    uint32_t glowNearCulled;
    uint32_t glowClipCulled;
    uint32_t glowSpanRuns;
    uint32_t glowPixels;
    uint32_t glowWallOccludedColumns;
    uint32_t glowFrameLoads;
    uint32_t glowFrameBytes;
    uint32_t glowMaxFrameBytes;
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

/*
 * Render the validated Junction BSP-visible sprite family onto the existing
 * walls+planes framebuffer. Base billboards keep legacy render modes 0/7.
 * Renderer-owned glow companions are emitted immediately after their parent
 * in the same sorted view-sprite sequence: 135/140 -> 136 mode7 and
 * 131 -> 144 mode7. The sparse catalog must already have explicit dependency
 * closure for any companion used by the map.
 *
 * Visibility and wall column depth come from EspNativeBspVisibility, which
 * reproduces the stateful compact BSP walk without retaining legacy map or
 * graphics pools. Logical IDs remain sparse ownership keys; physical bitshape
 * IDs and packed texels are resolved through bounded PAK reads. All Render
 * projection scratch is restored exactly before return.
 */
int EspNativeJunctionSprite_render(struct Render_s* render,
                                   EspNativeJunctionSpriteStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
