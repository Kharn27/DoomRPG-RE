#ifndef DOOMRPG_ESP32_MAP_EVENTS_H
#define DOOMRPG_ESP32_MAP_EVENTS_H

#include <stdint.h>

#include "esp_map_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_EVENT_TILE_MASK 0x000003ffU
#define ESP_MAP_EVENT_TILE_COUNT 1024U

#define ESP_MAP_EVENT_COMMAND_INDEX_MASK 0x0007fc00U
#define ESP_MAP_EVENT_COMMAND_INDEX_SHIFT 10U
#define ESP_MAP_EVENT_COMMAND_COUNT_MASK 0x01f80000U
#define ESP_MAP_EVENT_COMMAND_COUNT_SHIFT 19U
#define ESP_MAP_EVENT_STATE_MASK 0x1e000000U
#define ESP_MAP_EVENT_STATE_SHIFT 25U
#define ESP_MAP_EVENT_FLAGS_MASK 0xe0000000U
#define ESP_MAP_EVENT_FLAGS_SHIFT 29U

typedef struct EspMapEventRef_s {
    uint16_t index;
    uint16_t tileIndex;
    uint32_t value;
} EspMapEventRef;

typedef struct EspMapEventDescriptor_s {
    uint32_t value;
    uint16_t eventIndex;
    uint16_t tileIndex;
    uint16_t firstCommandIndex;
    uint16_t commandEndIndex;
    uint8_t commandCount;
    uint8_t initialState;
    uint8_t flags;
} EspMapEventDescriptor;

/*
 * Allocation-free tile -> event lookup over the immutable compact event
 * records already owned by EspMapRuntime.
 *
 * The recovered desktop engine binary-searches tileEvents[] by the low 10 bits
 * of each event value. Native code keeps that compact ownership model instead
 * of allocating a second 1024-entry tile index.
 *
 * Event records are expected to be ordered by tile index. MAP_INTRO hardware
 * validation proved strict ordering/uniqueness. If a future BSP contains
 * duplicate event tiles, this API deterministically returns the first matching
 * record (lower_bound semantics).
 */
int EspMapEvents_findByTile(uint32_t tileIndex, EspMapEventRef* outEvent);

/*
 * Decode one immutable source event into its structural descriptor.
 *
 * `initialState` is deliberately named as such: recovered gameplay mutates the
 * event-state bits later, so current runtime state must eventually live in a
 * separate mutable overlay rather than modifying the compact source arena.
 *
 * firstCommandIndex/commandCount are logical EspMapByteCode record indexes.
 * The desktop engine multiplied both by BYTE_CODE_MAX only because its
 * mapByteCode storage was an integer array flattened as ID/ARG1/ARG2 triples.
 */
int EspMapEvents_describe(const EspMapEventRef* eventRef,
                          EspMapEventDescriptor* outDescriptor);

/* Bounds-checked, allocation-free access to a command linked by a descriptor. */
int EspMapEvents_getCommand(const EspMapEventDescriptor* descriptor,
                            uint32_t commandOffset,
                            EspMapByteCode* outCommand);

#ifdef __cplusplus
}
#endif

#endif
