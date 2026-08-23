#ifndef DOOMRPG_ESP32_MAP_CATALOG_H
#define DOOMRPG_ESP32_MAP_CATALOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_CATALOG_FIRST_ID 1U
#define ESP_MAP_CATALOG_LAST_ID 13U
#define ESP_MAP_CATALOG_COUNT 13U

#define ESP_MAP_ID_INTRO 1U
#define ESP_MAP_ID_JUNCTION 9U
#define ESP_MAP_ID_END_GAME 13U

/*
 * Permanent native map identity catalog.
 *
 * IDs deliberately mirror the recovered Doom RPG map IDs, while resource
 * names are the original BSP names stored inside DoomRPG-ESP32.pak. The table
 * itself is immutable program data; callers receive no heap-owned strings.
 */
int EspMapCatalog_isValidId(uint8_t mapId);
const char* EspMapCatalog_nameForId(uint8_t mapId);
int EspMapCatalog_idForName(const char* resourceName, uint8_t* outMapId);

#ifdef __cplusplus
}
#endif

#endif
