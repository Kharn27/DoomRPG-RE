#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_map_committed_transition.h"
#include "esp_map_runtime.h"

static void clearSnapshot(EspMapResidentSnapshot* snapshot) {
    if (snapshot != NULL) memset(snapshot, 0, sizeof(*snapshot));
}

static int inventoryMatchesRuntime(const EspBspInventory* inventory) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();

    return inventory != NULL && runtime != NULL &&
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

static int inventoryIsComplete(const EspBspInventory* inventory) {
    return inventory != NULL && inventory->sourceBytes != 0U &&
           inventory->consumedBytes == inventory->sourceBytes &&
           inventory->structuralEndOffset == inventory->sourceBytes &&
           inventory->sections.endOffset == inventory->sourceBytes &&
           inventory->trailingBytes == 0U && inventory->crc32 != 0U &&
           inventory->expectedCrc32 == inventory->crc32 &&
           inventory->fnv1a32 != 0U && inventory->plan.persistentBytes != 0U;
}

static int targetInventoryMatchesState(
    const EspMapCommittedTransitionState* state,
    const EspBspInventory* inventory) {
    return state != NULL && inventoryIsComplete(inventory) &&
           inventory->sourceBytes == state->targetSourceBytes &&
           inventory->crc32 == state->targetSourceCrc32 &&
           inventory->fnv1a32 == state->targetSourceFNV1a &&
           inventory->loadMapId == state->targetGameplayLoadMapId;
}

static int sourceInventoryMatchesState(
    const EspMapCommittedTransitionState* state,
    const EspBspInventory* inventory) {
    return state != NULL && inventoryIsComplete(inventory) &&
           inventoryMatchesRuntime(inventory);
}

void EspMapCommittedTransition_reset(EspMapCommittedTransitionState* state) {
    if (state != NULL) memset(state, 0, sizeof(*state));
}

int EspMapCommittedTransition_isCommitted(
    const EspMapCommittedTransitionState* state) {
    return state != NULL &&
           state->phase == ESP_MAP_COMMITTED_TRANSITION_PHASE_COMMITTED &&
           state->committed != 0U;
}

EspMapCommittedTransitionStatus EspMapCommittedTransition_begin(
    EspMapCommittedTransitionState* state,
    uint8_t sourceMapId,
    EspMapChangeMapState* pendingChange,
    const EspMapChangeMapResult* changeResult,
    const EspStatsMenuIntent* statsIntent,
    const EspMapTransitionPreflightResult* targetPreflight) {
    EspMapCommittedTransitionState next;
    uint8_t showStats;

    if (state == NULL || pendingChange == NULL || changeResult == NULL ||
        targetPreflight == NULL || !EspMapCatalog_isValidId(sourceMapId) ||
        state->phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_EMPTY ||
        state->pendingConsumed != 0U || state->committed != 0U ||
        !EspMapChangeMap_isActive(pendingChange) || pendingChange->active != 1U ||
        changeResult->pending != 1U || changeResult->legacyReturnValue != 1U ||
        pendingChange->rawParam != changeResult->rawParam ||
        pendingChange->sourceEventIndex != changeResult->sourceEventIndex ||
        pendingChange->globalCommandIndex != changeResult->globalCommandIndex ||
        pendingChange->sourceCommandOffset != changeResult->sourceCommandOffset ||
        pendingChange->mapName.index != changeResult->mapStringIndex ||
        targetPreflight->ready != 1U ||
        !EspMapCatalog_isValidId(targetPreflight->targetMapId) ||
        targetPreflight->targetMapId == sourceMapId ||
        targetPreflight->gameplayLoadMapId <
            ESP_MAP_TRANSITION_GAMEPLAY_LOAD_ID_MIN ||
        targetPreflight->gameplayLoadMapId >
            ESP_MAP_TRANSITION_GAMEPLAY_LOAD_ID_MAX ||
        targetPreflight->sourceBytes == 0U ||
        targetPreflight->sourceCrc32 == 0U ||
        targetPreflight->sourceFNV1a == 0U) {
        return ESP_MAP_COMMITTED_TRANSITION_INVALID;
    }

    showStats = changeResult->showStats;
    if (showStats > 1U) return ESP_MAP_COMMITTED_TRANSITION_INVALID;
    if (showStats != 0U) {
        if (statsIntent == NULL || statsIntent->active != 1U ||
            statsIntent->consumePending != 1U ||
            statsIntent->targetMapId != targetPreflight->targetMapId ||
            (statsIntent->menuKind != ESP_STATS_MENU_KIND_LEVEL &&
             statsIntent->menuKind != ESP_STATS_MENU_KIND_OVERALL)) {
            return ESP_MAP_COMMITTED_TRANSITION_INVALID;
        }
    }
    else if (statsIntent != NULL &&
             (statsIntent->active != 0U || statsIntent->consumePending != 0U ||
              statsIntent->menuKind != ESP_STATS_MENU_KIND_NONE)) {
        return ESP_MAP_COMMITTED_TRANSITION_INVALID;
    }

    memset(&next, 0, sizeof(next));
    next.targetSourceBytes = targetPreflight->sourceBytes;
    next.targetSourceCrc32 = targetPreflight->sourceCrc32;
    next.targetSourceFNV1a = targetPreflight->sourceFNV1a;
    next.spawnParam = changeResult->spawnParam;
    next.sourceMapId = sourceMapId;
    next.targetMapId = targetPreflight->targetMapId;
    next.targetGameplayLoadMapId = targetPreflight->gameplayLoadMapId;
    next.menuKind = showStats != 0U ? statsIntent->menuKind
                                    : ESP_STATS_MENU_KIND_NONE;
    next.phase = showStats != 0U
                     ? ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS
                     : ESP_MAP_COMMITTED_TRANSITION_PHASE_READY;
    next.pendingConsumed = 1U;

    *state = next;
    EspMapChangeMap_reset(pendingChange);
    return showStats != 0U ? ESP_MAP_COMMITTED_TRANSITION_WAITING_STATS
                           : ESP_MAP_COMMITTED_TRANSITION_READY;
}

EspMapCommittedTransitionStatus EspMapCommittedTransition_ackStats(
    EspMapCommittedTransitionState* state) {
    if (state == NULL || state->pendingConsumed != 1U ||
        state->committed != 0U) {
        return ESP_MAP_COMMITTED_TRANSITION_INVALID;
    }
    if (state->phase == ESP_MAP_COMMITTED_TRANSITION_PHASE_READY &&
        state->statsAcknowledged == 1U) {
        return ESP_MAP_COMMITTED_TRANSITION_READY;
    }
    if (state->phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_WAIT_STATS ||
        state->menuKind == ESP_STATS_MENU_KIND_NONE ||
        state->statsAcknowledged != 0U) {
        return ESP_MAP_COMMITTED_TRANSITION_INVALID;
    }

    state->statsAcknowledged = 1U;
    state->phase = ESP_MAP_COMMITTED_TRANSITION_PHASE_READY;
    return ESP_MAP_COMMITTED_TRANSITION_READY;
}

EspMapCommittedTransitionStatus EspMapCommittedTransition_commit(
    EspMapCommittedTransitionState* state,
    const EspBspInventory* sourceInventory,
    const EspBspInventory* targetInventory,
    EspMapResidentSnapshot* outTargetSnapshot) {
    EspMapCommittedTransitionState next;
    EspMapResidentSnapshot sourceSnapshot;
    EspMapResidentSnapshot targetSnapshot;
    EspMapResidentSnapshot recoveredSnapshot;
    EspMapResidentLifecycleStatus residentStatus;
    const EspMapRuntimeView* targetRuntime;
    const char* sourceName;
    const char* targetName;

    clearSnapshot(outTargetSnapshot);
    if (state == NULL || sourceInventory == NULL || targetInventory == NULL ||
        outTargetSnapshot == NULL ||
        state->phase != ESP_MAP_COMMITTED_TRANSITION_PHASE_READY ||
        state->pendingConsumed != 1U || state->committed != 0U ||
        !EspMapCatalog_isValidId(state->sourceMapId) ||
        !EspMapCatalog_isValidId(state->targetMapId) ||
        state->sourceMapId == state->targetMapId ||
        EspAssetPack_isOpen()) {
        return ESP_MAP_COMMITTED_TRANSITION_INVALID;
    }
    if (state->menuKind != ESP_STATS_MENU_KIND_NONE &&
        state->statsAcknowledged != 1U) {
        return ESP_MAP_COMMITTED_TRANSITION_WAITING_STATS;
    }

    sourceName = EspMapCatalog_nameForId(state->sourceMapId);
    targetName = EspMapCatalog_nameForId(state->targetMapId);
    if (sourceName == NULL || targetName == NULL ||
        !sourceInventoryMatchesState(state, sourceInventory) ||
        !EspMapResidentLifecycle_capture(&sourceSnapshot)) {
        return ESP_MAP_COMMITTED_TRANSITION_SOURCE_MISMATCH;
    }
    if (!targetInventoryMatchesState(state, targetInventory)) {
        return ESP_MAP_COMMITTED_TRANSITION_INVENTORY_MISMATCH;
    }

    next = *state;
    EspMapResidentLifecycle_resetAll();
    residentStatus = EspMapResidentLifecycle_loadFromEmpty(
        targetName, targetInventory, &targetSnapshot);
    targetRuntime = EspMapRuntime_view();
    if (residentStatus == ESP_MAP_RESIDENT_OK && targetRuntime != NULL &&
        targetRuntime->sourceBytes == state->targetSourceBytes &&
        targetRuntime->sourceCrc32 == state->targetSourceCrc32 &&
        EspMapResidentLifecycle_isReady()) {
        next.phase = ESP_MAP_COMMITTED_TRANSITION_PHASE_COMMITTED;
        next.committed = 1U;
        *state = next;
        *outTargetSnapshot = targetSnapshot;
        return ESP_MAP_COMMITTED_TRANSITION_OK;
    }

    EspMapResidentLifecycle_resetAll();
    residentStatus = EspMapResidentLifecycle_loadFromEmpty(
        sourceName, sourceInventory, &recoveredSnapshot);
    if (residentStatus == ESP_MAP_RESIDENT_OK &&
        inventoryMatchesRuntime(sourceInventory) &&
        EspMapResidentLifecycle_isReady()) {
        next.phase = ESP_MAP_COMMITTED_TRANSITION_PHASE_ROLLED_BACK;
        next.committed = 0U;
        *state = next;
        return ESP_MAP_COMMITTED_TRANSITION_ROLLED_BACK;
    }

    EspMapResidentLifecycle_resetAll();
    next.phase = ESP_MAP_COMMITTED_TRANSITION_PHASE_FAILED;
    next.committed = 0U;
    *state = next;
    return ESP_MAP_COMMITTED_TRANSITION_RECOVERY_FAILED;
}
