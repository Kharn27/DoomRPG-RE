#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PROJECTION_OVERLAY_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PROJECTION_OVERLAY_H

#include <stdint.h>
#include "esp_native_gameplay_hud_direction.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ARCH-RC7: explicit final read-only composition atop ActionEngine's
 * fire/destructible projection. Order is stable: ActionEngine base, then
 * resource-consumed hiding + monster attack pose (visual); or crate overlay,
 * then consumed link/order suppression (entity). A base failure returns zero
 * without running later overlays. No BSP/topology mutation or allocation.
 * Caller selects this projected view deliberately; raw topology stays raw. */
int EspNativeGameplayProjectionOverlay_getVisualState(
    uint32_t spriteIndex, uint8_t* outVisualState);
int EspNativeGameplayProjectionOverlay_getEntity(
    uint32_t spriteIndex, uint8_t* outType, uint8_t* outSubType,
    uint16_t* outLinkState, uint16_t* outLinkOrder);

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
