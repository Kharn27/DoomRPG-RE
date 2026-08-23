#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "esp_map_automap_state.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_post_load_idle_time_state.h"

#define ESP_POST_LOAD_IDLE_TARGET_MAP 9U
#define ESP_POST_LOAD_IDLE_SOURCE_BYTES 21051U
#define ESP_POST_LOAD_IDLE_SOURCE_CRC32 0x4a2c5800U
#define ESP_POST_LOAD_IDLE_RUNTIME_FNV 0xbc432a0fU
#define ESP_POST_LOAD_IDLE_MAP_FNV 0x8dba0bb4U
#define ESP_POST_LOAD_IDLE_AUTOMAP_FNV 0xb699bd75U
#define ESP_POST_LOAD_IDLE_DELAY_MS 8000

static EspPostLoadIdleTimeState idleTimeState;

static int playingTransitionCanonical(
    const EspPostLoadPlayingTransitionState* state) {
    return state != NULL && state->active == 1U &&
           state->targetMapId == ESP_POST_LOAD_IDLE_TARGET_MAP &&
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

static int junctionWorldAtCallerBoundary(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapStateView* mapState = EspMapState_view();
    const EspMapAutomapStateView* automap = EspMapAutomapState_view();

    return runtime != NULL && mapState != NULL && automap != NULL &&
           runtime->sourceBytes == ESP_POST_LOAD_IDLE_SOURCE_BYTES &&
           runtime->sourceCrc32 == ESP_POST_LOAD_IDLE_SOURCE_CRC32 &&
           runtime->arenaFNV1a == ESP_POST_LOAD_IDLE_RUNTIME_FNV &&
           mapState->stateFNV1a == ESP_POST_LOAD_IDLE_MAP_FNV &&
           automap->stateFNV1a == ESP_POST_LOAD_IDLE_AUTOMAP_FNV;
}

void EspPostLoadIdleTime_reset(void) {
    memset(&idleTimeState, 0, sizeof(idleTimeState));
}

int EspPostLoadIdleTime_isReady(void) {
    return idleTimeState.active == 1U;
}

const EspPostLoadIdleTimeState* EspPostLoadIdleTime_view(void) {
    return EspPostLoadIdleTime_isReady() ? &idleTimeState : NULL;
}

EspPostLoadIdleTimeStatus EspPostLoadIdleTime_prepare(
    const EspPostLoadPlayingTransitionState* playingTransition,
    int32_t timeBefore,
    int32_t idleTimeBefore,
    EspPostLoadIdleTimeState* outState) {
    EspPostLoadIdleTimeState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (playingTransition == NULL || outState == NULL) {
        return ESP_POST_LOAD_IDLE_TIME_INVALID;
    }
    if (!playingTransitionCanonical(playingTransition)) {
        return ESP_POST_LOAD_IDLE_TIME_PLAYING_INVALID;
    }
    if (timeBefore < 0 || timeBefore > INT32_MAX - ESP_POST_LOAD_IDLE_DELAY_MS) {
        return ESP_POST_LOAD_IDLE_TIME_UNSUPPORTED_CONTEXT;
    }
    if (EspPostLoadIdleTime_isReady()) {
        return ESP_POST_LOAD_IDLE_TIME_ALREADY_ACTIVE;
    }
    if (!junctionWorldAtCallerBoundary()) {
        return ESP_POST_LOAD_IDLE_TIME_WORLD_NOT_READY;
    }

    memset(&next, 0, sizeof(next));
    next.timeBefore = timeBefore;
    next.idleTimeBefore = idleTimeBefore;
    next.idleTimeAfter = timeBefore + ESP_POST_LOAD_IDLE_DELAY_MS;
    next.targetMapId = playingTransition->targetMapId;
    next.active = 1U;
    *outState = next;
    return ESP_POST_LOAD_IDLE_TIME_OK;
}

EspPostLoadIdleTimeStatus EspPostLoadIdleTime_route(
    int32_t timeBefore,
    int32_t idleTimeBefore) {
    EspPostLoadIdleTimeState prepared;
    EspPostLoadIdleTimeStatus status;

    if (EspPostLoadIdleTime_isReady()) {
        return ESP_POST_LOAD_IDLE_TIME_ALREADY_ACTIVE;
    }

    status = EspPostLoadIdleTime_prepare(
        EspPostLoadPlayingTransition_view(), timeBefore, idleTimeBefore,
        &prepared);
    if (status != ESP_POST_LOAD_IDLE_TIME_OK) return status;

    idleTimeState = prepared;
    return ESP_POST_LOAD_IDLE_TIME_OK;
}
