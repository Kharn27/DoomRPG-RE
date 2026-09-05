#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HAZARD_TOUCH_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HAZARD_TOUCH_H

#include <stdint.h>

#include "esp_player_view_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

typedef enum EspNativeGameplayHazardTouchStatus_e {
    ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_NONE = 0,
    ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED = 1,
    ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED = 2,
    ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_FATAL = 3
} EspNativeGameplayHazardTouchStatus;

typedef struct EspNativeGameplayHazardPassTurnUndo_s {
    uint32_t param1Before;
    uint32_t playerFNVBefore;
    uint16_t tileIndex;
    uint8_t feedbackQueued;
    uint8_t committed;
} EspNativeGameplayHazardPassTurnUndo;

/* Recovered movement-side Game_touchTile(..., true) subset for linked type 10/11
 * hazards. The permanent owner remains the shared PlayerState; this executor has
 * no heap/BSS gameplay owner. Its caller owns same-session committed before/after
 * snapshots and map identity. Resource/hazard ordering on a mixed movement tile
 * remains fail-closed until the complete native TileTouch orchestrator exists. */
EspNativeGameplayHazardTouchStatus EspNativeGameplayHazardTouch_processMove(
    struct DoomRPG_s* doomRpg,
    const EspPlayerViewState* beforeView,
    const EspPlayerViewState* afterView);

/*
 * Recovered PASS_TURN Game_touchTile(..., false) subset. Only linked type 10/11
 * entities on the settled current tile are touched; resource entities are
 * intentionally ignored exactly as legacy touched=false does. This path commits
 * only PlayerState + one bounded damage feedback intent. It does not render: the
 * normal action-feedback service presents the queued top-bar/red-flash state
 * before MonsterTurn runs. The caller can roll the transaction back if the
 * subsequent monster-turn request cannot be armed.
 */
EspNativeGameplayHazardTouchStatus EspNativeGameplayHazardTouch_processPassTurn(
    EspNativeGameplayHazardPassTurnUndo* outUndo);
int EspNativeGameplayHazardTouch_rollbackPassTurn(
    const EspNativeGameplayHazardPassTurnUndo* undo);

#ifdef __cplusplus
}
#endif

#endif
