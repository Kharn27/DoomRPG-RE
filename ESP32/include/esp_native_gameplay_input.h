#ifndef DOOMRPG_ESP32_ESP_NATIVE_GAMEPLAY_INPUT_H
#define DOOMRPG_ESP32_ESP_NATIVE_GAMEPLAY_INPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Preserve the recovered Doom RPG keypad semantic IDs without importing the
 * legacy DoomCanvas playing-event loop. Gaps are intentional and match the
 * original mobile action table. */
typedef enum EspNativeGameplayAction_e {
    ESP_NATIVE_GAMEPLAY_ACTION_NONE = 0,
    ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD = 1,
    ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK = 2,
    ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT = 3,
    ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT = 4,
    ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN = 5,
    ESP_NATIVE_GAMEPLAY_ACTION_SELECT = 6,
    ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP = 7,
    ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT = 9,
    ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT = 10,
    ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON = 11,
    ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON = 12,
    ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN = 14
} EspNativeGameplayAction;

typedef enum EspNativeGameplayTouchZone_e {
    ESP_NATIVE_GAMEPLAY_ZONE_NONE = 0,
    ESP_NATIVE_GAMEPLAY_ZONE_MOVE_LEFT = 1,
    ESP_NATIVE_GAMEPLAY_ZONE_MOVE_FORWARD = 2,
    ESP_NATIVE_GAMEPLAY_ZONE_MOVE_RIGHT = 3,
    ESP_NATIVE_GAMEPLAY_ZONE_TURN_LEFT = 4,
    ESP_NATIVE_GAMEPLAY_ZONE_SELECT = 5,
    ESP_NATIVE_GAMEPLAY_ZONE_TURN_RIGHT = 6,
    ESP_NATIVE_GAMEPLAY_ZONE_NEXT_WEAPON = 7,
    ESP_NATIVE_GAMEPLAY_ZONE_MOVE_BACK = 8,
    ESP_NATIVE_GAMEPLAY_ZONE_PASS_TURN = 9,
    ESP_NATIVE_GAMEPLAY_ZONE_MENU = 10,
    ESP_NATIVE_GAMEPLAY_ZONE_AUTOMAP = 11
} EspNativeGameplayTouchZone;

typedef enum EspNativeGameplayInputStatus_e {
    ESP_NATIVE_GAMEPLAY_INPUT_OK = 0,
    ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT = 1,
    ESP_NATIVE_GAMEPLAY_INPUT_INVALID = 2,
    ESP_NATIVE_GAMEPLAY_INPUT_BUSY = 3,
    ESP_NATIVE_GAMEPLAY_INPUT_EMPTY = 4
} EspNativeGameplayInputStatus;

/* Pure hit-test result. Coordinates are inclusive logical 160x120 bounds. */
typedef struct EspNativeGameplayTouchHit_s {
    uint8_t action;
    uint8_t zone;
    uint8_t left;
    uint8_t top;
    uint8_t right;
    uint8_t bottom;
} EspNativeGameplayTouchHit;

/* One compact pointer-free semantic owner. No queue is hidden here: a producer
 * must fail closed while pending, and the future gameplay dispatcher must
 * explicitly consume an intent before another one can be routed. */
typedef struct EspNativeGameplayInputState_s {
    uint8_t action;
    uint8_t zone;
    uint8_t logicalX;
    uint8_t logicalY;
    uint32_t sequence;
    uint8_t pending;
    uint8_t active;
    uint8_t reserved[2];
} EspNativeGameplayInputState;

void EspNativeGameplayInput_reset(void);

EspNativeGameplayInputStatus EspNativeGameplayInput_classify(
    int logicalX,
    int logicalY,
    EspNativeGameplayTouchHit* outHit);

EspNativeGameplayInputStatus EspNativeGameplayInput_route(
    const EspNativeGameplayTouchHit* hit,
    int logicalX,
    int logicalY);

const EspNativeGameplayInputState* EspNativeGameplayInput_peek(void);

EspNativeGameplayInputStatus EspNativeGameplayInput_consume(
    EspNativeGameplayInputState* outIntent);

const char* EspNativeGameplayInput_actionName(uint8_t action);

#ifdef __cplusplus
}
#endif

#endif
