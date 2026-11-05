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

/* Native dashboard painter: four MENU_MAIN labels or three Options entries;
 * enabledMask keeps deferred cards visible but deliberately subdued. This is
 * the common presentation primitive used by MENU_MAIN and its Options child.
 */
int DoomRPG_esp32PaintMenuDashboardCards(
    struct DoomRPG_s* doomRpg,
    const char* const labels[],
    int selectedIndex,
    int armed,
    uint8_t enabledMask,
    uint32_t* framebufferFNV);

/* VIDEO intermediary: four cards Back/Luminosite/Orientation/Theme.
 * Identical geometry in its renderer and the touch frontend. */
int DoomRPG_esp32PaintVideoMenuCards(
    struct DoomRPG_s* doomRpg,
    int selectedIndex, int armed, uint8_t enabledMask,
    uint32_t* framebufferFNV);

/* Parent Options -> Luminosité child: right rail only; 5 cards with
 * persistent one-tap selection highlighting. Never touches logo or world. */
int DoomRPG_esp32PaintBrightnessPresetCards(
    struct DoomRPG_s* doomRpg,
    uint8_t activePercent,
    uint32_t* framebufferFNV);

/* Same bounded right-rail layout as brightness: Back/Normal/Inverse. */
int DoomRPG_esp32PaintOrientationPresetCards(
    struct DoomRPG_s* doomRpg,
    uint8_t activeOrientation,
    uint32_t* framebufferFNV);

/* Theme choices Back/Classic/Inferno, same 54x20 touch layout. */
int DoomRPG_esp32PaintThemePresetCards(
    struct DoomRPG_s* doomRpg,
    uint8_t activeTheme,
    uint32_t* framebufferFNV);

#ifdef __cplusplus
}
#endif

#endif
