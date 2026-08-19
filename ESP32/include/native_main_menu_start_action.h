#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_START_ACTION_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_START_ACTION_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Execute the real MENU_MAIN Start Game action up to the engine's natural
 * first-game boundary. On a fresh profile this runs MenuSystem_select(),
 * Menu_startGame(), Player_reset() and DoomCanvas_setState(ST_INTRO), loading
 * the real prologue text. The ESP32 main loop intentionally does not advance
 * the intro yet; that is the next bounded milestone.
 */
int DoomRPG_esp32ActivateMainMenuStart(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
