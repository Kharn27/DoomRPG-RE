#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "esp_map_facing_index.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_runtime.h"

#define ENTITY_DEF_RECORD_BYTES 24U
#define ENTITY_DEF_MAX_COUNT 1024U
#define ENTITY_DEF_LOOKUP_LIMIT 817U
#define LINE_ENTITY_LOOKUP_BASE 305U
#define LINE_ENTITY_LOOKUP_COUNT (ENTITY_DEF_LOOKUP_LIMIT - LINE_ENTITY_LOOKUP_BASE)
#define LINE_ENTITY_FALLBACK_FLAGS 0x00000018UL

static uint8_t* lineEntityTypes;
static EspMapFacingIndexView facingIndexView;

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (data == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

void EspMapFacingIndex_reset(void) {
    if (lineEntityTypes != NULL) heap_caps_free(lineEntityTypes);
    lineEntityTypes = NULL;
    memset(&facingIndexView, 0, sizeof(facingIndexView));
}

int EspMapFacingIndex_buildFromRuntime(const EspAssetPackEntry* entityDefsEntry) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint8_t typeByTexture[LINE_ENTITY_LOOKUP_COUNT];
    uint8_t header[2];
    uint8_t record[ENTITY_DEF_RECORD_BYTES];
    uint32_t defCount;
    uint32_t sourceBytes;
    uint32_t lookup;
    uint32_t i;
    uint16_t texture;
    uint8_t type;
    EspMapLine line;

    if (runtime == NULL || entityDefsEntry == NULL ||
        (entityDefsEntry->flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        !EspAssetPack_isOpen() || runtime->lineCount == 0U ||
        !EspMapLineTextureState_isReady() ||
        !EspAssetPack_readRange(entityDefsEntry, 0U, header, sizeof(header))) {
        return 0;
    }

    defCount = readLe16(header);
    if (defCount == 0U || defCount > ENTITY_DEF_MAX_COUNT) return 0;
    sourceBytes = 2U + defCount * ENTITY_DEF_RECORD_BYTES;
    if (sourceBytes > entityDefsEntry->size) return 0;

    memset(typeByTexture, ESP_MAP_FACING_LINE_NO_ENTITY,
           sizeof(typeByTexture));
    for (i = 0U; i < defCount; ++i) {
        if (!EspAssetPack_readRange(entityDefsEntry,
                                    2U + i * ENTITY_DEF_RECORD_BYTES,
                                    record, sizeof(record))) {
            return 0;
        }
        lookup = readLe16(record);
        if (lookup >= LINE_ENTITY_LOOKUP_BASE &&
            lookup < ENTITY_DEF_LOOKUP_LIMIT &&
            typeByTexture[lookup - LINE_ENTITY_LOOKUP_BASE] ==
                ESP_MAP_FACING_LINE_NO_ENTITY) {
            typeByTexture[lookup - LINE_ENTITY_LOOKUP_BASE] = record[2];
        }
    }

    if (lineEntityTypes == NULL || facingIndexView.lineCount != runtime->lineCount) {
        EspMapFacingIndex_reset();
        lineEntityTypes =
            (uint8_t*)heap_caps_malloc(runtime->lineCount, MALLOC_CAP_8BIT);
        if (lineEntityTypes == NULL) return 0;
    }

    memset(lineEntityTypes, ESP_MAP_FACING_LINE_NO_ENTITY, runtime->lineCount);
    memset(&facingIndexView, 0, sizeof(facingIndexView));
    facingIndexView.lineEntityTypes = lineEntityTypes;
    facingIndexView.lineCount = runtime->lineCount;
    facingIndexView.storageBytes = runtime->lineCount;
    facingIndexView.entityDefCount = defCount;

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line) ||
            !EspMapLineTextureState_getEffectiveTexture(i, &texture)) {
            EspMapFacingIndex_reset();
            return 0;
        }

        type = ESP_MAP_FACING_LINE_NO_ENTITY;
        if (texture < LINE_ENTITY_LOOKUP_COUNT &&
            typeByTexture[texture] != ESP_MAP_FACING_LINE_NO_ENTITY) {
            type = typeByTexture[texture];
        }
        else if ((line.flags & LINE_ENTITY_FALLBACK_FLAGS) != 0U) {
            type = ESP_MAP_FACING_DEFAULT_WALL_TYPE;
        }

        lineEntityTypes[i] = type;
        if (type != ESP_MAP_FACING_LINE_NO_ENTITY) {
            ++facingIndexView.lineEntityCount;
        }
    }

    facingIndexView.stateFNV1a =
        fnv1a32(lineEntityTypes, facingIndexView.storageBytes);
    return facingIndexView.stateFNV1a != 0U;
}

int EspMapFacingIndex_isReady(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    return lineEntityTypes != NULL && runtime != NULL &&
           facingIndexView.lineEntityTypes == lineEntityTypes &&
           facingIndexView.lineCount == runtime->lineCount &&
           facingIndexView.storageBytes == runtime->lineCount &&
           facingIndexView.stateFNV1a != 0U;
}

const EspMapFacingIndexView* EspMapFacingIndex_view(void) {
    return EspMapFacingIndex_isReady() ? &facingIndexView : NULL;
}

int EspMapFacingIndex_getLineEntityType(uint32_t lineIndex, uint8_t* outType) {
    if (!EspMapFacingIndex_isReady() || outType == NULL ||
        lineIndex >= facingIndexView.lineCount) {
        return 0;
    }
    *outType = lineEntityTypes[lineIndex];
    return 1;
}
