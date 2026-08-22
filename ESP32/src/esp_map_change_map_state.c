#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_change_map_state.h"
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
        !EspMapRuntime_getEvent(descriptor->eventIndex, &value)) return 0;

    ref.index = descriptor->eventIndex;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    return EspMapEvents_describe(&ref, &canonical) &&
           sameDescriptor(descriptor, &canonical);
}

static uint32_t decodeSpawnParam(uint32_t rawParam) {
    /*
     * Legacy: (changeMapParam << 1) >> 9.
     * Use unsigned shifts to preserve the intended bit extraction without
     * relying on signed-left-shift behavior. This keeps bits 8..30 and drops
     * the bit-31 SHOWSTATS flag plus the low-byte string id.
     */
    return (rawParam << 1U) >> 9U;
}

void EspMapChangeMap_reset(EspMapChangeMapState* state) {
    if (state != NULL) memset(state, 0, sizeof(*state));
}

int EspMapChangeMap_isActive(const EspMapChangeMapState* state) {
    return state != NULL && state->active != 0U;
}

EspMapChangeMapStatus EspMapChangeMap_apply(
    EspMapChangeMapState* state,
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapChangeMapResult* outResult) {
    EspMapByteCode command;
    EspMapStringRef mapName;
    EspMapChangeMapState next;
    uint32_t globalCommandIndex;
    uint32_t rawParam;
    uint32_t spawnParam;
    uint8_t showStats;
    uint8_t pending;
    uint8_t effectFlags = 0U;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (state == NULL || descriptor == NULL || outResult == NULL) {
        return ESP_MAP_CHANGE_MAP_INVALID;
    }
    if (!descriptorIsCanonical(descriptor) ||
        commandOffset >= descriptor->commandCount || commandOffset > 0xffU ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return ESP_MAP_CHANGE_MAP_INVALID;
    }
    if (command.id != ESP_MAP_OPCODE_CHANGE_MAP) {
        return ESP_MAP_CHANGE_MAP_UNSUPPORTED;
    }

    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    if (globalCommandIndex > 0xffffU) {
        return ESP_MAP_CHANGE_MAP_INVALID;
    }

    rawParam = command.arg1;
    spawnParam = decodeSpawnParam(rawParam);
    showStats = (uint8_t)((rawParam & ESP_MAP_CHANGE_MAP_SHOW_STATS_BIT) != 0U);
    pending = (uint8_t)(rawParam != 0U);

    memset(&next, 0, sizeof(next));
    if (pending != 0U) {
        if (!EspMapStrings_getRef(rawParam & 0xffU, &mapName)) {
            return ESP_MAP_CHANGE_MAP_STRING_NOT_FOUND;
        }
        next.rawParam = rawParam;
        next.mapName = mapName;
        next.sourceEventIndex = descriptor->eventIndex;
        next.globalCommandIndex = (uint16_t)globalCommandIndex;
        next.sourceCommandOffset = (uint8_t)commandOffset;
        next.active = 1U;

        effectFlags = ESP_MAP_CHANGE_MAP_EFFECT_ADD_LEVEL_STATS;
        effectFlags |= showStats != 0U
                           ? ESP_MAP_CHANGE_MAP_EFFECT_SHOW_STATS_MENU
                           : ESP_MAP_CHANGE_MAP_EFFECT_LOAD_MAP;
    }

    *state = next;

    outResult->rawParam = rawParam;
    outResult->spawnParam = spawnParam;
    outResult->sourceEventIndex = descriptor->eventIndex;
    outResult->globalCommandIndex = (uint16_t)globalCommandIndex;
    outResult->mapStringIndex = (uint16_t)(rawParam & 0xffU);
    outResult->sourceCommandOffset = (uint8_t)commandOffset;
    outResult->showStats = showStats;
    outResult->pending = pending;
    outResult->legacyReturnValue = 1U;
    outResult->removeCommandIfHandled =
        (uint8_t)((command.arg2 &
                   ESP_MAP_CHANGE_MAP_COMMAND_FLAG_REMOVE) != 0U);
    outResult->effectFlags = effectFlags;
    return ESP_MAP_CHANGE_MAP_OK;
}
