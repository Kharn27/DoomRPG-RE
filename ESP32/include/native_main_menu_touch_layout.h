#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_TOUCH_LAYOUT_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_TOUCH_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Paint the real MENU_MAIN model as the opaque finger-first 2x2 dashboard,
 * prepare touch zones, arm the main-menu tap gate and present once. No BSP,
 * wall or sprite replay occurs. The caller must already have MENU_MAIN selected
 * at item 0.
 */
int DoomRPG_esp32RepaintOpaqueMainMenu(struct DoomRPG_s* doomRpg,
                                       uint32_t* finalFramebufferFNV);

/* Repaint only the bounded dashboard band (y=64..119) for selection feedback.
 * selectedIndex owns the amber focus; armed!=0 upgrades that card to the bright
 * first-tap state. This is allocation-free and leaves the logo untouched.
 */
int DoomRPG_esp32MainMenuPaintDashboardSelection(
    struct DoomRPG_s* doomRpg,
    int selectedIndex,
    int armed,
    uint32_t* framebufferFNV);

/* Shared 2x2 Doom-tech dashboard painter. labels must expose four entries;
 * enabledMask keeps deferred cards visible but deliberately subdued. This is
 * the common presentation primitive used by MENU_MAIN and its Options child.
 */
int DoomRPG_esp32PaintMenuDashboardCards(
    struct DoomRPG_s* doomRpg,
    const char* const labels[4],
    int selectedIndex,
    int armed,
    uint8_t enabledMask,
    uint32_t* framebufferFNV);

#ifdef __cplusplus
}
#endif

#endif
