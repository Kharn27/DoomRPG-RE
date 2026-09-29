#include <stddef.h>
#include <string.h>

#include "esp_map_automap_state.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_post_load_view_invalidation_state.h"

#define ESP_POST_LOAD_VIEW_TARGET_MAP 9U
#define ESP_POST_LOAD_VIEW_SOURCE_BYTES 21051U
#define ESP_POST_LOAD_VIEW_SOURCE_CRC32 0x4a2c5800U
#define ESP_POST_LOAD_VIEW_RUNTIME_FNV 0xbc432a0fU
#define ESP_POST_LOAD_VIEW_MAP_FNV 0x8dba0bb4U
#define ESP_POST_LOAD_VIEW_AUTOMAP_FNV 0xb699bd75U

static EspPostLoadViewInvalidationState viewInvalidationState;

static int cleanupCanonical(
    const EspPostLoadEventParticleCleanupState* state) {
    return state != NULL && state->active == 1U &&
           state->targetMapId == ESP_POST_LOAD_VIEW_TARGET_MAP &&
           state->numEventsBefore == 0U &&
           state->numEventsAfterFirstClear == 0U &&
           state->particleCountBefore == 0U &&
           state->particleCountAfterClear == 0U &&
           state->numEventsAfterSecondClear == 0U && state->reserved == 0U;
}

static int junctionWorldAtCallerBoundary(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapStateView* mapState = EspMapState_view();
    const EspMapAutomapStateView* automap = EspMapAutomapState_view();

    return runtime != NULL && mapState != NULL && automap != NULL &&
           runtime->sourceBytes == ESP_POST_LOAD_VIEW_SOURCE_BYTES &&
           runtime->sourceCrc32 == ESP_POST_LOAD_VIEW_SOURCE_CRC32 &&
           runtime->arenaFNV1a == ESP_POST_LOAD_VIEW_RUNTIME_FNV &&
           mapState->stateFNV1a == ESP_POST_LOAD_VIEW_MAP_FNV &&
           automap->stateFNV1a == ESP_POST_LOAD_VIEW_AUTOMAP_FNV;
}

void EspPostLoadViewInvalidation_reset(void) {
    memset(&viewInvalidationState, 0, sizeof(viewInvalidationState));
}

int EspPostLoadViewInvalidation_isReady(void) {
    return viewInvalidationState.active == 1U;
}

const EspPostLoadViewInvalidationState* EspPostLoadViewInvalidation_view(void) {
    return EspPostLoadViewInvalidation_isReady() ? &viewInvalidationState : NULL;
}

EspPostLoadViewInvalidationStatus EspPostLoadViewInvalidation_prepare(
    const EspPostLoadEventParticleCleanupState* cleanup,
    uint8_t isUpdateViewBefore,
    EspPostLoadViewInvalidationState* outState) {
    EspPostLoadViewInvalidationState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (cleanup == NULL || outState == NULL) {
        return ESP_POST_LOAD_VIEW_INVALIDATION_INVALID;
    }
    if (!cleanupCanonical(cleanup)) {
        return ESP_POST_LOAD_VIEW_INVALIDATION_CLEANUP_INVALID;
    }
    if (isUpdateViewBefore > 1U) {
        return ESP_POST_LOAD_VIEW_INVALIDATION_UNSUPPORTED_CONTEXT;
    }
    if (EspPostLoadViewInvalidation_isReady()) {
        return ESP_POST_LOAD_VIEW_INVALIDATION_ALREADY_ACTIVE;
    }
    if (!junctionWorldAtCallerBoundary()) {
        return ESP_POST_LOAD_VIEW_INVALIDATION_WORLD_NOT_READY;
    }

    memset(&next, 0, sizeof(next));
    next.isUpdateViewBefore = isUpdateViewBefore;
    next.isUpdateViewAfter = 1U;
    next.targetMapId = cleanup->targetMapId;
    next.active = 1U;
    *outState = next;
    return ESP_POST_LOAD_VIEW_INVALIDATION_OK;
}

EspPostLoadViewInvalidationStatus EspPostLoadViewInvalidation_route(
    uint8_t isUpdateViewBefore) {
    EspPostLoadViewInvalidationState prepared;
    EspPostLoadViewInvalidationStatus status;

    if (EspPostLoadViewInvalidation_isReady()) {
        return ESP_POST_LOAD_VIEW_INVALIDATION_ALREADY_ACTIVE;
    }

    status = EspPostLoadViewInvalidation_prepare(
        EspPostLoadEventParticleCleanup_view(), isUpdateViewBefore, &prepared);
    if (status != ESP_POST_LOAD_VIEW_INVALIDATION_OK) return status;

    viewInvalidationState = prepared;
    return ESP_POST_LOAD_VIEW_INVALIDATION_OK;
}
