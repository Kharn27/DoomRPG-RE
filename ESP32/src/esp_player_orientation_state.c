#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_player_orientation_state.h"

static EspPlayerOrientationState orientationState;

static int viewIsReadyForOrientation(const EspPlayerViewState* view) {
    return view != NULL && view->active == 1U && view->spawnApplied == 1U &&
           view->loadType == 0U && view->hudRefreshPending == 0U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 0U &&
           view->facingRefreshPending == 1U;
}

static int tileMatchesView(const EspPlayerInitialTileState* tile,
                           const EspPlayerViewState* view) {
    return tile != NULL && view != NULL && tile->active == 1U &&
           tile->targetMapId == view->targetMapId &&
           tile->gameplayLoadMapId == view->gameplayLoadMapId &&
           tile->loadType == view->loadType;
}

void EspPlayerOrientation_reset(void) {
    memset(&orientationState, 0, sizeof(orientationState));
}

int EspPlayerOrientation_isReady(void) {
    return orientationState.active == 1U && orientationState.prepared == 1U;
}

const EspPlayerOrientationState* EspPlayerOrientation_view(void) {
    return EspPlayerOrientation_isReady() ? &orientationState : NULL;
}

EspPlayerOrientationStatus EspPlayerOrientation_prepare(
    const EspPlayerViewState* playerView,
    const EspPlayerInitialTileState* initialTile,
    EspPlayerOrientationState* outState) {
    EspPlayerOrientationState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (playerView == NULL || initialTile == NULL || outState == NULL) {
        return ESP_PLAYER_ORIENTATION_INVALID;
    }
    if (!viewIsReadyForOrientation(playerView)) {
        if (playerView->active != 1U || playerView->spawnApplied != 1U) {
            return ESP_PLAYER_ORIENTATION_VIEW_INVALID;
        }
        return ESP_PLAYER_ORIENTATION_UNSUPPORTED_ORDER;
    }
    if (initialTile->active != 1U) {
        return ESP_PLAYER_ORIENTATION_TILE_INVALID;
    }
    if (!tileMatchesView(initialTile, playerView)) {
        return ESP_PLAYER_ORIENTATION_TILE_INVALID;
    }
    if (playerView->destAngle != (int32_t)ESP_PLAYER_ORIENTATION_ANGLE_64 ||
        playerView->viewAngle != playerView->destAngle) {
        return ESP_PLAYER_ORIENTATION_UNSUPPORTED_CONTEXT;
    }

    memset(&next, 0, sizeof(next));
    next.viewSin = (int32_t)ESP_PLAYER_ORIENTATION_FIXED_ONE;
    next.viewCos = 0;
    next.viewStepX = 0;
    next.viewStepY = -(int32_t)ESP_PLAYER_ORIENTATION_STEP_SIZE;
    next.targetMapId = playerView->targetMapId;
    next.gameplayLoadMapId = playerView->gameplayLoadMapId;
    next.loadType = playerView->loadType;
    next.destAngle = (uint8_t)playerView->destAngle;
    next.prepared = 1U;
    next.active = 1U;

    *outState = next;
    return ESP_PLAYER_ORIENTATION_OK;
}

EspPlayerOrientationStatus EspPlayerOrientation_route(void) {
    EspPlayerOrientationState next;
    EspPlayerOrientationStatus status;
    const EspPlayerViewState* playerView;
    const EspPlayerInitialTileState* initialTile;

    if (EspPlayerOrientation_isReady()) {
        return ESP_PLAYER_ORIENTATION_ALREADY_ACTIVE;
    }

    playerView = EspPlayerView_view();
    initialTile = EspPlayerInitialTile_view();
    status = EspPlayerOrientation_prepare(playerView, initialTile, &next);
    if (status != ESP_PLAYER_ORIENTATION_OK) return status;

    orientationState = next;
    return ESP_PLAYER_ORIENTATION_OK;
}
