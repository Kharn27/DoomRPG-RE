#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_line_state.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_script_state.h"
#include "esp_native_gameplay_destructible.h"

static int descriptorForTile(uint16_t tile,
                             EspMapEventDescriptor* outDescriptor) {
    EspMapEventRef ref;
    if (outDescriptor != NULL) memset(outDescriptor, 0, sizeof(*outDescriptor));
    if (outDescriptor == NULL || !EspMapEvents_findByTile(tile, &ref)) return 0;
    return EspMapEvents_describe(&ref, outDescriptor);
}

static EspNativeGameplayDestructibleStatus prepareLineDeath(
    uint16_t eventTile,
    uint16_t expectedLineIndex,
    EspMapEventDescriptor* outDescriptor,
    uint32_t* outCommandOffset,
    uint16_t* outGlobal,
    uint8_t* outRemovedBefore) {
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    EspMapByteCode command;
    uint32_t selectedOffset = UINT32_MAX;
    uint16_t selectedGlobal = 0U;
    uint8_t selectedRemoved = 0U;
    uint8_t currentState;
    uint32_t offset;
    uint8_t open;
    uint8_t locked;

    if (outDescriptor == NULL || outCommandOffset == NULL || outGlobal == NULL ||
        outRemovedBefore == NULL) {
        return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_INVALID;
    }
    memset(outDescriptor, 0, sizeof(*outDescriptor));
    *outCommandOffset = 0U;
    *outGlobal = 0U;
    *outRemovedBefore = 0U;

    if (!EspMapScriptState_isReady() || !EspMapLineState_isReady()) {
        return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_NOT_READY;
    }
    if (!descriptorForTile(eventTile, &descriptor)) {
        return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_NO_EVENT;
    }
    if (!EspMapScriptState_getEventState(descriptor.eventIndex, &currentState) ||
        !EspMapEventFilter_prepare(
            &descriptor, currentState, 0U,
            ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_DEATH_RUN_FLAGS,
            0U, &plan)) {
        return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_INVALID;
    }

    for (offset = 0U; offset < descriptor.commandCount; ++offset) {
        uint32_t global = (uint32_t)descriptor.firstCommandIndex + offset;
        uint8_t removed;
        if (global > UINT16_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(&descriptor, &plan, offset,
                                        removed, &filtered)) {
            return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_INVALID;
        }
        if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
        if (selectedOffset != UINT32_MAX) {
            return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_UNSUPPORTED_EVENT;
        }
        if (!EspMapEvents_getCommand(&descriptor, offset, &command) ||
            command.id != ESP_MAP_OPCODE_OPENLINE ||
            command.arg1 != (uint32_t)expectedLineIndex) {
            return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_UNSUPPORTED_EVENT;
        }
        selectedOffset = offset;
        selectedGlobal = filtered.globalCommandIndex;
        selectedRemoved = removed;
    }

    if (selectedOffset == UINT32_MAX) {
        return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_NO_EFFECT;
    }
    if (!EspMapLineState_getOpen(expectedLineIndex, &open) ||
        !EspMapLineState_getLocked(expectedLineIndex, &locked)) {
        return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_INVALID;
    }
    if (locked != 0U) return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_UNSUPPORTED_EVENT;
    if (open != 0U) return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_NO_EFFECT;

    *outDescriptor = descriptor;
    *outCommandOffset = selectedOffset;
    *outGlobal = selectedGlobal;
    *outRemovedBefore = selectedRemoved;
    return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK;
}

EspNativeGameplayDestructibleStatus
EspNativeGameplayDestructible_preflightLineDeath(
    uint16_t eventTile,
    uint16_t expectedLineIndex,
    EspNativeGameplayDestructibleResult* outResult) {
    EspMapEventDescriptor descriptor;
    uint32_t commandOffset;
    uint16_t global;
    uint8_t removedBefore;
    uint8_t open;
    EspNativeGameplayDestructibleStatus status;

    if (outResult == NULL) return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_INVALID;
    memset(outResult, 0, sizeof(*outResult));
    status = prepareLineDeath(eventTile, expectedLineIndex, &descriptor,
                              &commandOffset, &global, &removedBefore);
    if (status != ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK) return status;
    if (commandOffset > UINT8_MAX ||
        !EspMapLineState_getOpen(expectedLineIndex, &open)) {
        return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_INVALID;
    }

    outResult->eventIndex = descriptor.eventIndex;
    outResult->lineIndex = expectedLineIndex;
    outResult->globalCommandIndex = global;
    outResult->commandOffset = (uint8_t)commandOffset;
    outResult->codeId = ESP_MAP_OPCODE_OPENLINE;
    outResult->openBefore = open;
    outResult->openAfter = open;
    outResult->removedBefore = removedBefore;
    outResult->removedAfter = removedBefore;
    return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK;
}

EspNativeGameplayDestructibleStatus
EspNativeGameplayDestructible_executeLineDeath(
    uint16_t eventTile,
    uint16_t expectedLineIndex,
    EspNativeGameplayDestructibleResult* outResult) {
    EspMapEventDescriptor descriptor;
    EspMapLineDoorResult door;
    uint32_t commandOffset;
    uint16_t global;
    uint8_t removedBefore;
    uint8_t removedAfter;
    EspNativeGameplayDestructibleStatus status;

    if (outResult == NULL) return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_INVALID;
    memset(outResult, 0, sizeof(*outResult));
    status = prepareLineDeath(eventTile, expectedLineIndex, &descriptor,
                              &commandOffset, &global, &removedBefore);
    if (status != ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK) return status;

    memset(&door, 0, sizeof(door));
    if (EspMapLineState_applyDoorCommand(&descriptor, commandOffset, &door) !=
            ESP_MAP_LINE_DOOR_OK ||
        door.mutated != 1U || door.codeId != ESP_MAP_OPCODE_OPENLINE ||
        door.lineIndex != expectedLineIndex ||
        door.globalCommandIndex != global) {
        if (door.mutated != 0U && door.lineIndex == expectedLineIndex) {
            (void)EspMapLineState_setOpen(door.lineIndex, door.openBefore);
        }
        return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_UNSUPPORTED_EVENT;
    }

    removedAfter = removedBefore;
    if (door.removeCommandIfHandled != 0U && removedBefore == 0U) {
        if (!EspMapScriptState_setCommandRemoved(global, 1U)) {
            (void)EspMapLineState_setOpen(door.lineIndex, door.openBefore);
            return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_INVALID;
        }
        removedAfter = 1U;
    }

    outResult->eventIndex = descriptor.eventIndex;
    outResult->lineIndex = door.lineIndex;
    outResult->globalCommandIndex = global;
    outResult->commandOffset = (uint8_t)commandOffset;
    outResult->codeId = door.codeId;
    outResult->openBefore = door.openBefore;
    outResult->openAfter = door.openAfter;
    outResult->removedBefore = removedBefore;
    outResult->removedAfter = removedAfter;
    outResult->mutated = 1U;
    outResult->rollbackAvailable = 1U;
    return ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK;
}

int EspNativeGameplayDestructible_rollbackLineDeath(
    const EspNativeGameplayDestructibleResult* result) {
    uint8_t openNow;
    uint8_t removedNow;

    if (result == NULL || result->mutated != 1U ||
        result->rollbackAvailable != 1U ||
        result->codeId != ESP_MAP_OPCODE_OPENLINE ||
        !EspMapLineState_getOpen(result->lineIndex, &openNow) ||
        !EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                             &removedNow) ||
        openNow != result->openAfter || removedNow != result->removedAfter) {
        return 0;
    }
    if (!EspMapLineState_setOpen(result->lineIndex, result->openBefore)) return 0;
    if (result->removedBefore != result->removedAfter &&
        !EspMapScriptState_setCommandRemoved(result->globalCommandIndex,
                                              result->removedBefore)) {
        (void)EspMapLineState_setOpen(result->lineIndex, result->openAfter);
        return 0;
    }
    return EspMapLineState_getOpen(result->lineIndex, &openNow) &&
           EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                               &removedNow) &&
           openNow == result->openBefore &&
           removedNow == result->removedBefore;
}

const char* EspNativeGameplayDestructible_statusName(
    EspNativeGameplayDestructibleStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_NO_EVENT: return "NO_EVENT";
    case ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_UNSUPPORTED_EVENT:
        return "UNSUPPORTED_EVENT";
    case ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_NO_EFFECT: return "NO_EFFECT";
    case ESP_NATIVE_GAMEPLAY_DESTRUCTIBLE_OK: return "OK";
    default: return "UNKNOWN";
    }
}
