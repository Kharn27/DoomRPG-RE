#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_MODEL_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/*
 * Transitional compact owner for the still-retained MenuSystem_t model.
 * It deliberately supports only the bounded pre-game models needed by the
 * finger-first ESP32 menu. Presentation and action dispatch live elsewhere.
 */
int DoomRPG_esp32MainMenuModelEnter(struct DoomRPG_s* doomRpg, int menuId);

/* Pure fixed MENU_MAIN model construction for boot-time composition.
 * It does not change DoomCanvas state or present a frame.
 */
int DoomRPG_esp32MainMenuModelBuildMain(struct DoomRPG_s* doomRpg);

/* Close MENU_MAIN after Menu_startGame() has established its next canvas state.
 * This mirrors only the small model bookkeeping historically hidden in
 * MenuSystem_setMenu(MENU_NONE); map/media ownership is not reintroduced here.
 */
int DoomRPG_esp32MainMenuModelLeave(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
