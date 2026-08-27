#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MOVE_EVENTS_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MOVE_EVENTS_H

#include <stdint.h>

#include "esp_native_gameplay_status_message.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativeGameplayMoveEventStatus_e {
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID = 0,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY = 1,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_EVENT = 2,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_ELIGIBLE = 3,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED = 4,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX = 5,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_LOCKED = 6,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_ALREADY_TARGET = 7,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK = 8,
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_FORCE_MESSAGE_OK = 9
} EspNativeGameplayMoveEventStatus;

typedef struct EspNativeGameplayMoveEventResult_s {
    uint32_t runFlags;
    uint16_t tile;
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
    uint8_t mutated;
    uint8_t removedBefore;
    uint8_t removedAfter;
    uint8_t removeIfHandled;
    uint8_t rollbackAvailable;
    EspNativeGameplayStatusMessageResult statusMessage;
} EspNativeGameplayMoveEventResult;

/*
 * Execute the bounded movement tile-event families recovered from
 * Game_executeTile()/Game_runEvent(): exactly one eligible regular door
 * OPENLINE/CLOSELINE or FORCE_MESSAGE command may run. Any broader eligible
 * event remains fail-closed for its own opcode milestone. Native key ownership
 * is still absent, so playerKeys remains deliberately 0.
 */
EspNativeGameplayMoveEventStatus EspNativeGameplayMoveEvents_executePhase(
    uint16_t tile,
    uint32_t runFlags,
    EspNativeGameplayMoveEventResult* outResult);

int EspNativeGameplayMoveEvents_rollbackPhase(
    const EspNativeGameplayMoveEventResult* result);

const char* EspNativeGameplayMoveEvents_statusName(
    EspNativeGameplayMoveEventStatus status);

/* Integration hooks used by the scoped gameplay MOVE/render transaction. */
void EspNativeGameplayMoveEvents_onFrameResult(int renderOk);

#ifdef __cplusplus
}
#endif

#endif
