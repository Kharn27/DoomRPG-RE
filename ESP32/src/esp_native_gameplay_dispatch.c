#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_native_gameplay_dispatch.h"

#define FIXED_ONE 65536L
#define STEP_SIZE 64L

typedef char EspNativeGameplayTurnState_must_be_24_bytes[
    sizeof(EspNativeGameplayTurnState) == 24U ? 1 : -1];
typedef char EspNativeGameplayDispatchResult_must_be_12_bytes[
    sizeof(EspNativeGameplayDispatchResult) == 12U ? 1 : -1];

static EspNativeGameplayTurnState turnState;

static int knownAction(uint8_t action) {
    switch (action) {
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD:
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_BACK:
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT:
    case ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT:
    case ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN:
    case ESP_NATIVE_GAMEPLAY_ACTION_SELECT:
    case ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP:
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_LEFT:
    case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_RIGHT:
    case ESP_NATIVE_GAMEPLAY_ACTION_PREV_WEAPON:
    case ESP_NATIVE_GAMEPLAY_ACTION_NEXT_WEAPON:
    case ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN:
        return 1;
    default:
        return 0;
    }
}

static int settledView(const EspPlayerViewState* view) {
    return view != NULL && view->active == 1U && view->spawnApplied == 1U &&
           view->viewX == view->destX && view->viewY == view->destY &&
           view->viewAngle == view->destAngle &&
           view->viewAngle >= 0 && view->viewAngle <= 255 &&
           (view->viewAngle & 63) == 0 &&
           view->hudRefreshPending == 0U &&
           view->facingRefreshPending == 0U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 0U;
}

static int deriveOrientation(uint8_t angle,
                             uint32_t sequence,
                             uint8_t action,
                             EspNativeGameplayTurnState* out) {
    EspNativeGameplayTurnState next;
    if (out == NULL || (angle & 63U) != 0U) return 0;
    memset(&next, 0, sizeof(next));
    switch (angle) {
    case 0U:
        next.viewSin = 0;
        next.viewCos = (int32_t)FIXED_ONE;
        next.viewStepX = (int32_t)STEP_SIZE;
        next.viewStepY = 0;
        break;
    case 64U:
        next.viewSin = (int32_t)FIXED_ONE;
        next.viewCos = 0;
        next.viewStepX = 0;
        next.viewStepY = -(int32_t)STEP_SIZE;
        break;
    case 128U:
        next.viewSin = 0;
        next.viewCos = -(int32_t)FIXED_ONE;
        next.viewStepX = -(int32_t)STEP_SIZE;
        next.viewStepY = 0;
        break;
    case 192U:
        next.viewSin = -(int32_t)FIXED_ONE;
        next.viewCos = 0;
        next.viewStepX = 0;
        next.viewStepY = (int32_t)STEP_SIZE;
        break;
    default:
        return 0;
    }
    next.sequence = sequence;
    next.destAngle = angle;
    next.lastAction = action;
    next.active = 1U;
    *out = next;
    return 1;
}

static int intentShapeValid(const EspNativeGameplayInputState* intent) {
    return intent != NULL && intent->active == 1U && intent->pending == 1U &&
           intent->sequence != 0U && knownAction(intent->action);
}

static int candidateMatchesResult(
    const EspPlayerViewState* beforeView,
    const EspPlayerViewState* afterView,
    const EspNativeGameplayTurnState* beforeTurn,
    const EspNativeGameplayTurnState* afterTurn,
    const EspNativeGameplayDispatchResult* result) {
    if (beforeView == NULL || afterView == NULL || beforeTurn == NULL ||
        afterTurn == NULL || result == NULL || result->prepared != 1U ||
        result->committed != 0U || result->rolledBack != 0U) return 0;
    if (result->action != ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT &&
        result->action != ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT) return 0;
    if (result->angleDelta != 64 && result->angleDelta != -64) return 0;
    if (result->angleBefore != (uint8_t)beforeView->viewAngle ||
        result->angleAfter != (uint8_t)afterView->viewAngle ||
        afterView->viewAngle != afterView->destAngle ||
        beforeTurn->destAngle != result->angleBefore ||
        afterTurn->destAngle != result->angleAfter ||
        afterTurn->sequence != result->sequence ||
        afterTurn->lastAction != result->action ||
        afterTurn->active != 1U) return 0;
    return 1;
}

void EspNativeGameplayDispatch_reset(void) {
    memset(&turnState, 0, sizeof(turnState));
}

int EspNativeGameplayDispatch_isReady(void) {
    return turnState.active == 1U;
}

const EspNativeGameplayTurnState* EspNativeGameplayDispatch_view(void) {
    return EspNativeGameplayDispatch_isReady() ? &turnState : NULL;
}

EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_adoptView(void) {
    const EspPlayerViewState* view;
    EspNativeGameplayTurnState next;
    if (EspNativeGameplayDispatch_isReady()) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_ALREADY_ACTIVE;
    }
    view = EspPlayerView_view();
    if (!settledView(view)) return ESP_NATIVE_GAMEPLAY_DISPATCH_VIEW_NOT_READY;
    if (!deriveOrientation((uint8_t)view->viewAngle, 0U,
                           ESP_NATIVE_GAMEPLAY_ACTION_NONE, &next)) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_VIEW_NOT_READY;
    }
    turnState = next;
    return ESP_NATIVE_GAMEPLAY_DISPATCH_OK;
}

EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_prepareTurn(
    const EspNativeGameplayInputState* intent,
    EspPlayerViewState* outBeforeView,
    EspPlayerViewState* outAfterView,
    EspNativeGameplayTurnState* outBeforeTurn,
    EspNativeGameplayTurnState* outAfterTurn,
    EspNativeGameplayDispatchResult* outResult) {
    EspPlayerViewTurnStatus viewStatus;
    int32_t delta;

    if (outBeforeView != NULL) memset(outBeforeView, 0, sizeof(*outBeforeView));
    if (outAfterView != NULL) memset(outAfterView, 0, sizeof(*outAfterView));
    if (outBeforeTurn != NULL) memset(outBeforeTurn, 0, sizeof(*outBeforeTurn));
    if (outAfterTurn != NULL) memset(outAfterTurn, 0, sizeof(*outAfterTurn));
    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (!intentShapeValid(intent) || outBeforeView == NULL || outAfterView == NULL ||
        outBeforeTurn == NULL || outAfterTurn == NULL || outResult == NULL) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID;
    }

    outResult->sequence = intent->sequence;
    outResult->action = intent->action;
    if (intent->action != ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT &&
        intent->action != ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED;
    }
    if (!EspNativeGameplayDispatch_isReady()) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_VIEW_NOT_READY;
    }

    delta = intent->action == ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT ? 64 : -64;
    viewStatus = EspPlayerView_prepareQuarterTurn(
        delta, outBeforeView, outAfterView);
    if (viewStatus != ESP_PLAYER_VIEW_TURN_OK) {
        return viewStatus == ESP_PLAYER_VIEW_TURN_STALE
                   ? ESP_NATIVE_GAMEPLAY_DISPATCH_STALE
                   : ESP_NATIVE_GAMEPLAY_DISPATCH_VIEW_NOT_READY;
    }
    if (turnState.destAngle != (uint8_t)outBeforeView->viewAngle) {
        memset(outBeforeView, 0, sizeof(*outBeforeView));
        memset(outAfterView, 0, sizeof(*outAfterView));
        return ESP_NATIVE_GAMEPLAY_DISPATCH_STALE;
    }

    *outBeforeTurn = turnState;
    if (!deriveOrientation((uint8_t)outAfterView->viewAngle,
                           intent->sequence, intent->action, outAfterTurn)) {
        memset(outBeforeView, 0, sizeof(*outBeforeView));
        memset(outAfterView, 0, sizeof(*outAfterView));
        memset(outBeforeTurn, 0, sizeof(*outBeforeTurn));
        return ESP_NATIVE_GAMEPLAY_DISPATCH_VIEW_NOT_READY;
    }

    outResult->angleDelta = (int8_t)delta;
    outResult->angleBefore = (uint8_t)outBeforeView->viewAngle;
    outResult->angleAfter = (uint8_t)outAfterView->viewAngle;
    outResult->prepared = 1U;
    return ESP_NATIVE_GAMEPLAY_DISPATCH_PREPARED;
}

EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_commitTurn(
    const EspPlayerViewState* expectedBeforeView,
    const EspPlayerViewState* preparedAfterView,
    const EspNativeGameplayTurnState* expectedBeforeTurn,
    const EspNativeGameplayTurnState* preparedAfterTurn,
    EspNativeGameplayDispatchResult* ioResult) {
    EspPlayerViewTurnStatus viewStatus;

    if (!candidateMatchesResult(expectedBeforeView, preparedAfterView,
                                expectedBeforeTurn, preparedAfterTurn, ioResult)) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID;
    }
    if (!EspNativeGameplayDispatch_isReady() ||
        memcmp(&turnState, expectedBeforeTurn, sizeof(turnState)) != 0) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_STALE;
    }

    viewStatus = EspPlayerView_commitPreparedTurn(expectedBeforeView,
                                                  preparedAfterView);
    if (viewStatus != ESP_PLAYER_VIEW_TURN_OK) {
        return viewStatus == ESP_PLAYER_VIEW_TURN_STALE
                   ? ESP_NATIVE_GAMEPLAY_DISPATCH_STALE
                   : ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
    }

    turnState = *preparedAfterTurn;
    ioResult->committed = 1U;
    return ESP_NATIVE_GAMEPLAY_DISPATCH_OK;
}

EspNativeGameplayDispatchStatus EspNativeGameplayDispatch_rollbackTurn(
    const EspPlayerViewState* expectedAfterView,
    const EspPlayerViewState* restoreBeforeView,
    const EspNativeGameplayTurnState* expectedAfterTurn,
    const EspNativeGameplayTurnState* restoreBeforeTurn,
    EspNativeGameplayDispatchResult* ioResult) {
    EspPlayerViewTurnStatus viewStatus;

    if (expectedAfterView == NULL || restoreBeforeView == NULL ||
        expectedAfterTurn == NULL || restoreBeforeTurn == NULL ||
        ioResult == NULL || ioResult->prepared != 1U ||
        ioResult->committed != 1U || ioResult->rolledBack != 0U) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID;
    }
    if (!EspNativeGameplayDispatch_isReady() ||
        memcmp(&turnState, expectedAfterTurn, sizeof(turnState)) != 0) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_STALE;
    }

    viewStatus = EspPlayerView_commitPreparedTurn(expectedAfterView,
                                                  restoreBeforeView);
    if (viewStatus != ESP_PLAYER_VIEW_TURN_OK) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
    }
    turnState = *restoreBeforeTurn;
    ioResult->committed = 0U;
    ioResult->rolledBack = 1U;
    return ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK;
}
