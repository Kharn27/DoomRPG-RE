#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_entity_def_type_catalog.h"

#define ENTITY_DEF_RECORD_BYTES 24U
#define ENTITY_DEF_MAX_COUNT 1024U
#define ENTITY_DEF_LINE_ENTRANCE_TILE_INDEX 312U

typedef struct EntityDefMetadata_s {
    uint16_t tileIndex;
    uint8_t type;
    uint8_t subtype;
    int32_t parm;
} EntityDefMetadata;

typedef char EntityDefMetadata_must_be_8_bytes[
    sizeof(EntityDefMetadata) == 8U ? 1 : -1];

static EntityDefMetadata
    entityDefMetadata[ESP_ENTITY_DEF_TYPE_CATALOG_MAX_DEFINITIONS];
static uint32_t entityDefCount;
static uint16_t entityDefMetadataCount;
static uint8_t entityDefTypesReady;

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int32_t readLe32s(const uint8_t* p) {
    uint32_t value = (uint32_t)p[0] |
                     ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) |
                     ((uint32_t)p[3] << 24);
    return (int32_t)value;
}

static void clearCatalog(void) {
    memset(entityDefMetadata, 0, sizeof(entityDefMetadata));
    entityDefCount = 0U;
    entityDefMetadataCount = 0U;
    entityDefTypesReady = 0U;
}

static int insertMetadata(uint16_t tileIndex,
                          uint8_t type,
                          uint8_t subtype,
                          int32_t parm) {
    uint16_t pos = 0U;

    while (pos < entityDefMetadataCount &&
           entityDefMetadata[pos].tileIndex < tileIndex) {
        ++pos;
    }
    /* Legacy type-only catalog retained the first definition for duplicate
     * tile indices. Preserve that exact first-wins rule for all metadata. */
    if (pos < entityDefMetadataCount &&
        entityDefMetadata[pos].tileIndex == tileIndex) {
        return 1;
    }
    if (entityDefMetadataCount >= ESP_ENTITY_DEF_TYPE_CATALOG_MAX_DEFINITIONS) {
        return 0;
    }
    if (pos < entityDefMetadataCount) {
        memmove(&entityDefMetadata[pos + 1U],
                &entityDefMetadata[pos],
                (size_t)(entityDefMetadataCount - pos) *
                    sizeof(entityDefMetadata[0]));
    }
    entityDefMetadata[pos].tileIndex = tileIndex;
    entityDefMetadata[pos].type = type;
    entityDefMetadata[pos].subtype = subtype;
    entityDefMetadata[pos].parm = parm;
    ++entityDefMetadataCount;
    return 1;
}

static const EntityDefMetadata* findMetadata(uint16_t tileIndex) {
    uint16_t lo = 0U;
    uint16_t hi = entityDefMetadataCount;

    if (!entityDefTypesReady || tileIndex >= ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT) {
        return NULL;
    }
    while (lo < hi) {
        uint16_t mid = (uint16_t)(lo + ((hi - lo) >> 1));
        uint16_t candidate = entityDefMetadata[mid].tileIndex;
        if (candidate < tileIndex) lo = (uint16_t)(mid + 1U);
        else hi = mid;
    }
    if (lo >= entityDefMetadataCount ||
        entityDefMetadata[lo].tileIndex != tileIndex) {
        return NULL;
    }
    return &entityDefMetadata[lo];
}

int EspEntityDefTypeCatalog_buildFromPackEntry(
    const EspAssetPackEntry* entityDefsEntry) {
    uint8_t header[2];
    uint8_t record[ENTITY_DEF_RECORD_BYTES];
    uint32_t count;
    uint32_t sourceBytes;
    uint32_t i;
    const EntityDefMetadata* entrance;

    if (entityDefTypesReady) return 1;
    if (entityDefsEntry == NULL ||
        (entityDefsEntry->flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        !EspAssetPack_isOpen() ||
        !EspAssetPack_readRange(entityDefsEntry, 0U, header, sizeof(header))) {
        return 0;
    }

    count = readLe16(header);
    if (count == 0U || count > ENTITY_DEF_MAX_COUNT ||
        count > ESP_ENTITY_DEF_TYPE_CATALOG_MAX_DEFINITIONS) {
        return 0;
    }
    sourceBytes = 2U + (count * ENTITY_DEF_RECORD_BYTES);
    if (sourceBytes > entityDefsEntry->size) return 0;

    clearCatalog();
    for (i = 0U; i < count; ++i) {
        uint16_t tileIndex;
        if (!EspAssetPack_readRange(entityDefsEntry,
                                    2U + (i * ENTITY_DEF_RECORD_BYTES),
                                    record, sizeof(record))) {
            clearCatalog();
            return 0;
        }
        tileIndex = readLe16(record);
        if (tileIndex >= ESP_ENTITY_DEF_TYPE_CATALOG_LIMIT) continue;
        if (!insertMetadata(tileIndex, record[2], record[3],
                            readLe32s(record + 4U))) {
            clearCatalog();
            return 0;
        }
    }

    entityDefCount = count;
    entityDefTypesReady = 1U;
    entrance = findMetadata(ENTITY_DEF_LINE_ENTRANCE_TILE_INDEX);
    printf("[ENTITYDEFTYPE] READY defs=%u metadata=%u cache=%uB recordBytes=%u tile312=%s%u subtype=%s%u parm=%ld\n",
           (unsigned int)entityDefCount,
           (unsigned int)entityDefMetadataCount,
           (unsigned int)sizeof(entityDefMetadata),
           (unsigned int)sizeof(entityDefMetadata[0]),
           entrance == NULL ? "none/" : "type/",
           (unsigned int)(entrance == NULL ? 0xffU : entrance->type),
           entrance == NULL ? "none/" : "subtype/",
           (unsigned int)(entrance == NULL ? 0xffU : entrance->subtype),
           (long)(entrance == NULL ? 0 : entrance->parm));
    return 1;
}

int EspEntityDefTypeCatalog_isReady(void) {
    return entityDefTypesReady != 0U;
}

int EspEntityDefTypeCatalog_getMetadata(uint16_t tileIndex,
                                        uint8_t* outType,
                                        uint8_t* outSubtype,
                                        int32_t* outParm) {
    const EntityDefMetadata* metadata;
    if (outType == NULL || outSubtype == NULL || outParm == NULL) return 0;
    metadata = findMetadata(tileIndex);
    if (metadata == NULL) return 0;
    *outType = metadata->type;
    *outSubtype = metadata->subtype;
    *outParm = metadata->parm;
    return 1;
}

int EspEntityDefTypeCatalog_getTypeAndSubtype(uint16_t tileIndex,
                                              uint8_t* outType,
                                              uint8_t* outSubtype) {
    const EntityDefMetadata* metadata;
    if (outType == NULL || outSubtype == NULL) return 0;
    metadata = findMetadata(tileIndex);
    if (metadata == NULL) return 0;
    *outType = metadata->type;
    *outSubtype = metadata->subtype;
    return 1;
}

int EspEntityDefTypeCatalog_getType(uint16_t tileIndex, uint8_t* outType) {
    const EntityDefMetadata* metadata;
    if (outType == NULL) return 0;
    metadata = findMetadata(tileIndex);
    if (metadata == NULL) return 0;
    *outType = metadata->type;
    return 1;
}

int EspEntityDefTypeCatalog_getSubtype(uint16_t tileIndex,
                                       uint8_t* outSubtype) {
    const EntityDefMetadata* metadata;
    if (outSubtype == NULL) return 0;
    metadata = findMetadata(tileIndex);
    if (metadata == NULL) return 0;
    *outSubtype = metadata->subtype;
    return 1;
}

int EspEntityDefTypeCatalog_getParm(uint16_t tileIndex, int32_t* outParm) {
    const EntityDefMetadata* metadata;
    if (outParm == NULL) return 0;
    metadata = findMetadata(tileIndex);
    if (metadata == NULL) return 0;
    *outParm = metadata->parm;
    return 1;
}

uint32_t EspEntityDefTypeCatalog_definitionCount(void) {
    return entityDefTypesReady ? entityDefCount : 0U;
}
