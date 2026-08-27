#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_ACTION_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_ACTION_H

#include <stdint.h>

#include "esp_native_gameplay_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativeGameplayActionStatus_e {
    ESP_NATIVE_GAMEPLAY_ACTION_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_ACTION_NO_EVENT = 2,
    ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE = 3,
    ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT = 4,
    ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT = 5,
    ESP_NATIVE_GAMEPLAY_ACTION_DOOR_LOCKED = 6,
    ESP_NATIVE_GAMEPLAY_ACTION_DOOR_ALREADY_TARGET = 7,
    ESP_NATIVE_GAMEPLAY_ACTION_DOOR_OK = 8
} EspNativeGameplayActionStatus;

typedef struct EspNativeGameplayActionResult_s {
    uint32_t sequence;
    uint16_t frontTile;
    uint16_t eventIndex;
    uint16_t globalCommandIndex;
    uint16_t lineIndex;
    uint16_t soundId;
    uint8_t commandOffset;
    uint8_t codeId;
    uint8_t eligibleCount;
    uint8_t unsupportedCodeId;
    uint8_t openBefore;
    uint8_t openAfter;
    uint8_t locked;
    uint8_t handled;
    uint8_t mutated;
    uint8_t effectFlags;
    uint8_t removedBefore;
    uint8_t removedAfter;
    uint8_t removeIfHandled;
    uint8_t rollbackAvailable;
} EspNativeGameplayActionResult;

/*
 * Execute only the first bounded production SELECT semantic family.
 *
 * The complete front-tile event is filtered before any mutation. Exactly one
 * eligible command is accepted, and it must be EV_OPENLINE/EV_CLOSELINE.
 * Any other eligible opcode or multi-command eligible event remains fail-closed
 * so production never partially executes a script that the native engine does
 * not yet completely own.
 *
 * Native key ownership is still absent, therefore filtering deliberately uses
 * playerKeys=0. Door lock/already-target outcomes match legacy false/no-op.
 * A successful door command mutates only EspMapLineState and, when requested by
 * source arg2 0x200, the compact removed-command bit. Sound/animation/entity
 * relink and turn advancement are returned/deferred, never performed here.
 */
EspNativeGameplayActionStatus EspNativeGameplayAction_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult);

/* Exact transactional rollback for a successful DOOR_OK result. */
int EspNativeGameplayAction_rollbackSelect(
    const EspNativeGameplayActionResult* result);

const char* EspNativeGameplayAction_statusName(
    EspNativeGameplayActionStatus status);

#ifdef __cplusplus
}
#endif

#endif
