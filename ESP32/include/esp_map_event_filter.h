#ifndef DOOMRPG_ESP32_MAP_EVENT_FILTER_H
#define DOOMRPG_ESP32_MAP_EVENT_FILTER_H

#include <stdint.h>

#include "esp_map_events.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_EVENT_FLAG_BLOCK_INPUT 0x01U
#define ESP_MAP_EVENT_BLOCK_INPUT_RUN_FLAG 0x00000400U
#define ESP_MAP_EVENT_STATE_ARG_MASK 0x01ff0000U
#define ESP_MAP_EVENT_KEY_ARG_MASK 0x0000f000U

#define ESP_MAP_PLAYER_KEY_GREEN 0x01U
#define ESP_MAP_PLAYER_KEY_YELLOW 0x02U
#define ESP_MAP_PLAYER_KEY_BLUE 0x04U
#define ESP_MAP_PLAYER_KEY_RED 0x08U

#define ESP_MAP_RUN_KEY_YELLOW 0x00001000U
#define ESP_MAP_RUN_KEY_GREEN 0x00002000U
#define ESP_MAP_RUN_KEY_BLUE 0x00004000U
#define ESP_MAP_RUN_KEY_RED 0x00008000U

typedef enum EspMapEventCommandDecision_e {
    ESP_MAP_EVENT_COMMAND_ELIGIBLE = 0,
    ESP_MAP_EVENT_COMMAND_EVENT_BLOCKED = 1,
    ESP_MAP_EVENT_COMMAND_BEFORE_START = 2,
    ESP_MAP_EVENT_COMMAND_REMOVED = 3,
    ESP_MAP_EVENT_COMMAND_STATE_MISMATCH = 4,
    ESP_MAP_EVENT_COMMAND_KEY_MISMATCH = 5,
    ESP_MAP_EVENT_COMMAND_FLAGS_MISMATCH = 6
} EspMapEventCommandDecision;

typedef struct EspMapEventFilterPlan_s {
    uint32_t inputFlags;
    uint32_t effectiveFlags;
    uint32_t stateArgMask;
    uint16_t eventIndex;
    uint16_t startCommandOffset;
    uint8_t currentState;
    uint8_t eventBlocked;
} EspMapEventFilterPlan;

typedef struct EspMapEventCommandFilterResult_s {
    uint32_t arg2;
    uint16_t globalCommandIndex;
    uint8_t commandOffset;
    uint8_t codeId;
    uint8_t decision;
} EspMapEventCommandFilterResult;

/*
 * Prepare the recovered Game_runEvent() filtering context without side effects.
 * currentState is the mutable runtime state, not descriptor.initialState.
 */
int EspMapEventFilter_prepare(const EspMapEventDescriptor* descriptor,
                              uint8_t currentState,
                              uint32_t startCommandOffset,
                              uint32_t inputFlags,
                              uint32_t playerKeys,
                              EspMapEventFilterPlan* outPlan);

/*
 * Classify one linked command exactly as Game_runEvent() would before calling
 * Game_executeEvent(). `removed` represents the native replacement for the
 * legacy mutation that zeroed mapByteCode[arg2] after MCODE_FLAG_REMOVE.
 */
int EspMapEventFilter_evaluate(const EspMapEventDescriptor* descriptor,
                               const EspMapEventFilterPlan* plan,
                               uint32_t commandOffset,
                               uint8_t removed,
                               EspMapEventCommandFilterResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
