#ifndef DOOMRPG_ESP32_MAP_KEY_GATE_H
#define DOOMRPG_ESP32_MAP_KEY_GATE_H

#include <stdint.h>

#include "esp_map_events.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_CHECK_KEY 41U
#define ESP_MAP_KEY_GATE_SOUND_ID 5065U

typedef enum EspMapKeyGateStatus_e {
    ESP_MAP_KEY_GATE_INVALID = 0,
    ESP_MAP_KEY_GATE_UNSUPPORTED = 1,
    ESP_MAP_KEY_GATE_PASS = 2,
    ESP_MAP_KEY_GATE_BLOCKED = 3
} EspMapKeyGateStatus;

typedef enum EspMapKeyIndex_e {
    ESP_MAP_KEY_GREEN = 0,
    ESP_MAP_KEY_YELLOW = 1,
    ESP_MAP_KEY_BLUE = 2,
    ESP_MAP_KEY_RED = 3
} EspMapKeyIndex;

typedef struct EspMapKeyGateResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t soundId;
    uint8_t sourceCommandOffset;
    uint8_t keyIndex;
    uint8_t requiredMask;
    uint8_t legacyReturnValue;
    uint8_t stopEvent;
    uint8_t saveCurrentCommand;
} EspMapKeyGateResult;

/*
 * Evaluate opcode 41 / EV_CHECK_KEY without mutating Player, Hud, Sound, Game,
 * world or render state. The descriptor must be the canonical descriptor for
 * the currently loaded native runtime.
 *
 * Recovered legacy semantics:
 *   required key present -> Game_executeEvent returns false and execution
 *                           continues with no side effect;
 *   required key missing -> message + sound 5065, executeEvent returns true,
 *                           Game_runEvent stops and saves the current command.
 *
 * Only real key selectors 0..3 are accepted. keyBits uses the legacy bit
 * layout 1,2,4,8; higher bits are ignored.
 */
EspMapKeyGateStatus EspMapKeyGate_evaluate(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    uint32_t keyBits,
    EspMapKeyGateResult* outResult);

/* Exact recovered Hud text for a blocked result, otherwise NULL. */
const char* EspMapKeyGate_message(const EspMapKeyGateResult* result);

#ifdef __cplusplus
}
#endif

#endif
