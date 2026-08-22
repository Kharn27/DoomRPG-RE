#ifndef DOOMRPG_ESP32_MAP_SAVE_ROUTE_H
#define DOOMRPG_ESP32_MAP_SAVE_ROUTE_H

#include <stdint.h>

#include "esp_asset_pack.h"
#include "esp_map_events.h"
#include "esp_map_strings.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_SAVEGAME 27U
#define ESP_MAP_SAVE_ROUTE_NAME_CAPACITY 32U
#define ESP_MAP_SAVE_ROUTE_COMMAND_FLAG_REMOVE 0x00000200UL

typedef enum EspMapSaveRouteStatus_e {
    ESP_MAP_SAVE_ROUTE_INVALID = 0,
    ESP_MAP_SAVE_ROUTE_UNSUPPORTED = 1,
    ESP_MAP_SAVE_ROUTE_STRING_NOT_FOUND = 2,
    ESP_MAP_SAVE_ROUTE_STRING_TOO_LONG = 3,
    ESP_MAP_SAVE_ROUTE_IO_ERROR = 4,
    ESP_MAP_SAVE_ROUTE_OK = 5
} EspMapSaveRouteStatus;

/*
 * Compact native equivalent of the persistent fields written by EV_SAVEGAME:
 *   Game.newMapName[32] / newDestX / newDestY / newAngle.
 *
 * Unlike ordinary map-local string refs, this route must survive destruction
 * of the current map runtime before the later save-state consumer runs. Keep
 * the one bounded destination name inline, exactly as the legacy Game state
 * did, rather than retaining a ref whose source map may already be gone.
 */
typedef struct EspMapSaveRouteState_s {
    char mapName[ESP_MAP_SAVE_ROUTE_NAME_CAPACITY];
    uint16_t destinationX;
    uint16_t destinationY;
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint8_t sourceCommandOffset;
    uint8_t angle;
    uint8_t rawX;
    uint8_t rawY;
    uint8_t mapNameLength;
    uint8_t active;
} EspMapSaveRouteState;

typedef struct EspMapSaveRouteResult_s {
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint16_t mapStringIndex;
    uint16_t destinationX;
    uint16_t destinationY;
    uint8_t sourceCommandOffset;
    uint8_t rawX;
    uint8_t rawY;
    uint8_t angle;
    uint8_t legacyReturnValue;
    uint8_t removeCommandIfHandled;
} EspMapSaveRouteResult;

void EspMapSaveRoute_reset(EspMapSaveRouteState* state);
int EspMapSaveRoute_isActive(const EspMapSaveRouteState* state);

/*
 * Execute only 27 / EV_SAVEGAME into caller-owned route state.
 *
 * Recovered legacy behavior does not write a save file here. It only captures
 * a future map name, destination x/y and angle; Game_saveState() consumes that
 * route later, after map-runtime teardown may already have started.
 *
 * Therefore this executor performs exactly one bounded native-PAK string read
 * into the 32-byte owner. It allocates nothing, performs no serialization, no
 * save-file write, no map transition and no legacy Game mutation. Runtime ZIP
 * access remains forbidden.
 */
EspMapSaveRouteStatus EspMapSaveRoute_apply(
    const EspAssetPackEntry* sourceEntry,
    EspMapSaveRouteState* state,
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapSaveRouteResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
