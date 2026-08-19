#ifndef DOOMRPG_ESP32_NATIVE_PROJECTED_WALL_BRIDGE_H
#define DOOMRPG_ESP32_NATIVE_PROJECTED_WALL_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;
struct Line_s;

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
 */
int EspNativeProjectedWall_begin(struct Render_s* render, int textureIndex);
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
