#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_OPTIONS_ACTION_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_OPTIONS_ACTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Execute the real MENU_MAIN -> MENU_MAIN_OPTIONS model transition, then paint
 * the resulting Options model through the bounded ESP32 presentation path.
 * No legacy Render_render() call is allowed here.
 */
int DoomRPG_esp32ActivateMainMenuOptions(struct DoomRPG_s* doomRpg,
                                         uint32_t* finalFramebufferFNV);

/* Repaint only the shared right-hand rail; selectedItem is 0 (Back) or
 * 1 (Video brightness), armed is the first-tap visual confirmation state.
 * Input/Sound remain disabled, and the left logo remains untouched. */
int DoomRPG_esp32PaintMainMenuOptionsDashboardSelection(
    struct DoomRPG_s* doomRpg,
    int selectedItem,
    int armed,
    uint32_t* framebufferFNV);

/* Compatibility entry used for initial Options composition. */
int DoomRPG_esp32PaintMainMenuOptionsDashboard(
    struct DoomRPG_s* doomRpg,
    int backArmed,
    uint32_t* framebufferFNV);

#ifdef __cplusplus
}
#endif

#endif
