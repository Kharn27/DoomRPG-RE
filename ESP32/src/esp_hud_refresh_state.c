#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_catalog.h"
#include "esp_hud_refresh_state.h"

static EspHudRefreshState hudRefreshState;

void EspHudRefresh_reset(void) {
    memset(&hudRefreshState, 0, sizeof(hudRefreshState));
}

int EspHudRefresh_isReady(void) {
    return hudRefreshState.active == 1U && hudRefreshState.routed == 1U &&
           hudRefreshState.refreshPending == 1U;
}

const EspHudRefreshState* EspHudRefresh_view(void) {
    return EspHudRefresh_isReady() ? &hudRefreshState : NULL;
}

EspHudRefreshStatus EspHudRefresh_preparePostSpawn(
    const EspPlayerViewState* playerView,
    EspHudRefreshState* outState) {
    EspHudRefreshState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (playerView == NULL || outState == NULL) return ESP_HUD_REFRESH_INVALID;

    if (playerView->active != 1U || playerView->spawnApplied != 1U ||
        !EspMapCatalog_isValidId(playerView->targetMapId) ||
        playerView->gameplayLoadMapId == 0U ||
        playerView->gameplayLoadMapId > 32U) {
        return ESP_HUD_REFRESH_VIEW_INVALID;
    }
    if (playerView->loadType != ESP_PLAYER_SPAWN_LOAD_FRESH_MAP ||
        playerView->hudRefreshPending != 1U ||
        playerView->facingRefreshPending != 1U ||
        playerView->playerSetupPending != 1U ||
        playerView->tileEnterPending != 1U) {
        return ESP_HUD_REFRESH_UNSUPPORTED_CONTEXT;
    }

    memset(&next, 0, sizeof(next));
    next.reason = ESP_HUD_REFRESH_REASON_POST_SPAWN;
    next.refreshPending = 1U;
    next.routed = 1U;
    next.active = 1U;
    next.targetMapId = playerView->targetMapId;
    next.gameplayLoadMapId = playerView->gameplayLoadMapId;
    next.loadType = playerView->loadType;
    *outState = next;
    return ESP_HUD_REFRESH_OK;
}

EspHudRefreshStatus EspHudRefresh_routePostSpawn(void) {
    const EspPlayerViewState* playerView;
    EspHudRefreshState next;
    EspHudRefreshStatus status;

    if (EspHudRefresh_isReady()) return ESP_HUD_REFRESH_ALREADY_ACTIVE;
    playerView = EspPlayerView_view();
    if (playerView == NULL) return ESP_HUD_REFRESH_VIEW_INVALID;

    status = EspHudRefresh_preparePostSpawn(playerView, &next);
    if (status != ESP_HUD_REFRESH_OK) return status;
    if (!EspPlayerView_consumeHudRefresh(next.targetMapId,
                                         next.gameplayLoadMapId,
                                         next.loadType)) {
        return ESP_HUD_REFRESH_VIEW_CONSUME_FAILED;
    }

    hudRefreshState = next;
    return ESP_HUD_REFRESH_OK;
}
