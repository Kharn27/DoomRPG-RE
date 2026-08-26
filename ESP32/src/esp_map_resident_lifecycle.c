#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "esp_asset_pack.h"
#include "esp_entity_def_type_catalog.h"
#include "esp_map_automap_state.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_state.h"

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

static void clearSnapshot(EspMapResidentSnapshot* snapshot) {
    if (snapshot != NULL) memset(snapshot, 0, sizeof(*snapshot));
}

void EspMapResidentLifecycle_resetAll(void) {
    const int beforeRuntime = EspMapRuntime_isLoaded();
    const int beforeMap = EspMapState_isReady();
    const int beforeScript = EspMapScriptState_isReady();
    const int beforeLine = EspMapLineState_isReady();
    const int beforeTexture = EspMapLineTextureState_isReady();
    const int beforeAutomap = EspMapAutomapState_isReady();
    const int beforeTopology = EspMapSpriteTopology_isReady();
    const uint32_t heapBefore =
        (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint32_t heapAfter;

    EspMapSpriteTopology_reset();
    EspMapAutomapState_reset();
    EspMapLineTextureState_reset();
    EspMapLineState_reset();
    EspMapScriptState_reset();
    EspMapState_reset();
    EspMapRuntime_reset();

    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);

    /* Temporary integrated-run diagnostic. The reversible handoff milestone
     * proved an exact 18008 B Entrance allocator release in isolation. Surface
     * the primitive's actual owner/heap transition without changing reset
     * semantics so a later fast-forward regression cannot be guessed around.
     */
    if (beforeRuntime || beforeMap || beforeScript || beforeLine ||
        beforeTexture || beforeAutomap || beforeTopology) {
        printf("[RESIDENTRESET] heap8=%u->%u released=%d before=%d/%d/%d/%d/%d/%d/%d after=%d/%d/%d/%d/%d/%d/%d empty=%d\n",
               (unsigned int)heapBefore,
               (unsigned int)heapAfter,
               (int)heapAfter - (int)heapBefore,
               beforeRuntime, beforeMap, beforeScript, beforeLine,
               beforeTexture, beforeAutomap, beforeTopology,
               EspMapRuntime_isLoaded(), EspMapState_isReady(),
               EspMapScriptState_isReady(), EspMapLineState_isReady(),
               EspMapLineTextureState_isReady(), EspMapAutomapState_isReady(),
               EspMapSpriteTopology_isReady(),
               EspMapResidentLifecycle_isEmpty());
    }
}

int EspMapResidentLifecycle_isEmpty(void) {
    return !EspMapRuntime_isLoaded() &&
           !EspMapState_isReady() &&
           !EspMapScriptState_isReady() &&
           !EspMapLineState_isReady() &&
           !EspMapLineTextureState_isReady() &&
           !EspMapAutomapState_isReady() &&
           !EspMapSpriteTopology_isReady();
}

int EspMapResidentLifecycle_capture(EspMapResidentSnapshot* outSnapshot) {
    const EspMapRuntimeView* runtime;
    const EspMapStateView* mapState;
    const EspMapScriptStateView* scriptState;
    const EspMapLineStateView* lineState;
    const EspMapLineTextureStateView* textureState;
    const EspMapAutomapStateView* automapState;
    const EspMapSpriteTopologyView* topology;
    uint64_t total;

    clearSnapshot(outSnapshot);
    if (outSnapshot == NULL) return 0;

    runtime = EspMapRuntime_view();
    mapState = EspMapState_view();
    scriptState = EspMapScriptState_view();
    lineState = EspMapLineState_view();
    textureState = EspMapLineTextureState_view();
    automapState = EspMapAutomapState_view();
    topology = EspMapSpriteTopology_view();

    if (runtime == NULL || mapState == NULL || scriptState == NULL ||
        lineState == NULL || textureState == NULL || automapState == NULL ||
        topology == NULL || runtime->arena == NULL ||
        runtime->arenaBytes == 0U || runtime->lineCount == 0U ||
        runtime->mapSpriteCount == 0U || runtime->eventCount == 0U ||
        runtime->byteCodeCount == 0U || runtime->stringCount == 0U ||
        mapState->tileCount != ESP_MAP_STATE_TILE_COUNT ||
        scriptState->eventCount != runtime->eventCount ||
        scriptState->byteCodeCount != runtime->byteCodeCount ||
        lineState->lineCount != runtime->lineCount ||
        textureState->lineCount != runtime->lineCount ||
        automapState->lineCount != runtime->lineCount ||
        automapState->spriteCount != runtime->mapSpriteCount ||
        topology->spriteCount != runtime->mapSpriteCount ||
        scriptState->storage == NULL || scriptState->storageBytes == 0U) {
        return 0;
    }

    total = (uint64_t)runtime->arenaBytes +
            (uint64_t)ESP_MAP_STATE_BYTES +
            (uint64_t)scriptState->storageBytes +
            (uint64_t)lineState->storageBytes +
            (uint64_t)textureState->storageBytes +
            (uint64_t)automapState->storageBytes +
            (uint64_t)topology->storageBytes;
    if (total > UINT32_MAX) return 0;

    outSnapshot->runtimeArenaBytes = runtime->arenaBytes;
    outSnapshot->mapStateBytes = ESP_MAP_STATE_BYTES;
    outSnapshot->scriptStateBytes = scriptState->storageBytes;
    outSnapshot->lineStateBytes = lineState->storageBytes;
    outSnapshot->textureStateBytes = textureState->storageBytes;
    outSnapshot->automapStateBytes = automapState->storageBytes;
    outSnapshot->topologyBytes = topology->storageBytes;
    outSnapshot->totalPayloadBytes = (uint32_t)total;

    outSnapshot->runtimeFNV1a = runtime->arenaFNV1a;
    outSnapshot->mapStateFNV1a = mapState->stateFNV1a;
    outSnapshot->scriptStateFNV1a =
        fnv1a32(scriptState->storage, scriptState->storageBytes);
    outSnapshot->lineStateFNV1a = lineState->stateFNV1a;
    outSnapshot->textureStateFNV1a = textureState->stateFNV1a;
    outSnapshot->automapStateFNV1a = automapState->stateFNV1a;
    outSnapshot->topologyFNV1a = topology->stateFNV1a;

    outSnapshot->nodeCount = runtime->nodeCount;
    outSnapshot->lineCount = runtime->lineCount;
    outSnapshot->spriteCount = runtime->mapSpriteCount;
    outSnapshot->eventCount = runtime->eventCount;
    outSnapshot->byteCodeCount = runtime->byteCodeCount;
    outSnapshot->stringCount = runtime->stringCount;

    outSnapshot->entityCount = topology->entityCount;
    outSnapshot->enemyCount = topology->enemyCount;
    outSnapshot->destructibleCount = topology->destructibleCount;
    return 1;
}

int EspMapResidentLifecycle_isReady(void) {
    EspMapResidentSnapshot snapshot;
    return EspMapResidentLifecycle_capture(&snapshot);
}

EspMapResidentLifecycleStatus EspMapResidentLifecycle_loadFromEmpty(
    const char* resourceName,
    const EspBspInventory* inventory,
    EspMapResidentSnapshot* outSnapshot) {
    EspAssetPackEntry entityDefsEntry;
    EspMapResidentLifecycleStatus status = ESP_MAP_RESIDENT_RUNTIME_FAILED;

    clearSnapshot(outSnapshot);
    if (resourceName == NULL || resourceName[0] == '\0' ||
        inventory == NULL || outSnapshot == NULL) {
        return ESP_MAP_RESIDENT_INVALID;
    }
    if (!EspMapResidentLifecycle_isEmpty()) {
        return ESP_MAP_RESIDENT_NOT_EMPTY;
    }
    if (EspAssetPack_isOpen()) {
        return ESP_MAP_RESIDENT_PACK_BUSY;
    }
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        return ESP_MAP_RESIDENT_PACK_OPEN_FAILED;
    }

    memset(&entityDefsEntry, 0, sizeof(entityDefsEntry));
    if (!EspAssetPack_findEntry(ESP_MAP_RESIDENT_ENTITY_DEFS_RESOURCE,
                                &entityDefsEntry) ||
        (entityDefsEntry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        entityDefsEntry.size == 0U) {
        status = ESP_MAP_RESIDENT_ENTITY_DEFS_MISSING;
        goto done;
    }
    if (!EspEntityDefTypeCatalog_buildFromPackEntry(&entityDefsEntry)) {
        status = ESP_MAP_RESIDENT_TOPOLOGY_FAILED;
        goto done;
    }

    if (!EspMapRuntime_loadPackEntry(resourceName, inventory)) {
        status = ESP_MAP_RESIDENT_RUNTIME_FAILED;
        goto done;
    }
    if (!EspMapState_buildFromRuntime()) {
        status = ESP_MAP_RESIDENT_MAP_STATE_FAILED;
        goto done;
    }
    if (!EspMapScriptState_buildFromRuntime()) {
        status = ESP_MAP_RESIDENT_SCRIPT_STATE_FAILED;
        goto done;
    }
    if (!EspMapLineState_buildFromRuntime()) {
        status = ESP_MAP_RESIDENT_LINE_STATE_FAILED;
        goto done;
    }
    if (!EspMapLineTextureState_buildFromRuntime()) {
        status = ESP_MAP_RESIDENT_TEXTURE_STATE_FAILED;
        goto done;
    }
    if (!EspMapAutomapState_buildFromRuntime()) {
        status = ESP_MAP_RESIDENT_AUTOMAP_STATE_FAILED;
        goto done;
    }
    if (!EspMapSpriteTopology_buildFromRuntime(&entityDefsEntry)) {
        status = ESP_MAP_RESIDENT_TOPOLOGY_FAILED;
        goto done;
    }
    if (!EspMapResidentLifecycle_capture(outSnapshot)) {
        status = ESP_MAP_RESIDENT_SNAPSHOT_FAILED;
        goto done;
    }

    status = ESP_MAP_RESIDENT_OK;

done:
    EspAssetPack_close();
    if (status != ESP_MAP_RESIDENT_OK) {
        EspMapResidentLifecycle_resetAll();
        clearSnapshot(outSnapshot);
    }
    return status;
}
