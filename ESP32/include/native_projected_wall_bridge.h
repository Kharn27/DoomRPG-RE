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
    uint32_t spanCalls;
    uint32_t pixelsDrawn;
    uint32_t outOfRangeReads;
    int lastTextureIndex;
    uint32_t lastTexelHash;
} EspNativeProjectedWallStats;

void EspNativeProjectedWall_resetStats(void);
void EspNativeProjectedWall_getStats(EspNativeProjectedWallStats* outStats);

int EspNativeProjectedWall_begin(struct Render_s* render, int textureIndex);
void EspNativeProjectedWall_end(void);
void EspNativeProjectedWall_spanMode0(struct Render_s* render,
                                      int x,
                                      int y,
                                      int texelPosition,
                                      int texelStep,
                                      int pixelCount);

#ifdef __cplusplus
}
#endif

#endif
