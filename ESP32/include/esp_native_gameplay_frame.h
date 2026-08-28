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

/*
 * Recompose one complete current resident-map gameplay frame after a committed
 * cardinal MOVE/TURN, or while priming the gameplay session cache. Map identity
 * and pose come from EspPlayerView; no fixed Junction/Entrance fingerprint is a
 * production precondition.
 *
 * The world phase is viewport-only: pixels outside 160x80@0,20 are preserved,
 * there is no intermediate physical presentation and no temporary HUD save.
 * Map sprites/glows, the idle first-person weapon and the bounded HUD direction
 * footprint are then repainted before exactly one complete-frame presentation.
 *
 * Storage/cache policy is owned by EspNativeGameplaySession. This compositor
 * merely takes logical PAK leases through its world/sprite/weapon/HUD children,
 * so it remains valid in normal or resident PAK mode.
 */
int EspNativeGameplayFrame_renderTurn(
    struct Render_s* render,
    uint8_t angle,
    EspNativeGameplayFrameStats* outStats);

#ifdef __cplusplus
}
#endif

#endif
