#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_catalog.h"
#include "esp_player_spawn_state.h"
#include "esp_player_view_state.h"

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
