#ifndef DOOMRPG_ESP32_MAP_EVENTS_H
#define DOOMRPG_ESP32_MAP_EVENTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_EVENT_TILE_MASK 0x000003ffU
#define ESP_MAP_EVENT_TILE_COUNT 1024U

typedef struct EspMapEventRef_s {
    uint16_t index;
    uint16_t tileIndex;
    uint32_t value;
} EspMapEventRef;

/*
 * Allocation-free tile -> event lookup over the immutable compact event
 * records already owned by EspMapRuntime.
 *
 * The recovered desktop engine binary-searches tileEvents[] by the low 10 bits
 * of each event value. Native code keeps that compact ownership model instead
 * of allocating a second 1024-entry tile index.
 *
 * Event records are expected to be ordered by tile index. The current hardware
 * probe validates strict ordering/uniqueness for MAP_INTRO. If a future BSP
 * needs multiple records on one tile, this API deterministically returns the
 * first matching record (lower_bound semantics).
 */
int EspMapEvents_findByTile(uint32_t tileIndex, EspMapEventRef* outEvent);

#ifdef __cplusplus
}
#endif

#endif
