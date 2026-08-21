#include <stddef.h>
#include <stdint.h>

#include "esp_map_events.h"
#include "esp_map_runtime.h"

int EspMapEvents_findByTile(uint32_t tileIndex, EspMapEventRef* outEvent) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t left;
    uint32_t right;
    uint32_t mid;
    uint32_t value;
    uint32_t eventTile;

    if (outEvent == NULL || tileIndex >= ESP_MAP_EVENT_TILE_COUNT ||
        runtime == NULL || runtime->events == NULL ||
        runtime->eventCount == 0U || runtime->eventCount > 0xffffU ||
        runtime->eventBytes != runtime->eventCount * ESP_MAP_EVENT_RECORD_BYTES) {
        return 0;
    }

    /* lower_bound by the recovered low-10-bit tile key */
    left = 0U;
    right = runtime->eventCount;
    while (left < right) {
        mid = left + ((right - left) >> 1);
        if (!EspMapRuntime_getEvent(mid, &value)) {
            return 0;
        }

        eventTile = value & ESP_MAP_EVENT_TILE_MASK;
        if (eventTile < tileIndex) {
            left = mid + 1U;
        }
        else {
            right = mid;
        }
    }

    if (left >= runtime->eventCount || !EspMapRuntime_getEvent(left, &value)) {
        return 0;
    }

    eventTile = value & ESP_MAP_EVENT_TILE_MASK;
    if (eventTile != tileIndex) {
        return 0;
    }

    outEvent->index = (uint16_t)left;
    outEvent->tileIndex = (uint16_t)eventTile;
    outEvent->value = value;
    return 1;
}

int EspMapEvents_describe(const EspMapEventRef* eventRef,
                          EspMapEventDescriptor* outDescriptor) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t value;
    uint32_t tileIndex;
    uint32_t firstCommandIndex;
    uint32_t commandCount;
    uint32_t commandEndIndex;

    if (eventRef == NULL || outDescriptor == NULL || runtime == NULL ||
        runtime->events == NULL || runtime->byteCodes == NULL ||
        eventRef->index >= runtime->eventCount ||
        eventRef->tileIndex >= ESP_MAP_EVENT_TILE_COUNT) {
        return 0;
    }

    if (!EspMapRuntime_getEvent(eventRef->index, &value) ||
        value != eventRef->value) {
        return 0;
    }

    tileIndex = value & ESP_MAP_EVENT_TILE_MASK;
    if (tileIndex != (uint32_t)eventRef->tileIndex) {
        return 0;
    }

    firstCommandIndex =
        (value & ESP_MAP_EVENT_COMMAND_INDEX_MASK) >>
        ESP_MAP_EVENT_COMMAND_INDEX_SHIFT;
    commandCount =
        (value & ESP_MAP_EVENT_COMMAND_COUNT_MASK) >>
        ESP_MAP_EVENT_COMMAND_COUNT_SHIFT;
    commandEndIndex = firstCommandIndex + commandCount;

    if (firstCommandIndex > runtime->byteCodeCount ||
        commandEndIndex > runtime->byteCodeCount ||
        commandEndIndex > 0xffffU) {
        return 0;
    }

    outDescriptor->value = value;
    outDescriptor->eventIndex = eventRef->index;
    outDescriptor->tileIndex = eventRef->tileIndex;
    outDescriptor->firstCommandIndex = (uint16_t)firstCommandIndex;
    outDescriptor->commandEndIndex = (uint16_t)commandEndIndex;
    outDescriptor->commandCount = (uint8_t)commandCount;
    outDescriptor->initialState = (uint8_t)(
        (value & ESP_MAP_EVENT_STATE_MASK) >> ESP_MAP_EVENT_STATE_SHIFT);
    outDescriptor->flags = (uint8_t)(
        (value & ESP_MAP_EVENT_FLAGS_MASK) >> ESP_MAP_EVENT_FLAGS_SHIFT);
    return 1;
}

int EspMapEvents_getCommand(const EspMapEventDescriptor* descriptor,
                            uint32_t commandOffset,
                            EspMapByteCode* outCommand) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint32_t commandIndex;
    uint32_t expectedEnd;

    if (descriptor == NULL || outCommand == NULL || runtime == NULL ||
        runtime->byteCodes == NULL || commandOffset >= descriptor->commandCount) {
        return 0;
    }

    expectedEnd = (uint32_t)descriptor->firstCommandIndex +
                  (uint32_t)descriptor->commandCount;
    if (descriptor->commandEndIndex != expectedEnd ||
        expectedEnd > runtime->byteCodeCount) {
        return 0;
    }

    commandIndex = (uint32_t)descriptor->firstCommandIndex + commandOffset;
    return EspMapRuntime_getByteCode(commandIndex, outCommand);
}
