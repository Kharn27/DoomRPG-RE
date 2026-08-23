#include <stddef.h>
#include <string.h>

#include "esp_map_automap_state.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_post_load_playing_transition_state.h"

#define ESP_POST_LOAD_PLAYING_TARGET_MAP 9U
#define ESP_POST_LOAD_PLAYING_SOURCE_BYTES 21051U
#define ESP_POST_LOAD_PLAYING_SOURCE_CRC32 0x4a2c5800U
#define ESP_POST_LOAD_PLAYING_RUNTIME_FNV 0xbc432a0fU
#define ESP_POST_LOAD_PLAYING_MAP_FNV 0x8dba0bb4U
#define ESP_POST_LOAD_PLAYING_AUTOMAP_FNV 0xb699bd75U

static EspPostLoadPlayingTransitionState playingTransitionState;

static int viewInvalidationCanonical(
    const EspPostLoadViewInvalidationState* state) {
    return state != NULL && state->active == 1U &&
           state->targetMapId == ESP_POST_LOAD_PLAYING_TARGET_MAP &&
           state->isUpdateViewBefore == 1U &&
           state->isUpdateViewAfter == 1U;
}

static int junctionWorldAtCallerBoundary(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapStateView* mapState = EspMapState_view();
    const EspMapAutomapStateView* automap = EspMapAutomapState_view();

    return runtime != NULL && mapState != NULL && automap != NULL &&
           runtime->sourceBytes == ESP_POST_LOAD_PLAYING_SOURCE_BYTES &&
           runtime->sourceCrc32 == ESP_POST_LOAD_PLAYING_SOURCE_CRC32 &&
           runtime->arenaFNV1a == ESP_POST_LOAD_PLAYING_RUNTIME_FNV &&
           mapState->stateFNV1a == ESP_POST_LOAD_PLAYING_MAP_FNV &&
           automap->stateFNV1a == ESP_POST_LOAD_PLAYING_AUTOMAP_FNV;
}

void EspPostLoadPlayingTransition_reset(void) {
    memset(&playingTransitionState, 0, sizeof(playingTransitionState));
}

int EspPostLoadPlayingTransition_isReady(void) {
    return playingTransitionState.active == 1U;
}

const EspPostLoadPlayingTransitionState* EspPostLoadPlayingTransition_view(void) {
    return EspPostLoadPlayingTransition_isReady() ? &playingTransitionState : NULL;
}

EspPostLoadPlayingTransitionStatus EspPostLoadPlayingTransition_prepare(
    const EspPostLoadViewInvalidationState* viewInvalidation,
    uint8_t monstersTurnBefore,
    uint8_t displaySoftKeysBefore,
    uint8_t restoreSoftKeysBefore,
    uint8_t skipCheckStateBefore,
    EspPostLoadPlayingTransitionState* outState) {
    EspPostLoadPlayingTransitionState next;
    uint8_t softKeyRequested;
    uint8_t softKeyVisible;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (viewInvalidation == NULL || outState == NULL) {
        return ESP_POST_LOAD_PLAYING_TRANSITION_INVALID;
    }
    if (!viewInvalidationCanonical(viewInvalidation)) {
        return ESP_POST_LOAD_PLAYING_TRANSITION_VIEW_INVALID;
    }
    if (monstersTurnBefore > 1U || displaySoftKeysBefore > 1U ||
        restoreSoftKeysBefore > 1U || skipCheckStateBefore > 1U) {
        return ESP_POST_LOAD_PLAYING_TRANSITION_UNSUPPORTED_CONTEXT;
    }
    if (EspPostLoadPlayingTransition_isReady()) {
        return ESP_POST_LOAD_PLAYING_TRANSITION_ALREADY_ACTIVE;
    }
    if (!junctionWorldAtCallerBoundary()) {
        return ESP_POST_LOAD_PLAYING_TRANSITION_WORLD_NOT_READY;
    }

    softKeyRequested = monstersTurnBefore == 0U ? 1U : 0U;
    softKeyVisible = (softKeyRequested != 0U && displaySoftKeysBefore != 0U)
                         ? 1U
                         : 0U;

    memset(&next, 0, sizeof(next));
    next.stateBefore = ESP_POST_LOAD_PLAYING_STATE_INTRO;
    next.stateAfter = ESP_POST_LOAD_PLAYING_STATE_PLAYING;
    next.monstersTurnBefore = monstersTurnBefore;
    next.displaySoftKeysBefore = displaySoftKeysBefore;
    next.restoreSoftKeysBefore = restoreSoftKeysBefore;
    next.restoreSoftKeysAfter = softKeyVisible;
    next.skipCheckStateBefore = skipCheckStateBefore;
    next.skipCheckStateAfter = 1U;
    next.softKeyIntent = softKeyRequested != 0U
                             ? ESP_POST_LOAD_PLAYING_SOFTKEY_MENU_MAP
                             : ESP_POST_LOAD_PLAYING_SOFTKEY_NONE;
    next.softKeyPresentationDeferred = softKeyVisible;
    next.targetMapId = viewInvalidation->targetMapId;
    next.active = 1U;
    *outState = next;
    return ESP_POST_LOAD_PLAYING_TRANSITION_OK;
}

EspPostLoadPlayingTransitionStatus EspPostLoadPlayingTransition_route(
    uint8_t monstersTurnBefore,
    uint8_t displaySoftKeysBefore,
    uint8_t restoreSoftKeysBefore,
    uint8_t skipCheckStateBefore) {
    EspPostLoadPlayingTransitionState prepared;
    EspPostLoadPlayingTransitionStatus status;

    if (EspPostLoadPlayingTransition_isReady()) {
        return ESP_POST_LOAD_PLAYING_TRANSITION_ALREADY_ACTIVE;
    }

    status = EspPostLoadPlayingTransition_prepare(
        EspPostLoadViewInvalidation_view(), monstersTurnBefore,
        displaySoftKeysBefore, restoreSoftKeysBefore, skipCheckStateBefore,
        &prepared);
    if (status != ESP_POST_LOAD_PLAYING_TRANSITION_OK) return status;

    playingTransitionState = prepared;
    return ESP_POST_LOAD_PLAYING_TRANSITION_OK;
}
