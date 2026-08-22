#ifndef DOOMRPG_ESP32_MAP_SAVE_ROUTE_H
#define DOOMRPG_ESP32_MAP_SAVE_ROUTE_H

#include <stdint.h>

#include "esp_map_events.h"
#include "esp_map_strings.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_OPCODE_SAVEGAME 27U
#define ESP_MAP_SAVE_ROUTE_LEGACY_NAME_CAPACITY 32U
#define ESP_MAP_SAVE_ROUTE_COMMAND_FLAG_REMOVE 0x00000200UL

typedef enum EspMapSaveRouteStatus_e {
    ESP_MAP_SAVE_ROUTE_INVALID = 0,
    ESP_MAP_SAVE_ROUTE_UNSUPPORTED = 1,
    ESP_MAP_SAVE_ROUTE_STRING_NOT_FOUND = 2,
    ESP_MAP_SAVE_ROUTE_STRING_TOO_LONG = 3,
    ESP_MAP_SAVE_ROUTE_OK = 4
} EspMapSaveRouteStatus;

/*
 * Compact native equivalent of the persistent fields written by EV_SAVEGAME:
 *   Game.newMapName / newDestX / newDestY / newAngle.
 *
 * The map name remains a zero-copy reference into the native pack-backed map
 * string table. rawX/rawY preserve the exact command payload while
 * destinationX/destinationY preserve the recovered 32 + (tile << 6) values.
 */
typedef struct EspMapSaveRouteState_s {
    EspMapStringRef mapName;
    uint16_t destinationX;
    uint16_t destinationY;
    uint16_t sourceEventIndex;
    uint16_t globalCommandIndex;
    uint8_t sourceCommandOffset;
    uint8_t angle;
    uint8_t rawX;
    uint8_t rawY;
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
 * Recovered legacy behavior does not write a save file here. It only stores a
 * future map name, destination x/y and angle in Game_t; Game_saveState()
 * consumes that route later. This native owner therefore performs no SD/PAK
 * I/O, no serialization, no map transition and no legacy Game mutation.
 *
 * The old destination name buffer was 32 bytes. Native code fails closed for a
 * source string whose payload would not fit as a terminated legacy-compatible
 * value (<32 bytes), while storing valid names as EspMapStringRef without a
 * copy.
 */
EspMapSaveRouteStatus EspMapSaveRoute_apply(
    EspMapSaveRouteState* state,
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapSaveRouteResult* outResult);

#ifdef __cplusplus
}
#endif

#endif
