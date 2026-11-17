#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PROJECTION_OVERLAY_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PROJECTION_OVERLAY_H

#include <stdint.h>
#include "esp_native_gameplay_hud_direction.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ARCH-RC5: after the compass owner has completed and closed its PAK lease,
 * refresh the projected live-player bottom HUD only when its immutable view is
 * dirty. No additional framebuffer, heap owner, gameplay mutation, or turn.
 * Zero signals the same fail-closed rendering failure as the former --wrap. */
int EspNativeGameplayProjectionOverlay_afterHudDirectionRender(
    uint8_t angle,
    EspNativeGameplayHudDirectionStats* outStats);

#ifdef __cplusplus
}
#endif
#endif
