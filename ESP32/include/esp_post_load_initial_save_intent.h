#ifndef DOOMRPG_ESP32_POST_LOAD_INITIAL_SAVE_INTENT_H
#define DOOMRPG_ESP32_POST_LOAD_INITIAL_SAVE_INTENT_H

#include <stdint.h>

#include "esp_player_view_state.h"
#include "esp_post_load_weapon_select_state.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ESP_POST_LOAD_SAVE_COMPONENT_CONFIG = 0x01U,
    ESP_POST_LOAD_SAVE_COMPONENT_PLAYER2 = 0x02U,
    ESP_POST_LOAD_SAVE_COMPONENT_WORLD = 0x04U,
    ESP_POST_LOAD_SAVE_COMPONENT_PLAYER_ROUTE = 0x08U,
    ESP_POST_LOAD_SAVE_COMPONENT_ALL = 0x0fU
};

typedef enum EspPostLoadInitialSaveIntentStatus_e {
    ESP_POST_LOAD_INITIAL_SAVE_INTENT_INVALID = 0,
    ESP_POST_LOAD_INITIAL_SAVE_INTENT_WEAPON_INVALID = 1,
    ESP_POST_LOAD_INITIAL_SAVE_INTENT_VIEW_INVALID = 2,
    ESP_POST_LOAD_INITIAL_SAVE_INTENT_UNSUPPORTED_CONTEXT = 3,
    ESP_POST_LOAD_INITIAL_SAVE_INTENT_WORLD_NOT_READY = 4,
    ESP_POST_LOAD_INITIAL_SAVE_INTENT_LOADED_CONTEXT_DEFERRED = 5,
    ESP_POST_LOAD_INITIAL_SAVE_INTENT_ALREADY_ACTIVE = 6,
    ESP_POST_LOAD_INITIAL_SAVE_INTENT_OK = 7
} EspPostLoadInitialSaveIntentStatus;

/*
 * Pointer-free semantic owner for the exact fresh-Junction caller decision:
 *
 *   if ((loadMapID != MAP_END_GAME) && (game->isLoaded == false)) {
 *       Game_saveState(game, loadMapID, viewX, viewY, viewAngle, false);
 *   }
 *
 * This milestone captures only the save request/preflight. It deliberately does
 * not reproduce the legacy save UI, present the framebuffer, or write Config,
 * Player2, World or Player files. Those persistence formats depend on native
 * player/world ownership that is not complete yet.
 */
typedef struct EspPostLoadInitialSaveIntentState_s {
    int32_t viewX;
    int32_t viewY;
    int32_t viewAngle;

    uint8_t mapId;
    uint8_t isLoadedBefore;
    uint8_t saveMode;
    uint8_t saveRequired;
    uint8_t componentMask;
    uint8_t persistenceDeferred;
    uint8_t presentationDeferred;
    uint8_t active;
    uint8_t reserved[4];
} EspPostLoadInitialSaveIntentState;

void EspPostLoadInitialSaveIntent_reset(void);
int EspPostLoadInitialSaveIntent_isReady(void);
const EspPostLoadInitialSaveIntentState* EspPostLoadInitialSaveIntent_view(void);

/*
 * Pure translation of the exact fresh-Junction conditional save callsite.
 * isLoadedBefore is supplied as a scalar by the future native load-state owner;
 * the temporary probe samples legacy Game.isLoaded read-only.
 */
EspPostLoadInitialSaveIntentStatus EspPostLoadInitialSaveIntent_prepare(
    const EspPostLoadWeaponSelectState* weaponSelect,
    const EspPlayerViewState* playerView,
    uint8_t isLoadedBefore,
    EspPostLoadInitialSaveIntentState* outState);

/* Park the intent only. No save I/O, UI, presentation or legacy mutation. */
EspPostLoadInitialSaveIntentStatus EspPostLoadInitialSaveIntent_route(
    uint8_t isLoadedBefore);

#ifdef __cplusplus
}
#endif

#endif
