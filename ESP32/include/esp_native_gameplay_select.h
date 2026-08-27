#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_SELECT_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_SELECT_H

#include <stdint.h>

#include "esp_native_gameplay_input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS 0x00000500UL
#define ESP_NATIVE_GAMEPLAY_SELECT_NO_EVENT 0xffffU

typedef enum EspNativeGameplaySelectStatus_e {
    ESP_NATIVE_GAMEPLAY_SELECT_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_SELECT_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_SELECT_OUT_OF_BOUNDS = 2,
    ESP_NATIVE_GAMEPLAY_SELECT_NO_TILE_EVENT = 3,
    ESP_NATIVE_GAMEPLAY_SELECT_TILE_EVENT = 4
} EspNativeGameplaySelectStatus;

/*
 * Read-only SELECT front-tile resolution. This is the native equivalent of
 * the first legacy SELECT step:
 *
 *   Game_executeTile(destX + viewStepX, destY + viewStepY, 1280)
 *
 * The result intentionally stops at immutable event provenance + current
 * compact script state. It does not execute bytecode, trace facing entities,
 * mutate keys/doors/world/render state, play sound, or advance a turn.
 */
typedef struct EspNativeGameplaySelectResult_s {
    uint32_t sequence;
    uint32_t inputFlags;
    int32_t frontX;
    int32_t frontY;
    uint16_t frontTile;
    uint16_t eventIndex;
    uint16_t firstCommandIndex;
    uint16_t commandEndIndex;
    uint8_t commandCount;
    uint8_t currentState;
    uint8_t eventFlags;
    uint8_t eventFound;
} EspNativeGameplaySelectResult;

EspNativeGameplaySelectStatus EspNativeGameplaySelect_resolve(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplaySelectResult* outResult);

const char* EspNativeGameplaySelect_statusName(
    EspNativeGameplaySelectStatus status);

#ifdef __cplusplus
}
#endif

#endif
