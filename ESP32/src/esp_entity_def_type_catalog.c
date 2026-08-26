#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_entity_def_type_catalog.h"

#define ENTITY_DEF_RECORD_BYTES 24U
#define ENTITY_DEF_MAX_COUNT 1024U
#define ENTITY_DEF_LINE_ENTRANCE_TILE_INDEX 312U

static uint8_t entityDefTypes[ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT];
static uint32_t entityDefCount;
static uint8_t entityDefTypesReady;

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

int EspEntityDefTypeCatalog_buildFromPackEntry(
    const EspAssetPackEntry* entityDefsEntry) {
    uint8_t header[2];
    uint8_t record[ENTITY_DEF_RECORD_BYTES];
    uint32_t count;
    uint32_t sourceBytes;
    uint32_t i;
    uint16_t tileIndex;
    uint8_t entranceType = 0xffU;

    if (entityDefTypesReady) return 1;
    if (entityDefsEntry == NULL ||
        (entityDefsEntry->flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        !EspAssetPack_isOpen() ||
        !EspAssetPack_readRange(entityDefsEntry, 0U, header, sizeof(header))) {
        return 0;
    }

    count = readLe16(header);
    if (count == 0U || count > ENTITY_DEF_MAX_COUNT) return 0;
    sourceBytes = 2U + (count * ENTITY_DEF_RECORD_BYTES);
    if (sourceBytes > entityDefsEntry->size) return 0;

    memset(entityDefTypes, 0xff, sizeof(entityDefTypes));
    for (i = 0U; i < count; ++i) {
        if (!EspAssetPack_readRange(entityDefsEntry,
                                    2U + (i * ENTITY_DEF_RECORD_BYTES),
                                    record, sizeof(record))) {
            memset(entityDefTypes, 0xff, sizeof(entityDefTypes));
            entityDefCount = 0U;
            return 0;
        }
        tileIndex = readLe16(record);
        if (tileIndex < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT &&
            entityDefTypes[tileIndex] == 0xffU) {
            entityDefTypes[tileIndex] = record[2];
        }
    }

    entityDefCount = count;
    entityDefTypesReady = 1U;
    if (ENTITY_DEF_LINE_ENTRANCE_TILE_INDEX < ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT) {
        entranceType = entityDefTypes[ENTITY_DEF_LINE_ENTRANCE_TILE_INDEX];
    }
    printf("[ENTITYDEFTYPE] READY defs=%u cache=%uB tile312=%s%u\n",
           (unsigned int)entityDefCount,
           (unsigned int)sizeof(entityDefTypes),
           entranceType == 0xffU ? "none/" : "type/",
           (unsigned int)entranceType);
    return 1;
}

int EspEntityDefTypeCatalog_isReady(void) {
    return entityDefTypesReady != 0U;
}

int EspEntityDefTypeCatalog_getType(uint16_t tileIndex, uint8_t* outType) {
    if (!entityDefTypesReady || outType == NULL ||
        tileIndex >= ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT ||
        entityDefTypes[tileIndex] == 0xffU) {
        return 0;
    }
    *outType = entityDefTypes[tileIndex];
    return 1;
}

uint32_t EspEntityDefTypeCatalog_definitionCount(void) {
    return entityDefTypesReady ? entityDefCount : 0U;
}
