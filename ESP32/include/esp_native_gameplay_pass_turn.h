#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PASS_TURN_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PASS_TURN_H

#include "esp_native_gameplay_input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native PASS TURN reproduces the bounded legacy ordering:
 *
 *   Hud_addMessage("Turn passed.")
 *   Game_touchTile(current, false)  -> linked type10/11 only
 *   Game_advanceTurn()
 *
 * The one-slot native top-bar cannot display both messages simultaneously, so
 * an owned current-tile hazard supersedes the transient "Turn passed." visual
 * with the already-proven damage feedback while preserving gameplay ordering:
 * hazard PlayerState mutation is committed before the MonsterTurn request. If
 * that request cannot be armed, hazard state + queued feedback roll back exactly.
 * With no hazard, the existing exact "Turn passed." feedback path is unchanged.
 * Neither route mutates player position/facing or allocates permanent memory. */
typedef enum EspNativeGameplayPassTurnStatus_e {
    ESP_NATIVE_GAMEPLAY_PASS_TURN_OK = 0,
    ESP_NATIVE_GAMEPLAY_PASS_TURN_INVALID = 1,
    ESP_NATIVE_GAMEPLAY_PASS_TURN_NOT_READY = 2,
    ESP_NATIVE_GAMEPLAY_PASS_TURN_TILE_TOUCH_DEFERRED = 3,
    ESP_NATIVE_GAMEPLAY_PASS_TURN_REQUEST_BUSY = 4
} EspNativeGameplayPassTurnStatus;

EspNativeGameplayPassTurnStatus EspNativeGameplayPassTurn_execute(
    const EspNativeGameplayInputState* intent);

#ifdef __cplusplus
}
#endif

#endif
