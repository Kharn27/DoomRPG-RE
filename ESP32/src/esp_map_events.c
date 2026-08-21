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
