#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_LOAD_ACTION_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_LOAD_ACTION_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

typedef enum DoomRpgEsp32MainMenuLoadResult_e {
    DOOMRPG_ESP32_MAIN_MENU_LOAD_FATAL = 0,
    DOOMRPG_ESP32_MAIN_MENU_LOAD_NO_SAVE = 1,
    DOOMRPG_ESP32_MAIN_MENU_LOAD_RECOVERED = 2,
    DOOMRPG_ESP32_MAIN_MENU_LOAD_TRANSITIONED = 3
} DoomRpgEsp32MainMenuLoadResult;

/* Typed checkpoint result. NO_SAVE is normal user feedback, RECOVERED means a
 * failed restore returned to a fully repainted/rearmed MENU_MAIN, TRANSITIONED
 * owns the resumed gameplay session, and FATAL requires caller recovery.
 */
DoomRpgEsp32MainMenuLoadResult
DoomRPG_esp32ActivateMainMenuLoad(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
