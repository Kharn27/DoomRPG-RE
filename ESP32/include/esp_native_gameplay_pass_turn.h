#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PASS_TURN_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PASS_TURN_H

#include "esp_native_gameplay_input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native PASS TURN owns only the turn request when the legacy
 * Game_touchTile(..., false) type-10/11 precondition is absent. The legacy
 * "Turn passed." HUD message remains a separate presentation milestone. */
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
