#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_TOUCH_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_TOUCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Prepare the four dashboard hit zones. No framebuffer underlay is captured:
 * selection feedback repaints the bounded dashboard directly.
 */
int DoomRPG_esp32MainMenuTouchPrepare(struct DoomRPG_s* doomRpg);

/* Arm touch handling after the initial MENU_MAIN frame is fully composed. */
int DoomRPG_esp32MainMenuTouchActivate(struct DoomRPG_s* doomRpg,
                                       uint32_t initialFramebufferFNV);

int DoomRPG_esp32MainMenuTouchIsActive(void);

/* First tap on the already-selected boot card needs visible feedback too. */
int DoomRPG_esp32MainMenuTouchArmSelected(int itemIndex);

/* Runtime framebuffer witness for the selected card. */
uint32_t DoomRPG_esp32MainMenuSelectionFramebufferFNV(int itemIndex);

/* Rebase deterministic selection witnesses after a bounded in-place status
 * paint such as the visible "NO SAVE" response.
 */
void DoomRPG_esp32MainMenuTouchRebaseFrame(int selectedIndex,
                                           uint32_t framebufferFNV);

/* Signature matches PlatformTapCallback. Physical coordinates are the calibrated
 * 320x240 landscape CYD coordinates emitted by PlatformInput.
 */
void DoomRPG_esp32MainMenuTouchOnTap(int16_t screenX,
                                     int16_t screenY,
                                     uint16_t pressure,
                                     uint16_t rawX,
                                     uint16_t rawY);

#ifdef __cplusplus
}
#endif

#endif
