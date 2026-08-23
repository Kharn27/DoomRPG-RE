#ifndef DOOMRPG_ESP32_PLAYER_FRESH_MAP_STATE_H
#define DOOMRPG_ESP32_PLAYER_FRESH_MAP_STATE_H

#include <stdint.h>

#include "esp_hud_refresh_state.h"
#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspPlayerFreshMapStatus_e {
    ESP_PLAYER_FRESH_MAP_INVALID = 0,
    ESP_PLAYER_FRESH_MAP_VIEW_INVALID = 1,
    ESP_PLAYER_FRESH_MAP_HUD_INVALID = 2,
    ESP_PLAYER_FRESH_MAP_UNSUPPORTED_CONTEXT = 3,
    ESP_PLAYER_FRESH_MAP_UNSUPPORTED_ORDER = 4,
    ESP_PLAYER_FRESH_MAP_WEAPON_RESTORE_DEFERRED = 5,
    ESP_PLAYER_FRESH_MAP_ALREADY_ACTIVE = 6,
    ESP_PLAYER_FRESH_MAP_VIEW_CONSUME_FAILED = 7,
    ESP_PLAYER_FRESH_MAP_OK = 8
} EspPlayerFreshMapStatus;

/*
 * Permanent pointer-free owner for the fresh-map fields written by recovered
 * Player_setup(). `levelStartTimeMs` is the caller-sampled equivalent of
 * DoomRPG_GetUpTimeMS(); it is intentionally dynamic and must not be treated as
 * a cross-run fingerprint input without normalization.
 *
 * The empty legacy Player.NotebookString is represented compactly by
 * notebookEmpty=1. The reusable EspMapNotebookState remains caller-owned until
 * the native gameplay NOTE path needs to materialize non-empty text.
 *
 * Weapon restoration is deliberately not performed here. A nonzero legacy
 * disabledWeapons value is rejected fail-closed because Player_restoreWeapons()
 * can select a weapon and request a view refresh, which belongs to a later
 * native player/inventory owner.
 */
typedef struct EspPlayerFreshMapState_s {
    uint32_t levelStartTimeMs;
    uint32_t moves;
    uint32_t xpGained;
    uint32_t berserkerTics;

    uint8_t familiarActive;
    uint8_t notebookEmpty;
    uint8_t weaponRestorePerformed;
    uint8_t targetMapId;
    uint8_t gameplayLoadMapId;
    uint8_t loadType;
    uint8_t setupApplied;
    uint8_t active;
} EspPlayerFreshMapState;

void EspPlayerFreshMap_reset(void);
int EspPlayerFreshMap_isReady(void);
const EspPlayerFreshMapState* EspPlayerFreshMap_view(void);

/*
 * Pure validation/translation of one post-HUD fresh-map Player_setup boundary.
 * The output is zeroed on refusal when non-NULL. No global state is mutated.
 */
EspPlayerFreshMapStatus EspPlayerFreshMap_prepare(
    const EspPlayerViewState* playerView,
    const EspHudRefreshState* hudRefresh,
    uint32_t nowMs,
    uint32_t disabledWeapons,
    EspPlayerFreshMapState* outState);

/*
 * Route the live post-HUD player/view owner into the permanent fresh-map
 * session owner and atomically consume only playerSetupPending. Facing and the
 * initial tile-enter remain pending. No allocation, PAK I/O or legacy mutation
 * occurs.
 */
EspPlayerFreshMapStatus EspPlayerFreshMap_route(
    uint32_t nowMs,
    uint32_t disabledWeapons);

#ifdef __cplusplus
}
#endif

#endif
