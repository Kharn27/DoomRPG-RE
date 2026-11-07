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

/* Root Options: BACK / VIDEO / disabled SOUND, 3 spacious 54x20 cards.
 * The immutable MENU_MAIN_OPTIONS model and logo stage stay unchanged. */
int DoomRPG_esp32PaintMainMenuOptionsDashboardSelection(
    struct DoomRPG_s* doomRpg,
    int selectedItem,
    int armed,
    uint32_t* framebufferFNV);

/* VIDEO middle layer: BACK/LUMINOSITE/ORIENTATION/THEME (4 cards).
 * Model remains MENU_MAIN_OPTIONS; only the native UI presentation changes. */
int DoomRPG_esp32PaintMainMenuVideoDashboardSelection(
    struct DoomRPG_s* doomRpg, int selectedItem, int armed,
    uint32_t* framebufferFNV);

/* Paint five bounded preset cards into the same right-hand rail while
 * keeping the immutable native Options model and the left logo intact. */
int DoomRPG_esp32PaintMainMenuBrightnessDashboard(
    struct DoomRPG_s* doomRpg,
    uint32_t* framebufferFNV);

/* Orientation child shares the protected right rail, with 3 direct-tap
 * choices (Back, Normal, Inverse); source framebuffer stays unchanged. */
int DoomRPG_esp32PaintMainMenuOrientationDashboard(
    struct DoomRPG_s* doomRpg, uint32_t* framebufferFNV);

/* Theme child: Back/Classic/Inferno; same fixed 54x20 rail. */
int DoomRPG_esp32PaintMainMenuThemeDashboard(
    struct DoomRPG_s* doomRpg,
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
