#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_catalog.h"
#include "esp_player_spawn_state.h"
#include "esp_player_view_state.h"

#define RUNTIME_STEP_SIZE 64

static EspPlayerViewState playerViewState;

static int spawnIsConsistent(const EspPlayerSpawnState* spawn) {
    uint32_t expectedTileIndex;
    uint32_t expectedWorldX;
    uint32_t expectedWorldY;
    uint32_t expectedOverrideX;
    uint32_t expectedOverrideY;
    uint32_t expectedOverrideAngle;

    if (spawn == NULL || spawn->active != 1U ||
        spawn->loadType != ESP_PLAYER_SPAWN_LOAD_FRESH_MAP ||
        !EspMapCatalog_isValidId(spawn->targetMapId) ||
        spawn->gameplayLoadMapId == 0U || spawn->gameplayLoadMapId > 32U ||
        spawn->tileX >= ESP_PLAYER_SPAWN_MAP_WIDTH ||
        spawn->tileY >= ESP_PLAYER_SPAWN_MAP_WIDTH ||
        spawn->tileIndex >= ESP_PLAYER_SPAWN_TILE_COUNT ||
        spawn->viewZ != ESP_PLAYER_SPAWN_VIEW_Z ||
        spawn->viewZOld != ESP_PLAYER_SPAWN_VIEW_Z_OLD ||
        spawn->facingRefreshPending != 1U ||
        spawn->playerSetupPending != 1U ||
        spawn->tileEnterPending != 1U) {
        return 0;
    }

    expectedTileIndex =
        (uint32_t)spawn->tileY * ESP_PLAYER_SPAWN_MAP_WIDTH + spawn->tileX;
    expectedWorldX =
        (uint32_t)spawn->tileX * ESP_PLAYER_SPAWN_TILE_SIZE +
        ESP_PLAYER_SPAWN_TILE_CENTER;
    expectedWorldY =
        (uint32_t)spawn->tileY * ESP_PLAYER_SPAWN_TILE_SIZE +
        ESP_PLAYER_SPAWN_TILE_CENTER;

    if (spawn->tileIndex != expectedTileIndex ||
        spawn->worldX != expectedWorldX || spawn->worldY != expectedWorldY) {
        return 0;
    }

    if (spawn->spawnSource == ESP_PLAYER_SPAWN_SOURCE_HEADER) {
        return spawn->sourceSpawnParam == 0U && spawn->overrideUsed == 0U;
    }
    if (spawn->spawnSource == ESP_PLAYER_SPAWN_SOURCE_OVERRIDE) {
        if (spawn->sourceSpawnParam == 0U || spawn->overrideUsed != 1U) return 0;
        expectedOverrideX = spawn->sourceSpawnParam & 31U;
        expectedOverrideY = (spawn->sourceSpawnParam >> 5U) & 31U;
        expectedOverrideAngle = (spawn->sourceSpawnParam >> 10U) & 255U;
        return spawn->tileX == expectedOverrideX &&
               spawn->tileY == expectedOverrideY &&
               spawn->angle == expectedOverrideAngle;
    }
    return 0;
}

static int viewSettledForRuntimeAction(const EspPlayerViewState* view) {
    return view != NULL && view->active == 1U && view->spawnApplied == 1U &&
           view->hudRefreshPending == 0U &&
           view->facingRefreshPending == 0U &&
           view->playerSetupPending == 0U &&
           view->tileEnterPending == 0U &&
           view->viewX == view->destX && view->viewY == view->destY &&
           view->viewAngle == view->destAngle &&
           view->viewAngle >= 0 && view->viewAngle <= 255 &&
           (view->viewAngle & 63) == 0;
}

static int sameExceptRuntimeAngles(const EspPlayerViewState* a,
                                   const EspPlayerViewState* b) {
    EspPlayerViewState left;
    EspPlayerViewState right;
    if (a == NULL || b == NULL) return 0;
    left = *a;
    right = *b;
    left.viewAngle = 0;
    left.destAngle = 0;
    right.viewAngle = 0;
    right.destAngle = 0;
    return memcmp(&left, &right, sizeof(left)) == 0;
}

static int sameExceptRuntimePosition(const EspPlayerViewState* a,
                                     const EspPlayerViewState* b) {
    EspPlayerViewState left;
    EspPlayerViewState right;
    if (a == NULL || b == NULL) return 0;
    left = *a;
    right = *b;
    left.viewX = 0;
    left.viewY = 0;
    left.destX = 0;
    left.destY = 0;
    right.viewX = 0;
    right.viewY = 0;
    right.destX = 0;
    right.destY = 0;
    return memcmp(&left, &right, sizeof(left)) == 0;
}

static int oneQuarterApart(int32_t before, int32_t after) {
    const int32_t diff = (after - before) & 255;
    return diff == 64 || diff == 192;
}

static int cardinalStep(int32_t deltaX, int32_t deltaY) {
    return (deltaX == RUNTIME_STEP_SIZE && deltaY == 0) ||
           (deltaX == -RUNTIME_STEP_SIZE && deltaY == 0) ||
           (deltaX == 0 && deltaY == RUNTIME_STEP_SIZE) ||
           (deltaX == 0 && deltaY == -RUNTIME_STEP_SIZE);
}

void EspPlayerView_reset(void) {
    memset(&playerViewState, 0, sizeof(playerViewState));
}

int EspPlayerView_isReady(void) {
    return playerViewState.active == 1U && playerViewState.spawnApplied == 1U;
}

const EspPlayerViewState* EspPlayerView_view(void) {
    return EspPlayerView_isReady() ? &playerViewState : NULL;
}

EspPlayerViewApplyStatus EspPlayerView_applySpawn(
    const EspPlayerSpawnState* spawn) {
    EspPlayerViewState next;

    if (spawn == NULL) return ESP_PLAYER_VIEW_APPLY_INVALID;
    if (EspPlayerView_isReady()) return ESP_PLAYER_VIEW_APPLY_ALREADY_ACTIVE;
    if (!spawnIsConsistent(spawn)) return ESP_PLAYER_VIEW_APPLY_SPAWN_INVALID;

    memset(&next, 0, sizeof(next));
    next.viewX = (int32_t)spawn->worldX;
    next.viewY = (int32_t)spawn->worldY;
    next.viewZ = (int32_t)spawn->viewZ;
    next.viewAngle = (int32_t)spawn->angle;
    next.destX = (int32_t)spawn->worldX;
    next.destY = (int32_t)spawn->worldY;
    next.destAngle = (int32_t)spawn->angle;
    next.viewZOld = (int32_t)spawn->viewZOld;
    next.targetMapId = spawn->targetMapId;
    next.gameplayLoadMapId = spawn->gameplayLoadMapId;
    next.loadType = spawn->loadType;
    next.spawnApplied = 1U;
    next.hudRefreshPending = 1U;
    next.facingRefreshPending = spawn->facingRefreshPending;
    next.playerSetupPending = spawn->playerSetupPending;
    next.tileEnterPending = spawn->tileEnterPending;
    next.active = 1U;

    playerViewState = next;
    return ESP_PLAYER_VIEW_APPLY_OK;
}

int EspPlayerView_consumeHudRefresh(uint8_t targetMapId,
                                    uint8_t gameplayLoadMapId,
                                    uint8_t loadType) {
    EspPlayerViewState next;

    if (!EspPlayerView_isReady() || playerViewState.hudRefreshPending != 1U ||
        playerViewState.facingRefreshPending != 1U ||
        playerViewState.playerSetupPending != 1U ||
        playerViewState.tileEnterPending != 1U ||
        targetMapId != playerViewState.targetMapId ||
        gameplayLoadMapId != playerViewState.gameplayLoadMapId ||
        loadType != playerViewState.loadType) return 0;

    next = playerViewState;
    next.hudRefreshPending = 0U;
    playerViewState = next;
    return 1;
}

int EspPlayerView_consumePlayerSetup(uint8_t targetMapId,
                                     uint8_t gameplayLoadMapId,
                                     uint8_t loadType) {
    EspPlayerViewState next;

    if (!EspPlayerView_isReady() || playerViewState.hudRefreshPending != 0U ||
        playerViewState.facingRefreshPending != 1U ||
        playerViewState.playerSetupPending != 1U ||
        playerViewState.tileEnterPending != 1U ||
        targetMapId != playerViewState.targetMapId ||
        gameplayLoadMapId != playerViewState.gameplayLoadMapId ||
        loadType != playerViewState.loadType) return 0;

    next = playerViewState;
    next.playerSetupPending = 0U;
    playerViewState = next;
    return 1;
}

int EspPlayerView_consumeTileEnter(uint8_t targetMapId,
                                   uint8_t gameplayLoadMapId,
                                   uint8_t loadType) {
    EspPlayerViewState next;

    if (!EspPlayerView_isReady() || playerViewState.hudRefreshPending != 0U ||
        playerViewState.facingRefreshPending != 1U ||
        playerViewState.playerSetupPending != 0U ||
        playerViewState.tileEnterPending != 1U ||
        targetMapId != playerViewState.targetMapId ||
        gameplayLoadMapId != playerViewState.gameplayLoadMapId ||
        loadType != playerViewState.loadType) return 0;

    next = playerViewState;
    next.tileEnterPending = 0U;
    playerViewState = next;
    return 1;
}

int EspPlayerView_consumeFacing(uint8_t targetMapId,
                                uint8_t gameplayLoadMapId,
                                uint8_t loadType) {
    EspPlayerViewState next;

    if (!EspPlayerView_isReady() || playerViewState.hudRefreshPending != 0U ||
        playerViewState.facingRefreshPending != 1U ||
        playerViewState.playerSetupPending != 0U ||
        playerViewState.tileEnterPending != 0U ||
        targetMapId != playerViewState.targetMapId ||
        gameplayLoadMapId != playerViewState.gameplayLoadMapId ||
        loadType != playerViewState.loadType) return 0;

    next = playerViewState;
    next.facingRefreshPending = 0U;
    playerViewState = next;
    return 1;
}

EspPlayerViewTurnStatus EspPlayerView_prepareQuarterTurn(
    int32_t angleDelta,
    EspPlayerViewState* outBefore,
    EspPlayerViewState* outAfter) {
    EspPlayerViewState next;

    if (outBefore != NULL) memset(outBefore, 0, sizeof(*outBefore));
    if (outAfter != NULL) memset(outAfter, 0, sizeof(*outAfter));
    if (outBefore == NULL || outAfter == NULL) return ESP_PLAYER_VIEW_TURN_INVALID;
    if (!EspPlayerView_isReady()) return ESP_PLAYER_VIEW_TURN_NOT_READY;
    if (!viewSettledForRuntimeAction(&playerViewState)) {
        return ESP_PLAYER_VIEW_TURN_UNSETTLED;
    }
    if (angleDelta != 64 && angleDelta != -64) {
        return ESP_PLAYER_VIEW_TURN_UNSUPPORTED;
    }

    *outBefore = playerViewState;
    next = playerViewState;
    next.viewAngle = (playerViewState.viewAngle + angleDelta) & 255;
    next.destAngle = next.viewAngle;
    *outAfter = next;
    return ESP_PLAYER_VIEW_TURN_OK;
}

EspPlayerViewTurnStatus EspPlayerView_commitPreparedTurn(
    const EspPlayerViewState* expectedBefore,
    const EspPlayerViewState* preparedAfter) {
    if (expectedBefore == NULL || preparedAfter == NULL) {
        return ESP_PLAYER_VIEW_TURN_INVALID;
    }
    if (!EspPlayerView_isReady()) return ESP_PLAYER_VIEW_TURN_NOT_READY;
    if (memcmp(&playerViewState, expectedBefore, sizeof(playerViewState)) != 0) {
        return ESP_PLAYER_VIEW_TURN_STALE;
    }
    if (!viewSettledForRuntimeAction(expectedBefore) ||
        !viewSettledForRuntimeAction(preparedAfter)) {
        return ESP_PLAYER_VIEW_TURN_UNSETTLED;
    }
    if (!sameExceptRuntimeAngles(expectedBefore, preparedAfter) ||
        !oneQuarterApart(expectedBefore->viewAngle, preparedAfter->viewAngle)) {
        return ESP_PLAYER_VIEW_TURN_UNSUPPORTED;
    }

    playerViewState = *preparedAfter;
    return ESP_PLAYER_VIEW_TURN_OK;
}

EspPlayerViewMoveStatus EspPlayerView_prepareCardinalMove(
    int32_t deltaX,
    int32_t deltaY,
    EspPlayerViewState* outBefore,
    EspPlayerViewState* outAfter) {
    EspPlayerViewState next;

    if (outBefore != NULL) memset(outBefore, 0, sizeof(*outBefore));
    if (outAfter != NULL) memset(outAfter, 0, sizeof(*outAfter));
    if (outBefore == NULL || outAfter == NULL) return ESP_PLAYER_VIEW_MOVE_INVALID;
    if (!EspPlayerView_isReady()) return ESP_PLAYER_VIEW_MOVE_NOT_READY;
    if (!viewSettledForRuntimeAction(&playerViewState)) {
        return ESP_PLAYER_VIEW_MOVE_UNSETTLED;
    }
    if (!cardinalStep(deltaX, deltaY)) {
        return ESP_PLAYER_VIEW_MOVE_UNSUPPORTED;
    }

    *outBefore = playerViewState;
    next = playerViewState;
    next.viewX += deltaX;
    next.viewY += deltaY;
    next.destX = next.viewX;
    next.destY = next.viewY;
    *outAfter = next;
    return ESP_PLAYER_VIEW_MOVE_OK;
}

EspPlayerViewMoveStatus EspPlayerView_commitPreparedMove(
    const EspPlayerViewState* expectedBefore,
    const EspPlayerViewState* preparedAfter) {
    int32_t deltaX;
    int32_t deltaY;

    if (expectedBefore == NULL || preparedAfter == NULL) {
        return ESP_PLAYER_VIEW_MOVE_INVALID;
    }
    if (!EspPlayerView_isReady()) return ESP_PLAYER_VIEW_MOVE_NOT_READY;
    if (memcmp(&playerViewState, expectedBefore, sizeof(playerViewState)) != 0) {
        return ESP_PLAYER_VIEW_MOVE_STALE;
    }
    if (!viewSettledForRuntimeAction(expectedBefore) ||
        !viewSettledForRuntimeAction(preparedAfter)) {
        return ESP_PLAYER_VIEW_MOVE_UNSETTLED;
    }
    deltaX = preparedAfter->viewX - expectedBefore->viewX;
    deltaY = preparedAfter->viewY - expectedBefore->viewY;
    if (!sameExceptRuntimePosition(expectedBefore, preparedAfter) ||
        !cardinalStep(deltaX, deltaY) ||
        preparedAfter->destX != preparedAfter->viewX ||
        preparedAfter->destY != preparedAfter->viewY) {
        return ESP_PLAYER_VIEW_MOVE_UNSUPPORTED;
    }

    playerViewState = *preparedAfter;
    return ESP_PLAYER_VIEW_MOVE_OK;
}
