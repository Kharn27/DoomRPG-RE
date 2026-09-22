#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_LOAD_ACTION_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_LOAD_ACTION_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* Load the native checkpoint selected from MENU_MAIN. Missing/invalid saves
 * leave the menu active; a successful load arms the resumed gameplay session. */
int DoomRPG_esp32ActivateMainMenuLoad(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
