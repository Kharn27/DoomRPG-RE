#ifndef DOOMRPG_ESP32_NATIVE_PROJECTED_WALL_BRIDGE_H
#define DOOMRPG_ESP32_NATIVE_PROJECTED_WALL_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

typedef struct EspNativeProjectedWallStats_s {
    uint32_t beginCalls;
    uint32_t endCalls;
    uint32_t boundBytes;
    int lastTextureIndex;
    int lastPaletteOffset;
    int originalTexelOffset;
    uint32_t lastTexelHash;
} EspNativeProjectedWallStats;

void EspNativeProjectedWall_resetStats(void);
void EspNativeProjectedWall_getStats(EspNativeProjectedWallStats* outStats);

/* Transitional proof bridge: acquire one bounded GFXRM wall frame, rebase that
 * texture's logical texel offset to zero, and expose only the 2 KB frame through
 * Render.mediaTexels while the unchanged legacy wall span path executes.
 * EspNativeProjectedWall_end() restores both fields immediately afterwards.
 */
int EspNativeProjectedWall_begin(struct Render_s* render, int textureIndex);
void EspNativeProjectedWall_end(void);

#ifdef __cplusplus
}
#endif

#endif
