#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_catalog.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_player_fresh_map_state.h"

static EspPlayerFreshMapState freshMapState;

_Static_assert(sizeof(EspPlayerLevelProgress) == 16U,
               "level progress checkpoint suffix must remain 16 bytes");

static uint32_t addSaturated(uint32_t a, uint32_t b) {
    return b > UINT32_MAX - a ? UINT32_MAX : a + b;
}

void EspPlayerFreshMap_beginTimer(uint32_t nowMs) {
    if (!EspPlayerFreshMap_isReady() || freshMapState.timerRunning) return;
    freshMapState.levelStartTimeMs = nowMs;
    freshMapState.timerRunning = 1U;
}

void EspPlayerFreshMap_recordMove(void) {
    if (EspPlayerFreshMap_isReady() && freshMapState.moves != UINT32_MAX) {
        ++freshMapState.moves;
    }
}

int EspPlayerFreshMap_snapshotProgress(uint32_t nowMs,
                                      EspPlayerLevelProgress* outProgress) {
    if (outProgress == NULL) return 0;
    memset(outProgress, 0, sizeof(*outProgress));
    if (!EspPlayerFreshMap_isReady()) return 0;
    outProgress->moves = freshMapState.moves;
    outProgress->elapsedMs = freshMapState.elapsedBeforeMs;
    if (freshMapState.timerRunning) {
        outProgress->elapsedMs = addSaturated(outProgress->elapsedMs,
            (uint32_t)(nowMs - freshMapState.levelStartTimeMs));
    }
    outProgress->xpBaseline = freshMapState.xpBaseline;
    outProgress->targetMapId = freshMapState.targetMapId;
    outProgress->complete = freshMapState.statsComplete;
    return 1;
}

int EspPlayerFreshMap_restoreProgress(const EspPlayerLevelProgress* progress) {
    if (progress == NULL || !EspMapCatalog_isValidId(progress->targetMapId) ||
        progress->complete > 1U || progress->reserved[0] != 0U ||
        progress->reserved[1] != 0U) return 0;
    memset(&freshMapState, 0, sizeof(freshMapState));
    freshMapState.moves = progress->moves;
    freshMapState.elapsedBeforeMs = progress->elapsedMs;
    freshMapState.xpBaseline = progress->xpBaseline;
    freshMapState.targetMapId = progress->targetMapId;
    {
        const EspPlayerViewState* view = EspPlayerView_view();
        if (view != NULL && view->targetMapId == progress->targetMapId) {
            freshMapState.gameplayLoadMapId = view->gameplayLoadMapId;
            freshMapState.loadType = view->loadType;
        }
    }
    freshMapState.statsComplete = progress->complete;
    freshMapState.setupApplied = 1U;
    freshMapState.active = 1U;
    return 1;
}

void EspPlayerFreshMap_resumeLegacy(uint8_t mapId, uint32_t xpGained) {
    EspPlayerLevelProgress progress;
    memset(&progress, 0, sizeof(progress));
    progress.targetMapId = mapId;
    progress.xpBaseline = xpGained;
    (void)EspPlayerFreshMap_restoreProgress(&progress);
}

static int hudIsCanonical(const EspHudRefreshState* hud,
                          const EspPlayerViewState* view) {
    return hud != NULL && view != NULL &&
           hud->reason == ESP_HUD_REFRESH_REASON_POST_SPAWN &&
           hud->refreshPending == 1U && hud->routed == 1U &&
           hud->active == 1U && hud->targetMapId == view->targetMapId &&
           hud->gameplayLoadMapId == view->gameplayLoadMapId &&
           hud->loadType == view->loadType && hud->reserved == 0U;
}

void EspPlayerFreshMap_reset(void) {
    memset(&freshMapState, 0, sizeof(freshMapState));
}

int EspPlayerFreshMap_isReady(void) {
    return freshMapState.active == 1U && freshMapState.setupApplied == 1U;
}

const EspPlayerFreshMapState* EspPlayerFreshMap_view(void) {
    return EspPlayerFreshMap_isReady() ? &freshMapState : NULL;
}

EspPlayerFreshMapStatus EspPlayerFreshMap_prepare(
    const EspPlayerViewState* playerView,
    const EspHudRefreshState* hudRefresh,
    uint32_t nowMs,
    uint32_t disabledWeapons,
    EspPlayerFreshMapState* outState) {
    EspPlayerFreshMapState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (playerView == NULL || hudRefresh == NULL || outState == NULL) {
        return ESP_PLAYER_FRESH_MAP_INVALID;
    }
    if (playerView->loadType != 0U) {
        return ESP_PLAYER_FRESH_MAP_UNSUPPORTED_CONTEXT;
    }
    if (playerView->active != 1U || playerView->spawnApplied != 1U ||
        !EspMapCatalog_isValidId(playerView->targetMapId) ||
        playerView->gameplayLoadMapId == 0U ||
        playerView->gameplayLoadMapId > 32U ||
        playerView->viewX != playerView->destX ||
        playerView->viewY != playerView->destY ||
        playerView->viewAngle != playerView->destAngle ||
        playerView->viewZ != 36 || playerView->viewZOld != 4) {
        return ESP_PLAYER_FRESH_MAP_VIEW_INVALID;
    }
    if (playerView->hudRefreshPending != 0U ||
        playerView->facingRefreshPending != 1U ||
        playerView->playerSetupPending != 1U ||
        playerView->tileEnterPending != 1U) {
        return ESP_PLAYER_FRESH_MAP_UNSUPPORTED_ORDER;
    }
    if (!hudIsCanonical(hudRefresh, playerView)) {
        return ESP_PLAYER_FRESH_MAP_HUD_INVALID;
    }
    if (disabledWeapons != 0U) {
        return ESP_PLAYER_FRESH_MAP_WEAPON_RESTORE_DEFERRED;
    }

    memset(&next, 0, sizeof(next));
    next.levelStartTimeMs = nowMs;
    next.moves = 0U;
    next.xpGained = 0U;
    next.berserkerTics = 0U;
    next.familiarActive = 0U;
    next.notebookEmpty = 1U;
    next.weaponRestorePerformed = 0U;
    next.targetMapId = playerView->targetMapId;
    next.gameplayLoadMapId = playerView->gameplayLoadMapId;
    next.loadType = playerView->loadType;
    next.setupApplied = 1U;
    next.active = 1U;

    *outState = next;
    return ESP_PLAYER_FRESH_MAP_OK;
}

EspPlayerFreshMapStatus EspPlayerFreshMap_route(
    uint32_t nowMs,
    uint32_t disabledWeapons) {
    const EspPlayerViewState* playerView;
    const EspHudRefreshState* hudRefresh;
    EspPlayerFreshMapState next;
    EspPlayerFreshMapStatus status;

    if (EspPlayerFreshMap_isReady()) {
        return ESP_PLAYER_FRESH_MAP_ALREADY_ACTIVE;
    }

    playerView = EspPlayerView_view();
    hudRefresh = EspHudRefresh_view();
    if (playerView == NULL) return ESP_PLAYER_FRESH_MAP_VIEW_INVALID;
    if (hudRefresh == NULL) return ESP_PLAYER_FRESH_MAP_HUD_INVALID;

    status = EspPlayerFreshMap_prepare(playerView, hudRefresh, nowMs,
                                       disabledWeapons, &next);
    if (status != ESP_PLAYER_FRESH_MAP_OK) return status;

    if (!EspPlayerView_consumePlayerSetup(next.targetMapId,
                                          next.gameplayLoadMapId,
                                          next.loadType)) {
        return ESP_PLAYER_FRESH_MAP_VIEW_CONSUME_FAILED;
    }

    freshMapState = next;
    freshMapState.xpBaseline = EspNativeGameplayPlayerState_view()->xpGained;
    freshMapState.statsComplete = 1U;
    return ESP_PLAYER_FRESH_MAP_OK;
}
