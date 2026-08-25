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
    uint32_t viewportBeforeFNV;
    uint32_t viewportAfterWorldFNV;
    uint32_t viewportAfterSpritesFNV;
    uint32_t hudBandsBeforeFNV;
    uint32_t hudBandsRestoredFNV;
    uint32_t hudBandsAfterFNV;
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
    uint8_t intermediatePresentSuppressed;
    uint8_t finalPresented;
    uint8_t active;
} EspNativeGameplayFrameStats;

/* Recompose one complete current Junction gameplay frame after a committed
 * cardinal view turn. The historical first-frame renderer is still reused for
 * the world pixels, but its intermediate presentation is suppressed: the
 * existing HUD bands are restored from one bounded temporary 12.8 KiB buffer,
 * the small compass panel is repainted, then exactly one final presentation is
 * issued. Phase FNVs make viewport/HUD round-trip drift explicit while this
 * bridge is being reduced toward a permanent viewport-only runtime renderer.
 * No persistent allocation is retained. */
int EspNativeGameplayFrame_renderTurn(
    struct Render_s* render,
    uint8_t angle,
    EspNativeGameplayFrameStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
