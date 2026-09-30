#include <SDL.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_asset_pack.h"
#include "esp_legacy_asset_source.h"

static int openEntry(const char* name, EspAssetPackEntry* outEntry) {
    if (name == NULL || name[0] == '\0' || outEntry == NULL ||
        EspAssetPack_isOpen()) {
        return 0;
    }
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return 0;
    }
    if (!EspAssetPack_findEntry(name, outEntry) ||
        (outEntry->flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        outEntry->size == 0U || outEntry->size > (uint32_t)INT_MAX) {
        EspAssetPack_close();
        return 0;
    }
    return 1;
}

int EspLegacyAssetSource_validate(uint32_t* outEntryCount) {
    int count;

    if (outEntryCount != NULL) *outEntryCount = 0U;
    if (EspAssetPack_isOpen() ||
        !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return 0;
    }

    count = EspAssetPack_entryCount();
    EspAssetPack_close();
    if (count <= 0) return 0;

    if (outEntryCount != NULL) *outEntryCount = (uint32_t)count;
    return 1;
}

int EspLegacyAssetSource_stat(const char* name, uint32_t* outSize) {
    EspAssetPackEntry entry;

    if (outSize != NULL) *outSize = 0U;
    if (!openEntry(name, &entry)) return 0;

    if (outSize != NULL) *outSize = entry.size;
    EspAssetPack_close();
    return 1;
}

uint8_t* EspLegacyAssetSource_readAlloc(const char* name, int* outSize) {
    EspAssetPackEntry entry;
    uint8_t* data;

    if (outSize != NULL) *outSize = 0;
    if (!openEntry(name, &entry)) return NULL;

    data = (uint8_t*)SDL_malloc((size_t)entry.size);
    if (data == NULL) {
        EspAssetPack_close();
        return NULL;
    }

    if (!EspAssetPack_readRange(&entry, 0U, data, (size_t)entry.size)) {
        SDL_free(data);
        EspAssetPack_close();
        return NULL;
    }

    EspAssetPack_close();
    if (outSize != NULL) *outSize = (int)entry.size;
    return data;
}

int EspLegacyAssetSource_readInto(const char* name,
                                  void* destination,
                                  size_t capacity,
                                  int* outSize) {
    EspAssetPackEntry entry;

    if (outSize != NULL) *outSize = 0;
    if (destination == NULL || !openEntry(name, &entry)) return 0;

    if ((size_t)entry.size > capacity ||
        !EspAssetPack_readRange(&entry, 0U, destination, (size_t)entry.size)) {
        EspAssetPack_close();
        return 0;
    }

    EspAssetPack_close();
    if (outSize != NULL) *outSize = (int)entry.size;
    return 1;
}
