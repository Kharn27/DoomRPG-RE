#include <string.h>

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"
#include "esp_map_catalog.h"
#include "esp_map_transition_preflight.h"

static void clearResult(EspMapTransitionPreflightResult* result) {
    if (result != NULL) memset(result, 0, sizeof(*result));
}

EspMapTransitionPreflightStatus EspMapTransitionPreflight_run(
    uint8_t targetMapId,
    EspMapTransitionPreflightResult* outResult) {
    const char* resourceName;
    EspAssetPackEntry entry;
    EspBspInventory inventory;
    EspMapTransitionPreflightStatus status = ESP_MAP_TRANSITION_PREFLIGHT_BSP_INVALID;

    clearResult(outResult);
    if (outResult == NULL || !EspMapCatalog_isValidId(targetMapId)) {
        return ESP_MAP_TRANSITION_PREFLIGHT_INVALID;
    }
    if (EspAssetPack_isOpen()) {
        return ESP_MAP_TRANSITION_PREFLIGHT_PACK_BUSY;
    }

    resourceName = EspMapCatalog_nameForId(targetMapId);
    if (resourceName == NULL) {
        return ESP_MAP_TRANSITION_PREFLIGHT_INVALID;
    }
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return ESP_MAP_TRANSITION_PREFLIGHT_PACK_OPEN_FAILED;
    }

    memset(&entry, 0, sizeof(entry));
    memset(&inventory, 0, sizeof(inventory));

    if (!EspAssetPack_findEntry(resourceName, &entry) ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        entry.size < ESP_BSP_HEADER_BYTES) {
        status = ESP_MAP_TRANSITION_PREFLIGHT_ENTRY_NOT_FOUND;
        goto done;
    }
    if (!EspBspReader_inventoryPackEntry(resourceName, &inventory) ||
        inventory.sourceBytes != entry.size ||
        inventory.consumedBytes != entry.size ||
        inventory.expectedCrc32 != entry.crc32 ||
        inventory.crc32 != entry.crc32) {
        status = ESP_MAP_TRANSITION_PREFLIGHT_BSP_INVALID;
        goto done;
    }
    if (inventory.loadMapId < ESP_MAP_TRANSITION_GAMEPLAY_LOAD_ID_MIN ||
        inventory.loadMapId > ESP_MAP_TRANSITION_GAMEPLAY_LOAD_ID_MAX) {
        status = ESP_MAP_TRANSITION_PREFLIGHT_GAMEPLAY_ID_INVALID;
        goto done;
    }

    outResult->nameHash = EspAssetPack_nameHash(resourceName);
    outResult->entryOffset = entry.offset;
    outResult->sourceBytes = entry.size;
    outResult->sourceCrc32 = entry.crc32;
    outResult->sourceFNV1a = inventory.fnv1a32;
    outResult->persistentPlanBytes = inventory.plan.persistentBytes;
    outResult->nodes = inventory.nodes;
    outResult->lines = inventory.lines;
    outResult->mapSprites = inventory.mapSprites;
    outResult->events = inventory.events;
    outResult->byteCodes = inventory.byteCodes;
    outResult->strings = inventory.strings;
    outResult->stringDataBytes = inventory.stringDataBytes;
    outResult->targetMapId = targetMapId;
    outResult->gameplayLoadMapId = inventory.loadMapId;
    outResult->ready = 1U;
    status = ESP_MAP_TRANSITION_PREFLIGHT_OK;

done:
    EspAssetPack_close();
    if (status != ESP_MAP_TRANSITION_PREFLIGHT_OK) clearResult(outResult);
    return status;
}
