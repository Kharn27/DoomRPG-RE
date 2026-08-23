#include <stddef.h>
#include <string.h>

#include "esp_hud_post_load_clear_state.h"

static EspHudPostLoadClearState hudPostLoadClearState;

static int identityMatches(const EspPlayerViewState* playerView,
                           const EspPlayerFacingState* facing) {
    return playerView != NULL && facing != NULL &&
           playerView->targetMapId == facing->targetMapId &&
           playerView->gameplayLoadMapId == facing->gameplayLoadMapId &&
           playerView->loadType == facing->loadType;
}

void EspHudPostLoadClear_reset(void) {
    memset(&hudPostLoadClearState, 0, sizeof(hudPostLoadClearState));
}

int EspHudPostLoadClear_isReady(void) {
    return hudPostLoadClearState.active == 1U &&
           hudPostLoadClearState.cleared == 1U;
}

const EspHudPostLoadClearState* EspHudPostLoadClear_view(void) {
    return EspHudPostLoadClear_isReady() ? &hudPostLoadClearState : NULL;
}

EspHudPostLoadClearStatus EspHudPostLoadClear_prepare(
    const EspPlayerViewState* playerView,
    const EspPlayerFacingState* facing,
    EspHudPostLoadClearState* outState) {
    EspHudPostLoadClearState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (playerView == NULL || facing == NULL || outState == NULL) {
        return ESP_HUD_POST_LOAD_CLEAR_INVALID;
    }
    if (playerView->active != 1U || playerView->spawnApplied != 1U) {
        return ESP_HUD_POST_LOAD_CLEAR_VIEW_INVALID;
    }
    if (facing->active != 1U ||
        facing->kind > ESP_PLAYER_FACING_KIND_WALL ||
        !identityMatches(playerView, facing)) {
        return ESP_HUD_POST_LOAD_CLEAR_FACING_INVALID;
    }

    /* Only the hardware-proven fresh-map branch is enabled. Saved-world load
     * ordering stays fail-closed until it receives its own milestone. */
    if (playerView->loadType != 0U) {
        return ESP_HUD_POST_LOAD_CLEAR_UNSUPPORTED_CONTEXT;
    }
    if (playerView->hudRefreshPending != 0U ||
        playerView->facingRefreshPending != 0U ||
        playerView->playerSetupPending != 0U ||
        playerView->tileEnterPending != 0U) {
        return ESP_HUD_POST_LOAD_CLEAR_UNSUPPORTED_ORDER;
    }

    memset(&next, 0, sizeof(next));
    next.targetMapId = playerView->targetMapId;
    next.gameplayLoadMapId = playerView->gameplayLoadMapId;
    next.loadType = playerView->loadType;
    next.messageCount = 0U;
    next.statBarMessagePresent = 0U;
    next.logMessageLength = 0U;
    next.cleared = 1U;
    next.active = 1U;
    *outState = next;
    return ESP_HUD_POST_LOAD_CLEAR_OK;
}

EspHudPostLoadClearStatus EspHudPostLoadClear_route(void) {
    EspHudPostLoadClearState next;
    EspHudPostLoadClearStatus status;

    if (EspHudPostLoadClear_isReady()) {
        return ESP_HUD_POST_LOAD_CLEAR_ALREADY_ACTIVE;
    }

    status = EspHudPostLoadClear_prepare(EspPlayerView_view(),
                                         EspPlayerFacing_view(),
                                         &next);
    if (status != ESP_HUD_POST_LOAD_CLEAR_OK) return status;

    hudPostLoadClearState = next;
    return ESP_HUD_POST_LOAD_CLEAR_OK;
}
