#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_START_ACTION_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_START_ACTION_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Execute the real MENU_MAIN Start Game action up to the next bounded ESP32
 * boundary. On a fresh profile this runs MenuSystem_select(), Menu_startGame(),
 * Player_reset() and DoomCanvas_setState(ST_INTRO), loads the real prologue
 * resources, then renders/presents exactly one deterministic intro frame.
 * No active DoomCanvas_run() loop, intro input or gameplay/map load is started.
 */
int DoomRPG_esp32ActivateMainMenuStart(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
