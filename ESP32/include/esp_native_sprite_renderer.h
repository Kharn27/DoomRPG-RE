#ifndef DOOMRPG_ESP32_NATIVE_SPRITE_RENDERER_H
#define DOOMRPG_ESP32_NATIVE_SPRITE_RENDERER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

typedef struct EspNativeSpriteStats_s {
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
} EspNativeSpriteStats;

/*
 * Map-generic native sprite renderer.
 *
 * BSP visibility and wall-column depth come from EspNativeBspVisibility.
 * Base billboards preserve the recovered render modes 0/7; renderer-owned glow
 * companions are emitted in the same sorted view-sprite sequence. Logical IDs
 * remain sparse ownership keys while physical bitshape IDs and packed texels
 * are resolved through bounded PAK reads. Render projection scratch is restored
 * exactly before return.
 */
int EspNativeSpriteRenderer_render(struct Render_s* render,
                                   EspNativeSpriteStats* outStats);


typedef struct EspNativeSpriteTransient_s {
    int16_t worldX;
    int16_t worldY;
} EspNativeSpriteTransient;

/* Bounded transient world billboards composed by the same native sprite
 * renderer after immutable map sprites. Gameplay owns lifetime/timing and the
 * caller-owned batch storage; renderTurn() consumes the pointer synchronously.
 * The single-item helper remains for crate traps and other one-shot effects. */
int EspNativeSpriteRenderer_armTransient(uint16_t logical,
                                         uint8_t animation,
                                         int16_t worldX,
                                         int16_t worldY);
int EspNativeSpriteRenderer_armTransientBatch(
    uint16_t logical,
    uint8_t animation,
    const EspNativeSpriteTransient* items,
    uint8_t count);
void EspNativeSpriteRenderer_clearTransient(void);

#ifdef __cplusplus
}
#endif

#endif
