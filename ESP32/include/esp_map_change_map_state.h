#ifndef DOOMRPG_ESP32_MAP_CHANGE_MAP_STATE_H
#define DOOMRPG_ESP32_MAP_CHANGE_MAP_STATE_H

#include <stdint.h>

#include "esp_map_events.h"
#include "esp_map_strings.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_CHANGE_MAP 2U
#define ESP_MAP_CHANGE_MAP_SHOW_STATS_BIT 0x80000000UL
#define ESP_MAP_CHANGE_MAP_COMMAND_FLAG_REMOVE 0x00000200UL

#define ESP_MAP_CHANGE_MAP_EFFECT_ADD_LEVEL_STATS 0x01U
#define ESP_MAP_CHANGE_MAP_EFFECT_SHOW_STATS_MENU 0x02U
#define ESP_MAP_CHANGE_MAP_EFFECT_LOAD_MAP 0x04U

typedef enum EspMapChangeMapStatus_e {
    ESP_MAP_CHANGE_MAP_INVALID = 0,
    ESP_MAP_CHANGE_MAP_UNSUPPORTED = 1,
    ESP_MAP_CHANGE_MAP_STRING_NOT_FOUND = 2,
    ESP_MAP_CHANGE_MAP_OK = 3
} EspMapChangeMapStatus;

/*
 * Compact native equivalent of the pending Game.changeMapParam field.
 *
 * The destination string stays as an immutable map-local span because the
 * legacy consumer resolves it while the source map is still resident, before
 * initiating teardown. EV_SAVEGAME is different and therefore owns an inline
 * durable name separately.
 */
typedef struct EspMapChangeMapState_s {
    uint32_t rawParam;
    EspMapStringRef mapName;
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint8_t sourceCommandOffset;
    uint8_t active;
} EspMapChangeMapState;

typedef struct EspMapChangeMapResult_s {
    uint32_t rawParam;
    uint32_t spawnParam;
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t mapStringIndex;
    uint8_t sourceCommandOffset;
    uint8_t showStats;
    uint8_t pending;
    uint8_t legacyReturnValue;
    uint8_t removeCommandIfHandled;
    uint8_t effectFlags;
} EspMapChangeMapResult;

void EspMapChangeMap_reset(EspMapChangeMapState* state);
int EspMapChangeMap_isActive(const EspMapChangeMapState* state);

/*
 * Execute only 2 / EV_CHANGEMAP into caller-owned pending-transition state.
 *
 * The bytecode itself only assigns Game.changeMapParam. A later door/transition
 * consumer resolves the destination, applies level-stat/menu/load effects and
 * clears the pending parameter. This native stage therefore performs no PAK
 * I/O, no sound, no level-stat mutation, no menu transition, no map load and no
 * legacy Game/Render/Entity mutation.
 *
 * For a non-zero parameter, the low byte is validated/resolved to an immutable
 * map string span now, while the current native map runtime is resident. A zero
 * parameter clears the pending native state and remains handled, matching the
 * legacy assignment/no-op-consumer semantics.
 */
EspMapChangeMapStatus EspMapChangeMap_apply(
    EspMapChangeMapState* state,
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapChangeMapResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
