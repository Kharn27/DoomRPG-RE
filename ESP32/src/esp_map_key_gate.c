#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_key_gate.h"
#include "esp_map_runtime.h"

static int sameDescriptor(const EspMapEventDescriptor* a,
                          const EspMapEventDescriptor* b) {
    return a != NULL && b != NULL &&
           a->value == b->value &&
           a->eventIndex == b->eventIndex &&
           a->tileIndex == b->tileIndex &&
           a->firstCommandIndex == b->firstCommandIndex &&
           a->commandEndIndex == b->commandEndIndex &&
           a->commandCount == b->commandCount &&
           a->initialState == b->initialState &&
           a->flags == b->flags;
}

static int descriptorIsCanonical(const EspMapEventDescriptor* descriptor) {
    EspMapEventRef ref;
    EspMapEventDescriptor canonical;
    uint32_t value;

    if (descriptor == NULL ||
        !EspMapRuntime_getEvent(descriptor->eventIndex, &value)) {
        return 0;
    }

    ref.index = descriptor->eventIndex;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    return EspMapEvents_describe(&ref, &canonical) &&
           sameDescriptor(descriptor, &canonical);
}

EspMapKeyGateStatus EspMapKeyGate_evaluate(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    uint32_t keyBits,
    EspMapKeyGateResult* outResult) {
    EspMapByteCode command;
    uint32_t globalCommandIndex;
    uint32_t requiredMask;

    if (outResult != NULL) {
        memset(outResult, 0, sizeof(*outResult));
    }
    if (descriptor == NULL || outResult == NULL ||
        !descriptorIsCanonical(descriptor) ||
        commandOffset >= descriptor->commandCount ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return ESP_MAP_KEY_GATE_INVALID;
    }
    if (command.id != ESP_MAP_OPCODE_CHECK_KEY) {
        return ESP_MAP_KEY_GATE_UNSUPPORTED;
    }
    if (command.arg1 > ESP_MAP_KEY_RED) {
        return ESP_MAP_KEY_GATE_INVALID;
    }

    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    if (globalCommandIndex > 0xffffU) {
        return ESP_MAP_KEY_GATE_INVALID;
    }

    requiredMask = 1UL << command.arg1;
    outResult->sourceEventIndex = descriptor->eventIndex;
    outResult->globalCommandIndex = (uint16_t)globalCommandIndex;
    outResult->sourceCommandOffset = (uint8_t)commandOffset;
    outResult->keyIndex = (uint8_t)command.arg1;
    outResult->requiredMask = (uint8_t)requiredMask;

    if ((keyBits & requiredMask) != 0U) {
        return ESP_MAP_KEY_GATE_PASS;
    }

    outResult->soundId = ESP_MAP_KEY_GATE_SOUND_ID;
    outResult->legacyReturnValue = 1U;
    outResult->stopEvent = 1U;
    outResult->saveCurrentCommand = 1U;
    return ESP_MAP_KEY_GATE_BLOCKED;
}

const char* EspMapKeyGate_message(const EspMapKeyGateResult* result) {
    if (result == NULL || result->soundId != ESP_MAP_KEY_GATE_SOUND_ID ||
        result->legacyReturnValue == 0U || result->stopEvent == 0U ||
        result->saveCurrentCommand == 0U) {
        return NULL;
    }

    switch (result->keyIndex) {
        case ESP_MAP_KEY_GREEN:  return "Need Green Key";
        case ESP_MAP_KEY_YELLOW: return "Need Yellow Key";
        case ESP_MAP_KEY_BLUE:   return "Need Blue Key";
        case ESP_MAP_KEY_RED:    return "Need Red Key";
        default:                 return NULL;
    }
}
