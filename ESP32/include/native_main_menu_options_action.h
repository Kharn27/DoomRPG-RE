#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_OPTIONS_ACTION_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_OPTIONS_ACTION_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Execute the real MENU_MAIN -> MENU_MAIN_OPTIONS model transition, then paint
 * the resulting Options model through the bounded ESP32 presentation path.
 * No legacy Render_render() call is allowed here.
 */
int DoomRPG_esp32ActivateMainMenuOptions(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
