#ifndef DOOMRPG_ESP32_MAP_OPCODE_EXECUTOR_H
#define DOOMRPG_ESP32_MAP_OPCODE_EXECUTOR_H

#include <stdint.h>

#include "esp_map_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_CHANGE_STATE 11U
#define ESP_MAP_OPCODE_NEXT_STATE 19U
#define ESP_MAP_OPCODE_PREV_STATE 20U

typedef enum EspMapOpcodeExecStatus_e {
    ESP_MAP_OPCODE_EXEC_INVALID = 0,
    ESP_MAP_OPCODE_EXEC_UNSUPPORTED = 1,
    ESP_MAP_OPCODE_EXEC_TARGET_NOT_FOUND = 2,
    ESP_MAP_OPCODE_EXEC_STATE_OUT_OF_RANGE = 3,
    ESP_MAP_OPCODE_EXEC_OK = 4
} EspMapOpcodeExecStatus;

typedef struct EspMapOpcodeExecResult_s {
    uint32_t arg1;
    uint32_t arg2;
    uint16_t targetTile;
    uint16_t targetEventIndex;
    uint8_t codeId;
    uint8_t stateBefore;
    uint8_t stateAfter;
    uint8_t mutated;
    uint8_t status;
} EspMapOpcodeExecResult;

/*
 * First deliberately tiny native opcode executor.
 *
 * Only the three recovered event-state mutation opcodes are supported:
 *   EV_CHANGESTATE (11)
 *   EV_NEXTSTATE   (19)
 *   EV_PREVSTATE   (20)
 *
 * Every other opcode fails closed with ESP_MAP_OPCODE_EXEC_UNSUPPORTED and
 * performs no mutation. Supported opcodes mutate only EspMapScriptState; the
 * immutable map arena, map tile state, renderer, entities and Game_t world are
 * never modified here.
 */
int EspMapOpcodeExecutor_supports(uint8_t codeId);
EspMapOpcodeExecStatus EspMapOpcodeExecutor_execute(
    const EspMapByteCode* command,
    EspMapOpcodeExecResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
