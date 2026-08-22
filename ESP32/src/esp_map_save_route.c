#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_save_route.h"
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

void EspMapSaveRoute_reset(EspMapSaveRouteState* state) {
    if (state != NULL) memset(state, 0, sizeof(*state));
}

int EspMapSaveRoute_isActive(const EspMapSaveRouteState* state) {
    return state != NULL && state->active != 0U;
}

EspMapSaveRouteStatus EspMapSaveRoute_apply(
    EspMapSaveRouteState* state,
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapSaveRouteResult* outResult) {
    EspMapByteCode command;
    EspMapStringRef mapName;
    EspMapSaveRouteState next;
    uint32_t globalCommandIndex;
    uint32_t packedDestination;
    uint32_t destinationX;
    uint32_t destinationY;
    uint8_t rawX;
    uint8_t rawY;
    uint8_t angle;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (state == NULL || descriptor == NULL || outResult == NULL) {
        return ESP_MAP_SAVE_ROUTE_INVALID;
    }
    if (!descriptorIsCanonical(descriptor) ||
        commandOffset >= descriptor->commandCount || commandOffset > 0xffU ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return ESP_MAP_SAVE_ROUTE_INVALID;
    }
    if (command.id != ESP_MAP_OPCODE_SAVEGAME) {
        return ESP_MAP_SAVE_ROUTE_UNSUPPORTED;
    }

    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    if (globalCommandIndex > 0xffffU) {
        return ESP_MAP_SAVE_ROUTE_INVALID;
    }

    if (!EspMapStrings_getRef(command.arg1 & 0xffU, &mapName)) {
        return ESP_MAP_SAVE_ROUTE_STRING_NOT_FOUND;
    }
    if (mapName.length >= ESP_MAP_SAVE_ROUTE_LEGACY_NAME_CAPACITY) {
        return ESP_MAP_SAVE_ROUTE_STRING_TOO_LONG;
    }

    packedDestination = command.arg1 >> 8;
    rawX = (uint8_t)(packedDestination & 0xffU);
    rawY = (uint8_t)((packedDestination >> 8) & 0xffU);
    angle = (uint8_t)((packedDestination >> 16) & 0xffU);
    destinationX = 32U + ((uint32_t)rawX << 6);
    destinationY = 32U + ((uint32_t)rawY << 6);
    if (destinationX > 0xffffU || destinationY > 0xffffU) {
        return ESP_MAP_SAVE_ROUTE_INVALID;
    }

    memset(&next, 0, sizeof(next));
    next.mapName = mapName;
    next.destinationX = (uint16_t)destinationX;
    next.destinationY = (uint16_t)destinationY;
    next.sourceEventIndex = descriptor->eventIndex;
    next.globalCommandIndex = (uint16_t)globalCommandIndex;
    next.sourceCommandOffset = (uint8_t)commandOffset;
    next.angle = angle;
    next.rawX = rawX;
    next.rawY = rawY;
    next.active = 1U;

    *state = next;

    outResult->sourceEventIndex = descriptor->eventIndex;
    outResult->globalCommandIndex = (uint16_t)globalCommandIndex;
    outResult->mapStringIndex = mapName.index;
    outResult->destinationX = (uint16_t)destinationX;
    outResult->destinationY = (uint16_t)destinationY;
    outResult->sourceCommandOffset = (uint8_t)commandOffset;
    outResult->rawX = rawX;
    outResult->rawY = rawY;
    outResult->angle = angle;
    outResult->legacyReturnValue = 1U;
    outResult->removeCommandIfHandled =
        (uint8_t)((command.arg2 & ESP_MAP_SAVE_ROUTE_COMMAND_FLAG_REMOVE) != 0U);
    return ESP_MAP_SAVE_ROUTE_OK;
}
