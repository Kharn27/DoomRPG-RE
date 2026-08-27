#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_line_state.h"
#include "esp_map_script_state.h"
#include "esp_native_gameplay_action.h"
#include "esp_native_gameplay_select.h"

typedef char EspNativeGameplayActionResult_must_be_28_bytes[
    sizeof(EspNativeGameplayActionResult) == 28U ? 1 : -1];

static int descriptorMatchesSelect(
    const EspMapEventDescriptor* descriptor,
    const EspNativeGameplaySelectResult* select) {
    return descriptor != NULL && select != NULL &&
           descriptor->eventIndex == select->eventIndex &&
           descriptor->tileIndex == select->frontTile &&
           descriptor->firstCommandIndex == select->firstCommandIndex &&
           descriptor->commandEndIndex == select->commandEndIndex &&
           descriptor->commandCount == select->commandCount;
}

EspNativeGameplayActionStatus EspNativeGameplayAction_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult) {
    EspNativeGameplaySelectResult select;
    EspNativeGameplaySelectStatus selectStatus;
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    EspMapLineDoorResult door;
    EspMapLineDoorStatus doorStatus;
    uint32_t selectedOffset = UINT32_MAX;
    uint16_t selectedGlobal = 0U;
    uint8_t selectedRemoved = 0U;
    uint8_t eligibleCount = 0U;
    uint32_t offset;

    if (outResult == NULL) return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    memset(outResult, 0, sizeof(*outResult));
    if (intent == NULL || intent->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        intent->active != 1U || intent->pending != 1U ||
        intent->sequence == 0U) {
        return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    }
    if (!EspMapScriptState_isReady() || !EspMapLineState_isReady()) {
        return ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY;
    }

    memset(&select, 0, sizeof(select));
    selectStatus = EspNativeGameplaySelect_resolve(intent, &select);
    outResult->sequence = intent->sequence;
    outResult->frontTile = select.frontTile;
    outResult->eventIndex = select.eventIndex;

    if (selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_OUT_OF_BOUNDS ||
        selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_NO_TILE_EVENT) {
        return ESP_NATIVE_GAMEPLAY_ACTION_NO_EVENT;
    }
    if (selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_NOT_READY) {
        return ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY;
    }
    if (selectStatus != ESP_NATIVE_GAMEPLAY_SELECT_TILE_EVENT ||
        select.eventFound != 1U) {
        return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    }

    memset(&eventRef, 0, sizeof(eventRef));
    memset(&descriptor, 0, sizeof(descriptor));
    if (!EspMapEvents_findByTile(select.frontTile, &eventRef) ||
        eventRef.index != select.eventIndex ||
        !EspMapEvents_describe(&eventRef, &descriptor) ||
        !descriptorMatchesSelect(&descriptor, &select) ||
        !EspMapEventFilter_prepare(&descriptor,
                                   select.currentState,
                                   0U,
                                   ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS,
                                   0U,
                                   &plan)) {
        return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    }

    /* Preflight the complete eligible command set before world mutation. */
    for (offset = 0U; offset < descriptor.commandCount; ++offset) {
        uint32_t global = (uint32_t)descriptor.firstCommandIndex + offset;
        uint8_t removed;

        if (global > UINT16_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(&descriptor,
                                        &plan,
                                        offset,
                                        removed,
                                        &filtered)) {
            return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
        }
        if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;

        if (eligibleCount != UINT8_MAX) ++eligibleCount;
        outResult->eligibleCount = eligibleCount;
        if (filtered.codeId != ESP_MAP_OPCODE_OPENLINE &&
            filtered.codeId != ESP_MAP_OPCODE_CLOSELINE) {
            outResult->unsupportedCodeId = filtered.codeId;
            return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
        }
        if (selectedOffset != UINT32_MAX) {
            return ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT;
        }
        selectedOffset = offset;
        selectedGlobal = filtered.globalCommandIndex;
        selectedRemoved = removed;
    }

    if (selectedOffset == UINT32_MAX || eligibleCount == 0U) {
        return ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE;
    }
    if (eligibleCount != 1U) {
        return ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT;
    }

    memset(&door, 0, sizeof(door));
    doorStatus = EspMapLineState_applyDoorCommand(
        &descriptor, selectedOffset, &door);

    outResult->globalCommandIndex = selectedGlobal;
    outResult->lineIndex = door.lineIndex;
    outResult->soundId = door.soundId;
    outResult->commandOffset = (uint8_t)selectedOffset;
    outResult->codeId = door.codeId;
    outResult->openBefore = door.openBefore;
    outResult->openAfter = door.openAfter;
    outResult->locked = door.locked;
    outResult->mutated = door.mutated;
    outResult->effectFlags = door.effectFlags;
    outResult->removeIfHandled = door.removeCommandIfHandled;
    outResult->removedBefore = selectedRemoved;
    outResult->removedAfter = selectedRemoved;

    if (doorStatus == ESP_MAP_LINE_DOOR_LOCKED) {
        return ESP_NATIVE_GAMEPLAY_ACTION_DOOR_LOCKED;
    }
    if (doorStatus == ESP_MAP_LINE_DOOR_ALREADY_TARGET) {
        return ESP_NATIVE_GAMEPLAY_ACTION_DOOR_ALREADY_TARGET;
    }
    if (doorStatus == ESP_MAP_LINE_DOOR_NOT_READY) {
        return ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY;
    }
    if (doorStatus != ESP_MAP_LINE_DOOR_OK || door.mutated != 1U) {
        return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    }

    if (door.removeCommandIfHandled != 0U) {
        if (!EspMapScriptState_setCommandRemoved(selectedGlobal, 1U)) {
            (void)EspMapLineState_setOpen(door.lineIndex, door.openBefore);
            memset(outResult, 0, sizeof(*outResult));
            return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
        }
        outResult->removedAfter = 1U;
    }

    outResult->handled = 1U;
    outResult->rollbackAvailable = 1U;
    return ESP_NATIVE_GAMEPLAY_ACTION_DOOR_OK;
}

int EspNativeGameplayAction_rollbackSelect(
    const EspNativeGameplayActionResult* result) {
    uint8_t openNow;
    uint8_t removedNow;

    if (result == NULL || result->handled != 1U || result->mutated != 1U ||
        result->rollbackAvailable != 1U ||
        result->codeId != ESP_MAP_OPCODE_OPENLINE &&
            result->codeId != ESP_MAP_OPCODE_CLOSELINE) {
        return 0;
    }
    if (!EspMapLineState_getOpen(result->lineIndex, &openNow) ||
        !EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                             &removedNow) ||
        openNow != result->openAfter || removedNow != result->removedAfter) {
        return 0;
    }

    if (!EspMapLineState_setOpen(result->lineIndex, result->openBefore)) {
        return 0;
    }
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

const char* EspNativeGameplayAction_statusName(
    EspNativeGameplayActionStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_ACTION_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_ACTION_NO_EVENT: return "NO_EVENT";
    case ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE: return "NO_ELIGIBLE";
    case ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT: return "UNSUPPORTED_EVENT";
    case ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT: return "COMPLEX_EVENT";
    case ESP_NATIVE_GAMEPLAY_ACTION_DOOR_LOCKED: return "DOOR_LOCKED";
    case ESP_NATIVE_GAMEPLAY_ACTION_DOOR_ALREADY_TARGET: return "DOOR_ALREADY_TARGET";
    case ESP_NATIVE_GAMEPLAY_ACTION_DOOR_OK: return "DOOR_OK";
    default: return "UNKNOWN";
    }
}
