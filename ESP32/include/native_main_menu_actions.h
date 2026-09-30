#ifndef DOOMRPG_ESP32_NATIVE_MAIN_MENU_ACTIONS_H
#define DOOMRPG_ESP32_NATIVE_MAIN_MENU_ACTIONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

enum {
    DOOMRPG_ESP32_MAIN_MENU_ACTION_START = 0,
    DOOMRPG_ESP32_MAIN_MENU_ACTION_LOAD = 1,
    DOOMRPG_ESP32_MAIN_MENU_ACTION_OPTIONS = 2,
    DOOMRPG_ESP32_MAIN_MENU_ACTION_HELP = 3
};

typedef enum DoomRpgEsp32MainMenuDispatchResult_e {
    DOOMRPG_ESP32_MAIN_MENU_DISPATCH_FAILED = 0,
    DOOMRPG_ESP32_MAIN_MENU_DISPATCH_STAY_MAIN = 1,
    DOOMRPG_ESP32_MAIN_MENU_DISPATCH_TRANSITIONED = 2
} DoomRpgEsp32MainMenuDispatchResult;

/* Semantic dispatcher for the four finger-first MENU_MAIN cards. Card layout
 * and legacy menu row numbering never leak into the action implementations.
 */
DoomRpgEsp32MainMenuDispatchResult
DoomRPG_esp32MainMenuDispatchConfirmed(struct DoomRPG_s* doomRpg,
                                       int action);

/* Explicit child -> MENU_MAIN Back action for the bounded pre-game menu domain.
 * Only HELP and OPTIONS are valid children in this milestone. The caller owns
 * child-specific touch/frame preconditions; this function owns semantic model
 * transition, legacy-equivalent Back sound, opaque repaint and touch re-arm.
 */
int DoomRPG_esp32MainMenuReturnToMain(struct DoomRPG_s* doomRpg,
                                      int expectedChildMenu,
                                      const char* source,
                                      uint32_t* finalFramebufferFNV);

#ifdef __cplusplus
}
#endif

#endif
