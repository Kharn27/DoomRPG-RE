#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_MOVE_EVENTS_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_MOVE_EVENTS_H

#include <stdint.h>

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
    ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK = 8
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
} EspNativeGameplayMoveEventResult;

/*
 * Execute the bounded movement tile-event family recovered from
 * Game_executeTile()/Game_runEvent(): exactly one eligible OPENLINE/CLOSELINE
 * command may mutate native line/script state. Any broader eligible event is
 * reported without mutation and remains deferred to its own opcode milestone.
 * Native key ownership is still absent, so playerKeys remains deliberately 0.
 */
EspNativeGameplayMoveEventStatus EspNativeGameplayMoveEvents_executeDoorPhase(
    uint16_t tile,
    uint32_t runFlags,
    EspNativeGameplayMoveEventResult* outResult);

int EspNativeGameplayMoveEvents_rollbackDoorPhase(
    const EspNativeGameplayMoveEventResult* result);

const char* EspNativeGameplayMoveEvents_statusName(
    EspNativeGameplayMoveEventStatus status);

/* Integration hooks used by the scoped gameplay MOVE/render transaction. */
void EspNativeGameplayMoveEvents_onFrameResult(int renderOk);

#ifdef __cplusplus
}
#endif

#endif
