#ifndef DOOMRPG_ESP32_ENTITY_DEF_TYPE_CATALOG_H
#define DOOMRPG_ESP32_ENTITY_DEF_TYPE_CATALOG_H

#include <stdint.h>

#include "esp_asset_pack.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Highest native lookup tile currently addressable by Doom RPG entity defs.
 * This remains an API validation bound; the implementation no longer burns
 * one byte for every sparse tile index. */
#define ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT 817U
#define ESP_ENTITY_DEF_TYPE_CATALOG_MAX_DEFINITIONS 128U

/*
 * Global immutable compact metadata view of /entities.db.
 *
 * Doom RPG line entities resolve their definition with tileIndex
 * 305 + line.texture before being linked into Game.entityDb. Native gameplay
 * needs both eType and selected eSubType behavior while shapeData remains NULL.
 * The implementation retains only sorted {tileIndex,type,subtype} records and
 * performs allocation-free binary lookup; no names, parm values or legacy
 * EntityDef_t pointers survive startup.
 */
int EspEntityDefTypeCatalog_buildFromPackEntry(
    const EspAssetPackEntry* entityDefsEntry);
int EspEntityDefTypeCatalog_isReady(void);
int EspEntityDefTypeCatalog_getType(uint16_t tileIndex, uint8_t* outType);
int EspEntityDefTypeCatalog_getSubtype(uint16_t tileIndex, uint8_t* outSubtype);
int EspEntityDefTypeCatalog_getTypeAndSubtype(uint16_t tileIndex,
                                              uint8_t* outType,
                                              uint8_t* outSubtype);
uint32_t EspEntityDefTypeCatalog_definitionCount(void);

#ifdef __cplusplus
}
#endif

#endif
