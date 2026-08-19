#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_TOUCH_LAYOUT_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_TOUCH_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Paint the real MENU_MAIN model on an opaque black 160x120 framebuffer using
 * the validated ESP32 logo/font/hand geometry, prepare cursor patches, arm the
 * main-menu touch gate and present once. No BSP, wall or sprite replay occurs.
 *
 * The caller must already have the real MENU_MAIN model selected at item 0.
 */
int DoomRPG_esp32RepaintOpaqueMainMenu(struct DoomRPG_s* doomRpg,
                                       uint32_t* finalFramebufferFNV);

#ifdef __cplusplus
}
#endif

#endif
