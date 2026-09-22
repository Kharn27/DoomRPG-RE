#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_AUTOMAP_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_AUTOMAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspNativeGameplayAutomapStats_s {
    uint32_t frameFNV1a;
    uint16_t visitedCells;
    uint16_t revealedLines;
    uint16_t revealedSpecials;
    uint16_t visibleItems;
    uint16_t pixelsWritten;
    uint8_t scale;
    uint8_t mapX;
    uint8_t mapY;
    uint8_t playerX;
    uint8_t playerY;
    uint8_t angle;
    uint8_t presented;
    uint8_t reserved[2];
} EspNativeGameplayAutomapStats;

/*
 * Recovered DoomCanvas_uncoverAutomap semantic mutation. It marks the current
 * 3x3 neighborhood visited, excluding secret neighbor cells while always
 * allowing the current cell. Like the original loop, row/column 31 are not
 * synthesized.
 */
int EspNativeGameplayAutomap_uncoverAt(int32_t worldX,
                                       int32_t worldY,
                                       uint16_t* outMutated);

/*
 * Render the compact native Automap directly into the shared 160x120 RGB565
 * framebuffer. No second framebuffer, legacy Render/Game entities, shapeData
 * or mediaTexels are used. The map geometry is 32x32 at scale 3, centered as
 * the original 120-pixel-short-side layout.
 */
int EspNativeGameplayAutomap_render(int32_t worldX,
                                    int32_t worldY,
                                    uint8_t angle,
                                    EspNativeGameplayAutomapStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
