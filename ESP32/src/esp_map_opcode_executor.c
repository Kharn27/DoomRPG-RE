#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_events.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_script_state.h"

int EspMapOpcodeExecutor_supports(uint8_t codeId) {
    return codeId == ESP_MAP_OPCODE_CHANGE_STATE ||
           codeId == ESP_MAP_OPCODE_NEXT_STATE ||
           codeId == ESP_MAP_OPCODE_PREV_STATE;
}

EspMapOpcodeExecStatus EspMapOpcodeExecutor_execute(
    const EspMapByteCode* command,
    EspMapOpcodeExecResult* outResult) {
    EspMapEventRef targetEvent;
    uint32_t x;
    uint32_t y;
    uint32_t targetTile;
    uint32_t requestedState;
    uint8_t before;
    uint8_t after;

    if (outResult != NULL) {
        memset(outResult, 0, sizeof(*outResult));
    }
    if (command == NULL || outResult == NULL || !EspMapScriptState_isReady()) {
        return ESP_MAP_OPCODE_EXEC_INVALID;
    }

    outResult->arg1 = command->arg1;
    outResult->arg2 = command->arg2;
    outResult->codeId = command->id;
    outResult->status = ESP_MAP_OPCODE_EXEC_INVALID;

    if (!EspMapOpcodeExecutor_supports(command->id)) {
        outResult->status = ESP_MAP_OPCODE_EXEC_UNSUPPORTED;
        return ESP_MAP_OPCODE_EXEC_UNSUPPORTED;
    }

    x = command->arg1 & 0xffU;
    y = (command->arg1 >> 8) & 0xffU;
    targetTile = x + (y * 32U);
    if (targetTile >= ESP_MAP_EVENT_TILE_COUNT ||
        !EspMapEvents_findByTile(targetTile, &targetEvent)) {
        outResult->targetTile =
            (uint16_t)(targetTile <= 0xffffU ? targetTile : 0xffffU);
        outResult->status = ESP_MAP_OPCODE_EXEC_TARGET_NOT_FOUND;
        return ESP_MAP_OPCODE_EXEC_TARGET_NOT_FOUND;
    }

    outResult->targetTile = targetEvent.tileIndex;
    outResult->targetEventIndex = targetEvent.index;

    if (!EspMapScriptState_getEventState(targetEvent.index, &before)) {
        outResult->status = ESP_MAP_OPCODE_EXEC_INVALID;
        return ESP_MAP_OPCODE_EXEC_INVALID;
    }

    after = before;
    if (command->id == ESP_MAP_OPCODE_CHANGE_STATE) {
        requestedState = (command->arg1 >> 16) & 0xffU;
        if (requestedState > 15U) {
            outResult->stateBefore = before;
            outResult->stateAfter = before;
            outResult->status = ESP_MAP_OPCODE_EXEC_STATE_OUT_OF_RANGE;
            return ESP_MAP_OPCODE_EXEC_STATE_OUT_OF_RANGE;
        }
        after = (uint8_t)requestedState;
    }
    else if (command->id == ESP_MAP_OPCODE_NEXT_STATE) {
        if (before < 9U) {
            after = (uint8_t)(before + 1U);
        }
    }
    else if (command->id == ESP_MAP_OPCODE_PREV_STATE) {
        if (before > 0U) {
            after = (uint8_t)(before - 1U);
        }
    }

    outResult->stateBefore = before;
    outResult->stateAfter = after;
    outResult->mutated = (uint8_t)(after != before ? 1U : 0U);

    if (after != before &&
        !EspMapScriptState_setEventState(targetEvent.index, after)) {
        outResult->status = ESP_MAP_OPCODE_EXEC_INVALID;
        return ESP_MAP_OPCODE_EXEC_INVALID;
    }

    outResult->status = ESP_MAP_OPCODE_EXEC_OK;
    return ESP_MAP_OPCODE_EXEC_OK;
}
