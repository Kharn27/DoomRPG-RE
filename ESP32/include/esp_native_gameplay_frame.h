#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_FRAME_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_FRAME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Render_s;

typedef struct EspNativeGameplayFrameStats_s {
    uint32_t frameBeforeFNV;
    uint32_t worldFrameFNV;
    uint32_t frameAfterFNV;
    uint32_t temporaryHudBytes;
    uint32_t wallDraws;
    uint32_t wallPixels;
    uint32_t planePixels;
    uint32_t spriteDraws;
    uint32_t spritePixels;
    uint32_t glowDraws;
    uint32_t glowPixels;
    uint32_t spritePackReads;
    uint32_t hudPackReads;
    uint32_t hudPixels;
    uint8_t angle;
    uint8_t worldPresented;
    uint8_t finalPresented;
    uint8_t active;
} EspNativeGameplayFrameStats;

/* Recompose one complete current Junction gameplay frame after a committed
 * cardinal view turn. Existing HUD bands are kept in one bounded temporary
 * 12.8 KiB buffer because the historical first-frame world route clears the
 * whole logical framebuffer. The world route may present its walls+planes
 * intermediate; the final present occurs only after sprites/glows, HUD-band
 * restore and compass repaint. No persistent allocation is retained. */
int EspNativeGameplayFrame_renderTurn(
    struct Render_s* render,
    uint8_t angle,
    EspNativeGameplayFrameStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
