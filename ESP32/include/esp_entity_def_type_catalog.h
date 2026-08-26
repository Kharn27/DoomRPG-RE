#ifndef DOOMRPG_ESP32_ENTITY_DEF_TYPE_CATALOG_H
#define DOOMRPG_ESP32_ENTITY_DEF_TYPE_CATALOG_H

#include <stdint.h>

#include "esp_asset_pack.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT 817U

/*
 * Global immutable type-only view of /entities.db.
 *
 * Doom RPG line entities resolve their definition with tileIndex
 * 305 + line.texture before being linked into Game.entityDb. Native gameplay
 * already parses the same database for map-sprite topology; this bounded BSS
 * catalog keeps only the eType byte needed by Game_trace collision semantics.
 * It owns no heap and is independent of the current resident map lifecycle.
 */
int EspEntityDefTypeCatalog_buildFromPackEntry(
    const EspAssetPackEntry* entityDefsEntry);
int EspEntityDefTypeCatalog_isReady(void);
int EspEntityDefTypeCatalog_getType(uint16_t tileIndex, uint8_t* outType);
uint32_t EspEntityDefTypeCatalog_definitionCount(void);

#ifdef __cplusplus
}
#endif

#endif
