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
    ESP_NATIVE_GAMEPLAY_ACTION_DOOR_OK = 8,
    ESP_NATIVE_GAMEPLAY_ACTION_DIALOG_READY = 9
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
 * Execute only bounded production SELECT semantic families.
 *
 * The complete front-tile event is filtered before any mutation. Supported
 * families at this boundary are:
 *
 *   1. exactly one eligible EV_OPENLINE/EV_CLOSELINE command;
 *   2. a first eligible EV_DIALOG/EV_DIALOGNOBACK pause followed by at most one
 *      eligible state-only continuation already owned by EspMapOpcodeExecutor
 *      (11/19/20). The dialog itself is returned as DIALOG_READY without any
 *      UI or script mutation; the resident dialog session owns presentation and
 *      resume after the user closes it.
 *
 * Any other eligible opcode, an unsupported resume, or a multi-command door
 * event remains fail-closed so production never partially executes a script.
 * Native key ownership is still absent, therefore filtering uses playerKeys=0.
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
