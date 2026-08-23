#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_catalog.h"
#include "esp_player_fresh_map_state.h"

static EspPlayerFreshMapState freshMapState;

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
    return ESP_PLAYER_FRESH_MAP_OK;
}
