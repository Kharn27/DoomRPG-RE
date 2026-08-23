#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_native_graphics_catalog.h"

#define MAPPINGS_HEADER_BYTES 16U
#define MAPPING_PAIR_BYTES 8U
#define PALETTES_HEADER_BYTES 4U
#define PALETTE_BYTES (ESP_NATIVE_GRAPHICS_PALETTE_COLORS * 2U)
#define RESOURCE_ID_COUNT 256U

typedef char EspNativeGraphicsCatalogRecord_must_be_40_bytes[
    sizeof(EspNativeGraphicsCatalogRecord) == 40U ? 1 : -1];

static EspNativeGraphicsCatalogRecord* catalogArena;
static EspNativeGraphicsCatalogView catalogView;

static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t readLe16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t fnvAppend(uint32_t hash, const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static int textureRequired(uint32_t id) {
    return EspMapRuntime_textureRequired(id) ||
           EspMapRuntime_planeTextureUsed(id);
}

static uint16_t countResources(int sprites) {
    uint16_t count = 0U;
    uint32_t id;
    for (id = 0U; id < RESOURCE_ID_COUNT; ++id) {
        int required = sprites ? EspMapRuntime_spriteRequired(id)
                               : textureRequired(id);
        if (required) {
            ++count;
        }
    }
    return count;
}

static EspNativeGraphicsCatalogStatus readRecord(
    const EspAssetPackEntry* mappings,
    const EspAssetPackEntry* palettes,
    uint32_t pairBase,
    uint32_t pairCount,
    uint32_t paletteEntries,
    uint16_t resourceId,
    EspNativeGraphicsCatalogRecord* outRecord) {
    uint8_t pair[MAPPING_PAIR_BYTES];
    uint8_t paletteBytes[PALETTE_BYTES];
    uint32_t pairOffset;
    int32_t sourceOffset;
    int32_t paletteOffset;
    uint32_t i;

    if (mappings == NULL || palettes == NULL || outRecord == NULL ||
        (uint32_t)resourceId >= pairCount) {
        return ESP_NATIVE_GRAPHICS_CATALOG_RESOURCE_UNSUPPORTED;
    }

    pairOffset = pairBase + ((uint32_t)resourceId * MAPPING_PAIR_BYTES);
    if (pairOffset > mappings->size ||
        MAPPING_PAIR_BYTES > mappings->size - pairOffset ||
        !EspAssetPack_readRange(mappings, pairOffset, pair, sizeof(pair))) {
        return ESP_NATIVE_GRAPHICS_CATALOG_READ_FAILED;
    }

    sourceOffset = (int32_t)readLe32(pair);
    paletteOffset = (int32_t)readLe32(pair + 4U);
    if (sourceOffset < 0 || paletteOffset < 0 ||
        (uint32_t)paletteOffset > paletteEntries ||
        ESP_NATIVE_GRAPHICS_PALETTE_COLORS >
            paletteEntries - (uint32_t)paletteOffset) {
        return ESP_NATIVE_GRAPHICS_CATALOG_SOURCE_INVALID;
    }

    if (!EspAssetPack_readRange(
            palettes,
            PALETTES_HEADER_BYTES + ((uint32_t)paletteOffset * 2U),
            paletteBytes,
            sizeof(paletteBytes))) {
        return ESP_NATIVE_GRAPHICS_CATALOG_READ_FAILED;
    }

    memset(outRecord, 0, sizeof(*outRecord));
    outRecord->resourceId = resourceId;
    outRecord->paletteSourceOffset = (uint16_t)paletteOffset;
    outRecord->sourceOffset = (uint32_t)sourceOffset;

    /*
     * palettes.bin itself stores the original RGB565 words. The transitional
     * legacy loader swaps R/B for its historical backend and the ESP32 native
     * palette-normalization step swaps them back. Reading the little-endian
     * source words directly therefore yields the permanent native RGB565 form.
     * The hardware probe cross-checks every selected color against the already
     * normalized transitional Render palette before this legacy dependency is
     * removed from the renderer path.
     */
    for (i = 0U; i < ESP_NATIVE_GRAPHICS_PALETTE_COLORS; ++i) {
        outRecord->paletteRgb565[i] = readLe16(&paletteBytes[i * 2U]);
    }

    return ESP_NATIVE_GRAPHICS_CATALOG_OK;
}

void EspNativeGraphicsCatalog_reset(void) {
    free(catalogArena);
    catalogArena = NULL;
    memset(&catalogView, 0, sizeof(catalogView));
}

int EspNativeGraphicsCatalog_isReady(void) {
    return catalogArena != NULL && catalogView.storageBytes != 0U &&
           catalogView.stateFNV1a != 0U;
}

const EspNativeGraphicsCatalogView* EspNativeGraphicsCatalog_view(void) {
    return EspNativeGraphicsCatalog_isReady() ? &catalogView : NULL;
}

EspNativeGraphicsCatalogStatus EspNativeGraphicsCatalog_buildFromRuntime(void) {
    EspAssetPackEntry mappings;
    EspAssetPackEntry palettes;
    const EspMapRuntimeView* runtime;
    uint8_t mappingHeader[MAPPINGS_HEADER_BYTES];
    uint8_t paletteHeader[PALETTES_HEADER_BYTES];
    uint32_t texelPairs;
    uint32_t bitShapePairs;
    uint32_t textureIdCount;
    uint32_t spriteIdCount;
    uint32_t paletteBytes;
    uint32_t paletteEntries;
    uint64_t expectedMappingsBytes;
    uint32_t spritePairBase;
    uint16_t textureCount;
    uint16_t spriteCount;
    uint32_t totalCount;
    uint32_t storageBytes;
    uint32_t textureIndex = 0U;
    uint32_t spriteIndex = 0U;
    uint32_t id;
    uint32_t hash;
    uint16_t counts[2];
    EspNativeGraphicsCatalogStatus status = ESP_NATIVE_GRAPHICS_CATALOG_INVALID;

    if (EspNativeGraphicsCatalog_isReady()) {
        return ESP_NATIVE_GRAPHICS_CATALOG_ALREADY_ACTIVE;
    }
    runtime = EspMapRuntime_view();
    if (runtime == NULL || !EspMapRuntime_isLoaded() ||
        runtime->arena == NULL || runtime->arenaBytes == 0U) {
        return ESP_NATIVE_GRAPHICS_CATALOG_RUNTIME_NOT_READY;
    }
    if (EspAssetPack_isOpen()) {
        return ESP_NATIVE_GRAPHICS_CATALOG_PACK_BUSY;
    }

    textureCount = countResources(0);
    spriteCount = countResources(1);
    totalCount = (uint32_t)textureCount + (uint32_t)spriteCount;
    if (textureCount == 0U || totalCount == 0U ||
        totalCount > UINT32_MAX / (uint32_t)sizeof(*catalogArena)) {
        return ESP_NATIVE_GRAPHICS_CATALOG_SOURCE_INVALID;
    }
    storageBytes = totalCount * (uint32_t)sizeof(*catalogArena);

    catalogArena = (EspNativeGraphicsCatalogRecord*)malloc(storageBytes);
    if (catalogArena == NULL) {
        return ESP_NATIVE_GRAPHICS_CATALOG_ALLOC_FAILED;
    }
    memset(catalogArena, 0, storageBytes);

    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        status = ESP_NATIVE_GRAPHICS_CATALOG_PACK_OPEN_FAILED;
        goto fail;
    }
    if (!EspAssetPack_findEntry("mappings.bin", &mappings) ||
        !EspAssetPack_findEntry("palettes.bin", &palettes)) {
        status = ESP_NATIVE_GRAPHICS_CATALOG_SOURCE_MISSING;
        goto fail_pack;
    }
    if (mappings.size < MAPPINGS_HEADER_BYTES ||
        palettes.size < PALETTES_HEADER_BYTES ||
        !EspAssetPack_readRange(&mappings, 0U, mappingHeader,
                                sizeof(mappingHeader)) ||
        !EspAssetPack_readRange(&palettes, 0U, paletteHeader,
                                sizeof(paletteHeader))) {
        status = ESP_NATIVE_GRAPHICS_CATALOG_READ_FAILED;
        goto fail_pack;
    }

    texelPairs = readLe32(mappingHeader);
    bitShapePairs = readLe32(mappingHeader + 4U);
    textureIdCount = readLe32(mappingHeader + 8U);
    spriteIdCount = readLe32(mappingHeader + 12U);
    paletteBytes = readLe32(paletteHeader);

    if (texelPairs == 0U || bitShapePairs == 0U ||
        texelPairs > 4096U || bitShapePairs > 4096U ||
        textureIdCount > 4096U || spriteIdCount > 4096U ||
        (paletteBytes & 1U) != 0U || paletteBytes < PALETTE_BYTES ||
        paletteBytes > palettes.size - PALETTES_HEADER_BYTES ||
        paletteBytes + PALETTES_HEADER_BYTES != palettes.size) {
        status = ESP_NATIVE_GRAPHICS_CATALOG_SOURCE_INVALID;
        goto fail_pack;
    }

    expectedMappingsBytes =
        (uint64_t)MAPPINGS_HEADER_BYTES +
        ((uint64_t)texelPairs * MAPPING_PAIR_BYTES) +
        ((uint64_t)bitShapePairs * MAPPING_PAIR_BYTES) +
        ((uint64_t)textureIdCount * 2U) +
        ((uint64_t)spriteIdCount * 2U);
    if (expectedMappingsBytes != mappings.size ||
        expectedMappingsBytes > UINT32_MAX) {
        status = ESP_NATIVE_GRAPHICS_CATALOG_SOURCE_INVALID;
        goto fail_pack;
    }

    paletteEntries = paletteBytes / 2U;
    spritePairBase = MAPPINGS_HEADER_BYTES + texelPairs * MAPPING_PAIR_BYTES;

    for (id = 0U; id < RESOURCE_ID_COUNT; ++id) {
        if (textureRequired(id)) {
            status = readRecord(&mappings, &palettes,
                                MAPPINGS_HEADER_BYTES, texelPairs,
                                paletteEntries, (uint16_t)id,
                                &catalogArena[textureIndex]);
            if (status != ESP_NATIVE_GRAPHICS_CATALOG_OK) {
                goto fail_pack;
            }
            ++textureIndex;
        }
    }
    for (id = 0U; id < RESOURCE_ID_COUNT; ++id) {
        if (EspMapRuntime_spriteRequired(id)) {
            status = readRecord(&mappings, &palettes,
                                spritePairBase, bitShapePairs,
                                paletteEntries, (uint16_t)id,
                                &catalogArena[(uint32_t)textureCount + spriteIndex]);
            if (status != ESP_NATIVE_GRAPHICS_CATALOG_OK) {
                goto fail_pack;
            }
            ++spriteIndex;
        }
    }

    if (textureIndex != textureCount || spriteIndex != spriteCount) {
        status = ESP_NATIVE_GRAPHICS_CATALOG_SOURCE_INVALID;
        goto fail_pack;
    }

    EspAssetPack_close();

    catalogView.textures = catalogArena;
    catalogView.sprites = catalogArena + textureCount;
    catalogView.textureCount = textureCount;
    catalogView.spriteCount = spriteCount;
    catalogView.storageBytes = storageBytes;

    counts[0] = textureCount;
    counts[1] = spriteCount;
    hash = fnvAppend(2166136261U, counts, sizeof(counts));
    hash = fnvAppend(hash, catalogArena, storageBytes);
    catalogView.stateFNV1a = hash;
    return ESP_NATIVE_GRAPHICS_CATALOG_OK;

fail_pack:
    EspAssetPack_close();
fail:
    EspNativeGraphicsCatalog_reset();
    return status;
}

static const EspNativeGraphicsCatalogRecord* findRecord(
    const EspNativeGraphicsCatalogRecord* records,
    uint16_t count,
    uint16_t resourceId) {
    uint16_t lo = 0U;
    uint16_t hi = count;
    while (lo < hi) {
        uint16_t mid = (uint16_t)(lo + (uint16_t)((hi - lo) / 2U));
        uint16_t id = records[mid].resourceId;
        if (id == resourceId) return &records[mid];
        if (id < resourceId) lo = (uint16_t)(mid + 1U);
        else hi = mid;
    }
    return NULL;
}

const EspNativeGraphicsCatalogRecord*
EspNativeGraphicsCatalog_findTexture(uint16_t resourceId) {
    if (!EspNativeGraphicsCatalog_isReady()) return NULL;
    return findRecord(catalogView.textures, catalogView.textureCount, resourceId);
}

const EspNativeGraphicsCatalogRecord*
EspNativeGraphicsCatalog_findSprite(uint16_t resourceId) {
    if (!EspNativeGraphicsCatalog_isReady()) return NULL;
    return findRecord(catalogView.sprites, catalogView.spriteCount, resourceId);
}
