#ifndef DOOMRPG_ESP32_MAP_RESIDENT_LIFECYCLE_H
#define DOOMRPG_ESP32_MAP_RESIDENT_LIFECYCLE_H

#include <stdint.h>

#include "esp_bsp_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_RESIDENT_ENTITY_DEFS_RESOURCE "/entities.db"

typedef enum EspMapResidentLifecycleStatus_e {
    ESP_MAP_RESIDENT_INVALID = 0,
    ESP_MAP_RESIDENT_NOT_EMPTY = 1,
    ESP_MAP_RESIDENT_PACK_BUSY = 2,
    ESP_MAP_RESIDENT_PACK_OPEN_FAILED = 3,
    ESP_MAP_RESIDENT_ENTITY_DEFS_MISSING = 4,
    ESP_MAP_RESIDENT_RUNTIME_FAILED = 5,
    ESP_MAP_RESIDENT_MAP_STATE_FAILED = 6,
    ESP_MAP_RESIDENT_SCRIPT_STATE_FAILED = 7,
    ESP_MAP_RESIDENT_LINE_STATE_FAILED = 8,
    ESP_MAP_RESIDENT_TEXTURE_STATE_FAILED = 9,
    ESP_MAP_RESIDENT_AUTOMAP_STATE_FAILED = 10,
    ESP_MAP_RESIDENT_TOPOLOGY_FAILED = 11,
    ESP_MAP_RESIDENT_SNAPSHOT_FAILED = 12,
    ESP_MAP_RESIDENT_OK = 13
} EspMapResidentLifecycleStatus;

/*
 * Pointer-free summary of one completely built native resident map.
 *
 * byte fields are logical payload sizes owned by the seven current resident
 * owners. Actual allocator cost is deliberately measured by the lifecycle
 * caller/probe because allocator overhead is platform-specific.
 */
typedef struct EspMapResidentSnapshot_s {
    uint32_t runtimeArenaBytes;
    uint32_t mapStateBytes;
    uint32_t scriptStateBytes;
    uint32_t lineStateBytes;
    uint32_t textureStateBytes;
    uint32_t automapStateBytes;
    uint32_t topologyBytes;
    uint32_t totalPayloadBytes;

    uint32_t runtimeFNV1a;
    uint32_t mapStateFNV1a;
    uint32_t scriptStateFNV1a;
    uint32_t lineStateFNV1a;
    uint32_t textureStateFNV1a;
    uint32_t automapStateFNV1a;
    uint32_t topologyFNV1a;

    uint32_t nodeCount;
    uint32_t lineCount;
    uint32_t spriteCount;
    uint32_t eventCount;
    uint32_t byteCodeCount;
    uint32_t stringCount;

    uint32_t entityCount;
    uint32_t enemyCount;
    uint32_t destructibleCount;
} EspMapResidentSnapshot;

/*
 * Explicit dependency-safe teardown. The order is reverse-owner dependency:
 * topology/automap/texture/line/script/map-state first, immutable runtime last.
 */
void EspMapResidentLifecycle_resetAll(void);

/* True only when none of the seven resident owners is currently live. */
int EspMapResidentLifecycle_isEmpty(void);

/* True only when all seven owners are live and cardinalities agree. */
int EspMapResidentLifecycle_isReady(void);

/* Capture one deterministic pointer-free view of the live owner set. */
int EspMapResidentLifecycle_capture(EspMapResidentSnapshot* outSnapshot);

/*
 * Build one complete resident map only from an explicitly empty lifecycle.
 *
 * This function NEVER tears down an existing source map. A non-empty lifecycle
 * returns ESP_MAP_RESIDENT_NOT_EMPTY before opening the PAK. The caller must
 * call resetAll() explicitly at the handoff point.
 *
 * The function owns one temporary PAK session, builds the compact immutable
 * runtime and all currently permanent mutable owners, closes the PAK, and
 * returns a caller-owned snapshot. On any failure the lifecycle is reset back
 * to empty and the output snapshot is zeroed.
 */
EspMapResidentLifecycleStatus EspMapResidentLifecycle_loadFromEmpty(
    const char* resourceName,
    const EspBspInventory* inventory,
    EspMapResidentSnapshot* outSnapshot);

#ifdef __cplusplus
}
#endif

#endif
