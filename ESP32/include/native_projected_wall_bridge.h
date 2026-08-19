#ifndef DOOMRPG_ESP32_NATIVE_PROJECTED_WALL_BRIDGE_H
#define DOOMRPG_ESP32_NATIVE_PROJECTED_WALL_BRIDGE_H

#include <stdint.h>

/* src/Render.c hard-codes FIXED_VERSION=1. The extracted ESP32 wall-span math
 * must use the same fixed-point branch so its framebuffer remains bit-identical.
 */
#ifndef FIXED_VERSION
#define FIXED_VERSION 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;
struct Line_s;
struct EspNativeWallFrame_s;

typedef struct EspNativeProjectedWallStats_s {
    uint32_t beginCalls;
    uint32_t endCalls;
    uint32_t boundBytes;
    uint32_t spanCalls;
    uint32_t pixelsDrawn;
    uint32_t rangeErrors;
    uint32_t legacyPointerViolations;
    uint32_t mappingOffsetViolations;
    int lastTextureIndex;
    int lastPaletteOffset;
    int sourceTexelOffset;
    uint32_t lastTexelHash;
} EspNativeProjectedWallStats;

void EspNativeProjectedWall_resetStats(void);
void EspNativeProjectedWall_getStats(EspNativeProjectedWallStats* outStats);

/* Acquire one bounded GFXRM wall frame. Unlike the previous compatibility
 * bridge, this never aliases Render.mediaTexels and never rewrites mappings.
 * This entry point owns/releases the acquired frame and remains the default for
 * standalone projected-wall probes.
 */
int EspNativeProjectedWall_begin(struct Render_s* render, int textureIndex);

/* Activate an already-resident wall frame without taking ownership of its
 * payload. This is the cache-facing path: EspNativeProjectedWall_end() clears
 * only the borrowed view, while the caller/cache retains the 2 KB payload.
 */
int EspNativeProjectedWall_beginBorrowed(
    struct Render_s* render,
    const struct EspNativeWallFrame_s* borrowedFrame);

void EspNativeProjectedWall_end(void);
int EspNativeProjectedWall_isActive(void);

/* First ESP32-native projected wall primitive. Projection/clipping remain in
 * the original Render helpers; this function preserves Render_drawWallSpans()
 * mode-0 column geometry while sampling the active bounded GFXRM frame directly.
 */
int EspNativeProjectedWall_drawWallSpans(struct Render_s* render,
                                         struct Line_s* projectedLine);

#ifdef __cplusplus
}
#endif

#endif
