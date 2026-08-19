#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_TOUCH_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_TOUCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Capture the four tiny framebuffer patches that live underneath the possible
 * hand-cursor positions. Call after the scaled logo is composed and before menu
 * rows are drawn.
 */
int DoomRPG_esp32MainMenuTouchPrepare(struct DoomRPG_s* doomRpg);

/* Arm touch handling after the initial fitted MENU_MAIN frame is fully composed.
 * The initial framebuffer hash becomes the deterministic selected-item-0 hash.
 */
int DoomRPG_esp32MainMenuTouchActivate(struct DoomRPG_s* doomRpg,
                                       uint32_t initialFramebufferFNV);

int DoomRPG_esp32MainMenuTouchIsActive(void);

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
