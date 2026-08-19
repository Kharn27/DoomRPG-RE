#ifndef DOOMRPG_ESP32_NATIVE_PROJECTED_SPRITE_RENDERER_H
#define DOOMRPG_ESP32_NATIVE_PROJECTED_SPRITE_RENDERER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;
struct Sprite_s;

typedef struct EspNativeProjectedSpriteStats_s {
    uint32_t objectCalls;
    uint32_t hiddenObjects;
    uint32_t lightObjectsSkipped;
    uint32_t entityObjectsUnsupported;
    uint32_t resolvedDrawCalls;
    uint32_t spriteFrameRequests;
    uint32_t wallBackedRequests;
    uint32_t uniqueSpriteFrames;
    uint32_t repeatedSpriteFrames;
    uint32_t requestHash;
    uint32_t nearCulled;
    uint32_t backfaceCulled;
    uint32_t clipCulled;
    uint32_t spanCalls;
    uint32_t pixelsDrawn;
    uint32_t rangeErrors;
    uint32_t legacyPointerViolations;
    uint32_t legacyShapeViolations;
    uint32_t mappingViolations;
    uint32_t unsupportedFlagPaths;
    uint32_t unsupportedRenderModes;
    uint32_t maxFrameBytes;
} EspNativeProjectedSpriteStats;

void EspNativeProjectedSprite_resetStats(void);
void EspNativeProjectedSprite_getStats(EspNativeProjectedSpriteStats* outStats);

/* Draw one Sprite_t from the original BSP-produced viewSprites list.
 * The implementation preserves the original media-id/animation resolution for
 * map sprites without linked gameplay entities, but loads bitshape + texels on
 * demand through GFXRM instead of shapeData/mediaTexels.
 */
int EspNativeProjectedSprite_drawObject(struct Render_s* render,
                                        struct Sprite_s* sprite,
                                        int objectIndex,
                                        int renderFloorCeilingTextures);

#ifdef __cplusplus
}
#endif

#endif
