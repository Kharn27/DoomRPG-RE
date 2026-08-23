#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_catalog.h"
#include "esp_map_resident_lifecycle.h"
#include "esp_map_runtime.h"
#include "esp_player_spawn_state.h"

static int inventoryIsComplete(const EspBspInventory* inventory) {
    return inventory != NULL && inventory->sourceBytes != 0U &&
           inventory->consumedBytes == inventory->sourceBytes &&
           inventory->structuralEndOffset == inventory->sourceBytes &&
           inventory->sections.endOffset == inventory->sourceBytes &&
           inventory->trailingBytes == 0U && inventory->crc32 != 0U &&
           inventory->expectedCrc32 == inventory->crc32 &&
           inventory->fnv1a32 != 0U && inventory->plan.persistentBytes != 0U;
}

static int inventoryMatchesTransition(
    const EspMapCommittedTransitionState* transition,
    const EspBspInventory* inventory) {
    return transition != NULL && inventoryIsComplete(inventory) &&
           inventory->sourceBytes == transition->targetSourceBytes &&
           inventory->crc32 == transition->targetSourceCrc32 &&
           inventory->fnv1a32 == transition->targetSourceFNV1a &&
           inventory->loadMapId == transition->targetGameplayLoadMapId;
}

static int inventoryMatchesRuntime(const EspBspInventory* inventory) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();

    return inventory != NULL && runtime != NULL &&
           EspMapResidentLifecycle_isReady() &&
           runtime->sourceBytes == inventory->sourceBytes &&
           runtime->sourceCrc32 == inventory->crc32 &&
           runtime->arenaBytes == inventory->plan.persistentBytes &&
           runtime->nodeCount == inventory->nodes &&
           runtime->lineCount == inventory->lines &&
           runtime->mapSpriteCount == inventory->mapSprites &&
           runtime->eventCount == inventory->events &&
           runtime->byteCodeCount == inventory->byteCodes &&
           runtime->stringCount == inventory->strings;
}

void EspPlayerSpawn_reset(EspPlayerSpawnState* state) {
    if (state != NULL) memset(state, 0, sizeof(*state));
}

EspPlayerSpawnStatus EspPlayerSpawn_prepareCommitted(
    const EspMapCommittedTransitionState* transition,
    const EspBspInventory* targetInventory,
    uint8_t loadType,
    uint8_t gameIsLoaded,
    EspPlayerSpawnState* outState) {
    EspPlayerSpawnState next;
    uint32_t spawnParam;
    uint32_t tileX;
    uint32_t tileY;
    uint32_t angle;
    uint32_t tileIndex;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (transition == NULL || targetInventory == NULL || outState == NULL) {
        return ESP_PLAYER_SPAWN_INVALID;
    }
    if (!EspMapCommittedTransition_isCommitted(transition) ||
        transition->phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_COMMITTED ||
        transition->committed != 1U || transition->pendingConsumed != 1U ||
        !EspMapCatalog_isValidId(transition->targetMapId)) {
        return ESP_PLAYER_SPAWN_NOT_COMMITTED;
    }
    if (loadType != ESP_PLAYER_SPAWN_LOAD_FRESH_MAP || gameIsLoaded != 0U) {
        return ESP_PLAYER_SPAWN_UNSUPPORTED_CONTEXT;
    }
    if (!inventoryMatchesTransition(transition, targetInventory)) {
        return ESP_PLAYER_SPAWN_TARGET_MISMATCH;
    }
    if (!inventoryMatchesRuntime(targetInventory)) {
        return ESP_PLAYER_SPAWN_RUNTIME_MISMATCH;
    }

    spawnParam = transition->spawnParam;
    memset(&next, 0, sizeof(next));
    next.sourceSpawnParam = spawnParam;

    if (spawnParam == 0U) {
        if (targetInventory->spawnIndex >= ESP_PLAYER_SPAWN_TILE_COUNT) {
            return ESP_PLAYER_SPAWN_SPAWN_INVALID;
        }
        tileIndex = targetInventory->spawnIndex;
        tileX = tileIndex % ESP_PLAYER_SPAWN_MAP_WIDTH;
        tileY = tileIndex / ESP_PLAYER_SPAWN_MAP_WIDTH;
        angle = targetInventory->spawnDirection;
        next.spawnSource = ESP_PLAYER_SPAWN_SOURCE_HEADER;
        next.overrideUsed = 0U;
    }
    else {
        tileX = spawnParam & 31U;
        tileY = (spawnParam >> 5U) & 31U;
        angle = (spawnParam >> 10U) & 255U;
        tileIndex = tileY * ESP_PLAYER_SPAWN_MAP_WIDTH + tileX;
        next.spawnSource = ESP_PLAYER_SPAWN_SOURCE_OVERRIDE;
        next.overrideUsed = 1U;
    }

    next.tileIndex = (uint16_t)tileIndex;
    next.worldX = (uint16_t)(tileX * ESP_PLAYER_SPAWN_TILE_SIZE +
                             ESP_PLAYER_SPAWN_TILE_CENTER);
    next.worldY = (uint16_t)(tileY * ESP_PLAYER_SPAWN_TILE_SIZE +
                             ESP_PLAYER_SPAWN_TILE_CENTER);
    next.tileX = (uint8_t)tileX;
    next.tileY = (uint8_t)tileY;
    next.angle = (uint8_t)angle;
    next.viewZ = ESP_PLAYER_SPAWN_VIEW_Z;
    next.viewZOld = ESP_PLAYER_SPAWN_VIEW_Z_OLD;
    next.loadType = loadType;
    next.facingRefreshPending = 1U;
    next.playerSetupPending = 1U;
    next.tileEnterPending = 1U;
    next.active = 1U;
    next.targetMapId = transition->targetMapId;
    next.gameplayLoadMapId = transition->targetGameplayLoadMapId;

    *outState = next;
    return ESP_PLAYER_SPAWN_OK;
}
