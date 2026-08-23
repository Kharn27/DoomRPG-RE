#include <stddef.h>
#include <string.h>

#include "esp_map_automap_state.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_native_playing_service_state.h"

#define ESP_NATIVE_PLAYING_TARGET_MAP 9U
#define ESP_NATIVE_PLAYING_SOURCE_BYTES 21051U
#define ESP_NATIVE_PLAYING_SOURCE_CRC32 0x4a2c5800U
#define ESP_NATIVE_PLAYING_RUNTIME_FNV 0xbc432a0fU
#define ESP_NATIVE_PLAYING_MAP_FNV 0x8dba0bb4U
#define ESP_NATIVE_PLAYING_AUTOMAP_FNV 0xb699bd75U
#define ESP_NATIVE_PLAYING_IDLE_DELTA_MS 8000

static EspNativePlayingServiceState playingServiceState;

static int playingCanonical(
    const EspPostLoadPlayingTransitionState* state) {
    return state != NULL && state->active == 1U &&
           state->targetMapId == ESP_NATIVE_PLAYING_TARGET_MAP &&
           state->stateBefore == ESP_POST_LOAD_PLAYING_STATE_INTRO &&
           state->stateAfter == ESP_POST_LOAD_PLAYING_STATE_PLAYING &&
           state->monstersTurnBefore == 0U &&
           state->displaySoftKeysBefore == 0U &&
           state->restoreSoftKeysBefore == 0U &&
           state->restoreSoftKeysAfter == 0U &&
           state->skipCheckStateBefore == 0U &&
           state->skipCheckStateAfter == 1U &&
           state->softKeyIntent == ESP_POST_LOAD_PLAYING_SOFTKEY_MENU_MAP &&
           state->softKeyPresentationDeferred == 0U;
}

static int idleCanonical(const EspPostLoadIdleTimeState* state) {
    return state != NULL && state->active == 1U &&
           state->targetMapId == ESP_NATIVE_PLAYING_TARGET_MAP &&
           state->reserved0 == 0U && state->reserved1 == 0U &&
           state->timeBefore >= 0 &&
           state->idleTimeAfter >= state->timeBefore &&
           (state->idleTimeAfter - state->timeBefore) ==
               ESP_NATIVE_PLAYING_IDLE_DELTA_MS;
}

static int junctionWorldAtPlayingBoundary(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapStateView* mapState = EspMapState_view();
    const EspMapAutomapStateView* automap = EspMapAutomapState_view();

    return runtime != NULL && mapState != NULL && automap != NULL &&
           runtime->sourceBytes == ESP_NATIVE_PLAYING_SOURCE_BYTES &&
           runtime->sourceCrc32 == ESP_NATIVE_PLAYING_SOURCE_CRC32 &&
           runtime->arenaFNV1a == ESP_NATIVE_PLAYING_RUNTIME_FNV &&
           mapState->stateFNV1a == ESP_NATIVE_PLAYING_MAP_FNV &&
           automap->stateFNV1a == ESP_NATIVE_PLAYING_AUTOMAP_FNV;
}

void EspNativePlayingService_reset(void) {
    memset(&playingServiceState, 0, sizeof(playingServiceState));
}

int EspNativePlayingService_isReady(void) {
    return playingServiceState.active == 1U;
}

const EspNativePlayingServiceState* EspNativePlayingService_view(void) {
    return EspNativePlayingService_isReady() ? &playingServiceState : NULL;
}

EspNativePlayingServiceStatus EspNativePlayingService_prepare(
    const EspPostLoadPlayingTransitionState* playingTransition,
    const EspPostLoadIdleTimeState* idleTime,
    uint8_t inputCountBefore,
    EspNativePlayingServiceState* outState) {
    EspNativePlayingServiceState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (playingTransition == NULL || idleTime == NULL || outState == NULL) {
        return ESP_NATIVE_PLAYING_SERVICE_INVALID;
    }
    if (!playingCanonical(playingTransition)) {
        return ESP_NATIVE_PLAYING_SERVICE_PLAYING_INVALID;
    }
    if (!idleCanonical(idleTime)) {
        return ESP_NATIVE_PLAYING_SERVICE_IDLE_INVALID;
    }
    if (inputCountBefore != 0U) {
        return ESP_NATIVE_PLAYING_SERVICE_INPUT_PENDING;
    }
    if (EspNativePlayingService_isReady()) {
        return ESP_NATIVE_PLAYING_SERVICE_ALREADY_ACTIVE;
    }
    if (!junctionWorldAtPlayingBoundary()) {
        return ESP_NATIVE_PLAYING_SERVICE_WORLD_NOT_READY;
    }

    memset(&next, 0, sizeof(next));
    next.nativeState = ESP_POST_LOAD_PLAYING_STATE_PLAYING;
    next.serviceOrdinal = 1U;
    next.inputCountBefore = inputCountBefore;
    next.inputConsumed = 0U;
    next.gameplayDispatched = 0U;
    next.renderIntent = 1U;
    next.renderDeferred = 1U;
    next.presentationDeferred = 1U;
    next.hudIntent = 1U;
    next.targetMapId = playingTransition->targetMapId;
    next.active = 1U;
    next.reserved = 0U;
    *outState = next;
    return ESP_NATIVE_PLAYING_SERVICE_OK;
}

EspNativePlayingServiceStatus EspNativePlayingService_route(
    uint8_t inputCountBefore) {
    EspNativePlayingServiceState prepared;
    EspNativePlayingServiceStatus status;

    if (EspNativePlayingService_isReady()) {
        return ESP_NATIVE_PLAYING_SERVICE_ALREADY_ACTIVE;
    }

    status = EspNativePlayingService_prepare(
        EspPostLoadPlayingTransition_view(), EspPostLoadIdleTime_view(),
        inputCountBefore, &prepared);
    if (status != ESP_NATIVE_PLAYING_SERVICE_OK) return status;

    playingServiceState = prepared;
    return ESP_NATIVE_PLAYING_SERVICE_OK;
}
