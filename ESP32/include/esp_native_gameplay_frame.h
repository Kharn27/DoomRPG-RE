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
    uint32_t worldMicros;
    uint32_t spriteMicros;
    uint32_t hudMicros;
    uint32_t presentMicros;
    uint32_t totalMicros;
    uint8_t angle;
    uint8_t worldRouteNoPresent;
    uint8_t finalPresented;
    uint8_t active;
} EspNativeGameplayFrameStats;

typedef char EspNativeGameplayFrameStats_must_be_104_bytes[
    sizeof(EspNativeGameplayFrameStats) == 104U ? 1 : -1];

/* Recompose one complete current Junction gameplay frame after a committed
 * cardinal MOVE/TURN. The world phase now uses a viewport-only route: pixels
 * outside 160x80@0,20 are preserved in place, no historical intermediate
 * presentation exists, and no temporary HUD save buffer is allocated. The
 * bounded compass footprint is then repainted for the current angle before
 * exactly one final complete-frame presentation is issued.
 *
 * Timing witnesses cover the gameplay compositor only (world/sprites/HUD/final
 * present); they intentionally exclude the separate input feedback hold and its
 * feedback/restore presentations. */
int EspNativeGameplayFrame_renderTurn(
    struct Render_s* render,
    uint8_t angle,
    EspNativeGameplayFrameStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
