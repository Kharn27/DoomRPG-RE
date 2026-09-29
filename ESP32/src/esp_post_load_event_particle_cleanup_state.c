#include <stddef.h>
#include <string.h>

#include "esp_map_automap_state.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_post_load_event_particle_cleanup_state.h"

#define ESP_POST_LOAD_EP_TARGET_MAP 9U
#define ESP_POST_LOAD_EP_SOURCE_BYTES 21051U
#define ESP_POST_LOAD_EP_SOURCE_CRC32 0x4a2c5800U
#define ESP_POST_LOAD_EP_RUNTIME_FNV 0xbc432a0fU
#define ESP_POST_LOAD_EP_MAP_FNV 0x8dba0bb4U
#define ESP_POST_LOAD_EP_AUTOMAP_FNV 0xb699bd75U

static EspPostLoadEventParticleCleanupState cleanupState;

static int flagCleanupCanonical(const EspPostLoadFlagCleanupState* state) {
    return state != NULL && state->active == 1U &&
           state->targetMapId == ESP_POST_LOAD_EP_TARGET_MAP &&
           state->isLoadedBefore == 0U && state->isSavedBefore == 0U &&
           state->activeLoadTypeBefore == 0U &&
           state->isLoadedAfter == 0U && state->isSavedAfter == 0U &&
           state->activeLoadTypeAfter == 0U;
}

static int junctionWorldAtCallerBoundary(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapStateView* mapState = EspMapState_view();
    const EspMapAutomapStateView* automap = EspMapAutomapState_view();

    return runtime != NULL && mapState != NULL && automap != NULL &&
           runtime->sourceBytes == ESP_POST_LOAD_EP_SOURCE_BYTES &&
           runtime->sourceCrc32 == ESP_POST_LOAD_EP_SOURCE_CRC32 &&
           runtime->arenaFNV1a == ESP_POST_LOAD_EP_RUNTIME_FNV &&
           mapState->stateFNV1a == ESP_POST_LOAD_EP_MAP_FNV &&
           automap->stateFNV1a == ESP_POST_LOAD_EP_AUTOMAP_FNV;
}

void EspPostLoadEventParticleCleanup_reset(void) {
    memset(&cleanupState, 0, sizeof(cleanupState));
}

int EspPostLoadEventParticleCleanup_isReady(void) {
    return cleanupState.active == 1U;
}

const EspPostLoadEventParticleCleanupState*
EspPostLoadEventParticleCleanup_view(void) {
    return EspPostLoadEventParticleCleanup_isReady() ? &cleanupState : NULL;
}

EspPostLoadEventParticleCleanupStatus
EspPostLoadEventParticleCleanup_prepare(
    const EspPostLoadFlagCleanupState* flagCleanup,
    uint8_t numEventsBefore,
    uint8_t particleCountBefore,
    EspPostLoadEventParticleCleanupState* outState) {
    EspPostLoadEventParticleCleanupState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (flagCleanup == NULL || outState == NULL) {
        return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_INVALID;
    }
    if (!flagCleanupCanonical(flagCleanup)) {
        return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_FLAG_STATE_INVALID;
    }
    if (numEventsBefore > 8U || particleCountBefore > 64U) {
        return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_UNSUPPORTED_CONTEXT;
    }
    if (numEventsBefore != 0U) {
        return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_EVENTS_NOT_EMPTY;
    }
    if (particleCountBefore != 0U) {
        return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_PARTICLES_NOT_EMPTY;
    }
    if (EspPostLoadEventParticleCleanup_isReady()) {
        return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_ALREADY_ACTIVE;
    }
    if (!junctionWorldAtCallerBoundary()) {
        return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_WORLD_NOT_READY;
    }

    memset(&next, 0, sizeof(next));
    next.numEventsBefore = numEventsBefore;
    next.numEventsAfterFirstClear = 0U;
    next.particleCountBefore = particleCountBefore;
    next.particleCountAfterClear = 0U;
    next.numEventsAfterSecondClear = 0U;
    next.targetMapId = flagCleanup->targetMapId;
    next.active = 1U;
    *outState = next;
    return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_OK;
}

EspPostLoadEventParticleCleanupStatus
EspPostLoadEventParticleCleanup_route(
    uint8_t numEventsBefore,
    uint8_t particleCountBefore) {
    EspPostLoadEventParticleCleanupState prepared;
    EspPostLoadEventParticleCleanupStatus status;

    if (EspPostLoadEventParticleCleanup_isReady()) {
        return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_ALREADY_ACTIVE;
    }

    status = EspPostLoadEventParticleCleanup_prepare(
        EspPostLoadFlagCleanup_view(), numEventsBefore, particleCountBefore,
        &prepared);
    if (status != ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_OK) return status;

    cleanupState = prepared;
    return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_OK;
}
