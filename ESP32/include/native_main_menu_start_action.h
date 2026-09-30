#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_START_ACTION_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_START_ACTION_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Release legacy/menu-only runtime before a native map rebuild. Shared menu
 * images stay resident for later menu returns. */
int DoomRPG_esp32ReleaseMainMenuMemory(struct DoomRPG_s* doomRpg);

/* Execute the MENU_MAIN Start Game action up to the next bounded ESP32
 * boundary. On a fresh profile the native menu model composes the selection
 * directly, then the retained narrow Menu_startGame(new) behavior performs
 * Player_reset() and DoomCanvas_setState(ST_INTRO), loads the real prologue
 * resources, and renders/presents exactly one deterministic intro frame.
 * No desktop-wide Menu_select()/MenuSystem_select() dispatch is used.
 */
int DoomRPG_esp32ActivateMainMenuStart(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
