#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_HAZARD_TOUCH_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_HAZARD_TOUCH_H

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

/* Recovered movement-side Game_touchTile(..., true) subset for linked type 10/11
 * hazards. The permanent owner remains the shared PlayerState; this executor has
 * no heap/BSS gameplay owner. Its caller owns same-session committed before/after
 * snapshots and map identity. Resource/hazard ordering on a mixed tile remains
 * fail-closed until the complete native TileTouch orchestrator exists. PASS TURN
 * keeps its existing type-10/11 fail-closed guard until native ordered multi-
 * message feedback can preserve "Turn passed." + damage + burn text ordering. */
EspNativeGameplayHazardTouchStatus EspNativeGameplayHazardTouch_processMove(
    struct DoomRPG_s* doomRpg,
    const EspPlayerViewState* beforeView,
    const EspPlayerViewState* afterView);

#ifdef __cplusplus
}
#endif

#endif
