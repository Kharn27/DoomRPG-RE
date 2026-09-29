/*
 * Doom RPG ESP32 post-load state owners.
 *
 * Structural consolidation only: the eight historical milestone translation
 * units are kept as the same public APIs and state machines in one owner file.
 * Do not add new post-load behavior here as part of consolidation changes.
 */

/* Consolidated from esp_post_load_event_particle_cleanup_state.c; public API intentionally unchanged. */
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

static EspPostLoadEventParticleCleanupState eventParticleCleanupState;

static int flagCleanupCanonical(const EspPostLoadFlagCleanupState* state) {
    return state != NULL && state->active == 1U &&
           state->targetMapId == ESP_POST_LOAD_EP_TARGET_MAP &&
           state->isLoadedBefore == 0U && state->isSavedBefore == 0U &&
           state->activeLoadTypeBefore == 0U &&
           state->isLoadedAfter == 0U && state->isSavedAfter == 0U &&
           state->activeLoadTypeAfter == 0U;
}

static int eventParticleJunctionWorldAtCallerBoundary(void) {
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
    memset(&eventParticleCleanupState, 0, sizeof(eventParticleCleanupState));
}

int EspPostLoadEventParticleCleanup_isReady(void) {
    return eventParticleCleanupState.active == 1U;
}

const EspPostLoadEventParticleCleanupState*
EspPostLoadEventParticleCleanup_view(void) {
    return EspPostLoadEventParticleCleanup_isReady() ? &eventParticleCleanupState : NULL;
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
    if (!eventParticleJunctionWorldAtCallerBoundary()) {
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

    eventParticleCleanupState = prepared;
    return ESP_POST_LOAD_EVENT_PARTICLE_CLEANUP_OK;
}


/* Consolidated from esp_post_load_flag_cleanup_state.c; public API intentionally unchanged. */
#include <stddef.h>
#include <string.h>

#include "esp_map_automap_state.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_post_load_flag_cleanup_state.h"

#define ESP_POST_LOAD_CLEANUP_TARGET_MAP 9U
#define ESP_POST_LOAD_CLEANUP_SOURCE_BYTES 21051U
#define ESP_POST_LOAD_CLEANUP_SOURCE_CRC32 0x4a2c5800U
#define ESP_POST_LOAD_CLEANUP_RUNTIME_FNV 0xbc432a0fU
#define ESP_POST_LOAD_CLEANUP_MAP_FNV 0x8dba0bb4U
#define ESP_POST_LOAD_CLEANUP_AUTOMAP_FNV 0xb699bd75U

static EspPostLoadFlagCleanupState flagCleanupState;

static int saveIntentCanonical(
    const EspPostLoadInitialSaveIntentState* saveIntent) {
    return saveIntent != NULL && saveIntent->active == 1U &&
           saveIntent->mapId == ESP_POST_LOAD_CLEANUP_TARGET_MAP &&
           saveIntent->viewX == 992 && saveIntent->viewY == 1888 &&
           saveIntent->viewAngle == 64 && saveIntent->isLoadedBefore == 0U &&
           saveIntent->saveMode == 0U && saveIntent->saveRequired == 1U &&
           saveIntent->componentMask == ESP_POST_LOAD_SAVE_COMPONENT_ALL &&
           saveIntent->persistenceDeferred == 1U &&
           saveIntent->presentationDeferred == 1U;
}

static int flagCleanupJunctionWorldAtCallerBoundary(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapStateView* mapState = EspMapState_view();
    const EspMapAutomapStateView* automap = EspMapAutomapState_view();

    return runtime != NULL && mapState != NULL && automap != NULL &&
           runtime->sourceBytes == ESP_POST_LOAD_CLEANUP_SOURCE_BYTES &&
           runtime->sourceCrc32 == ESP_POST_LOAD_CLEANUP_SOURCE_CRC32 &&
           runtime->arenaFNV1a == ESP_POST_LOAD_CLEANUP_RUNTIME_FNV &&
           mapState->stateFNV1a == ESP_POST_LOAD_CLEANUP_MAP_FNV &&
           automap->stateFNV1a == ESP_POST_LOAD_CLEANUP_AUTOMAP_FNV;
}

void EspPostLoadFlagCleanup_reset(void) {
    memset(&flagCleanupState, 0, sizeof(flagCleanupState));
}

int EspPostLoadFlagCleanup_isReady(void) {
    return flagCleanupState.active == 1U;
}

const EspPostLoadFlagCleanupState* EspPostLoadFlagCleanup_view(void) {
    return EspPostLoadFlagCleanup_isReady() ? &flagCleanupState : NULL;
}

EspPostLoadFlagCleanupStatus EspPostLoadFlagCleanup_prepare(
    const EspPostLoadInitialSaveIntentState* saveIntent,
    uint8_t isLoadedBefore,
    uint8_t isSavedBefore,
    uint8_t activeLoadTypeBefore,
    EspPostLoadFlagCleanupState* outState) {
    EspPostLoadFlagCleanupState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (saveIntent == NULL || outState == NULL) {
        return ESP_POST_LOAD_FLAG_CLEANUP_INVALID;
    }
    if (!saveIntentCanonical(saveIntent)) {
        return ESP_POST_LOAD_FLAG_CLEANUP_SAVE_INTENT_INVALID;
    }
    if (isLoadedBefore > 1U || isSavedBefore > 1U ||
        activeLoadTypeBefore > 2U ||
        isLoadedBefore != saveIntent->isLoadedBefore) {
        return ESP_POST_LOAD_FLAG_CLEANUP_UNSUPPORTED_CONTEXT;
    }
    if (EspPostLoadFlagCleanup_isReady()) {
        return ESP_POST_LOAD_FLAG_CLEANUP_ALREADY_ACTIVE;
    }
    if (!flagCleanupJunctionWorldAtCallerBoundary()) {
        return ESP_POST_LOAD_FLAG_CLEANUP_WORLD_NOT_READY;
    }

    memset(&next, 0, sizeof(next));
    next.isLoadedBefore = isLoadedBefore;
    next.isSavedBefore = isSavedBefore;
    next.activeLoadTypeBefore = activeLoadTypeBefore;
    next.isLoadedAfter = 0U;
    next.isSavedAfter = 0U;
    next.activeLoadTypeAfter = 0U;
    next.targetMapId = saveIntent->mapId;
    next.active = 1U;
    *outState = next;
    return ESP_POST_LOAD_FLAG_CLEANUP_OK;
}

EspPostLoadFlagCleanupStatus EspPostLoadFlagCleanup_route(
    uint8_t isLoadedBefore,
    uint8_t isSavedBefore,
    uint8_t activeLoadTypeBefore) {
    EspPostLoadFlagCleanupState prepared;
    EspPostLoadFlagCleanupStatus status;

    if (EspPostLoadFlagCleanup_isReady()) {
        return ESP_POST_LOAD_FLAG_CLEANUP_ALREADY_ACTIVE;
    }

    status = EspPostLoadFlagCleanup_prepare(
        EspPostLoadInitialSaveIntent_view(), isLoadedBefore, isSavedBefore,
        activeLoadTypeBefore, &prepared);
    if (status != ESP_POST_LOAD_FLAG_CLEANUP_OK) return status;

    flagCleanupState = prepared;
    return ESP_POST_LOAD_FLAG_CLEANUP_OK;
}


/* Consolidated from esp_post_load_givemap_state.c; public API intentionally unchanged. */
#include <stddef.h>
#include <string.h>

#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_post_load_givemap_state.h"

#define ESP_POST_LOAD_GIVEMAP_JUNCTION_TARGET_MAP 9U
#define ESP_POST_LOAD_GIVEMAP_JUNCTION_GAMEPLAY_MAP 2U
#define ESP_POST_LOAD_GIVEMAP_JUNCTION_SOURCE_BYTES 21051U
#define ESP_POST_LOAD_GIVEMAP_JUNCTION_SOURCE_CRC32 0x4a2c5800U
#define ESP_POST_LOAD_GIVEMAP_JUNCTION_RUNTIME_FNV 0xbc432a0fU
#define ESP_POST_LOAD_GIVEMAP_JUNCTION_MAP_FNV 0xc5cdfc04U
#define ESP_POST_LOAD_GIVEMAP_JUNCTION_AUTOMAP_FNV 0x0b2ae445U

static EspPostLoadGiveMapState postLoadGiveMapState;

static int hudClearCanonical(const EspHudPostLoadClearState* hudClear) {
    return hudClear != NULL && hudClear->active == 1U &&
           hudClear->cleared == 1U && hudClear->messageCount == 0U &&
           hudClear->statBarMessagePresent == 0U &&
           hudClear->logMessageLength == 0U;
}

static int giveMapJunctionWorldAtCallerBoundary(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapStateView* mapState = EspMapState_view();
    const EspMapAutomapStateView* automap = EspMapAutomapState_view();

    return runtime != NULL && mapState != NULL && automap != NULL &&
           runtime->sourceBytes == ESP_POST_LOAD_GIVEMAP_JUNCTION_SOURCE_BYTES &&
           runtime->sourceCrc32 == ESP_POST_LOAD_GIVEMAP_JUNCTION_SOURCE_CRC32 &&
           runtime->arenaFNV1a == ESP_POST_LOAD_GIVEMAP_JUNCTION_RUNTIME_FNV &&
           mapState->stateFNV1a == ESP_POST_LOAD_GIVEMAP_JUNCTION_MAP_FNV &&
           automap->stateFNV1a == ESP_POST_LOAD_GIVEMAP_JUNCTION_AUTOMAP_FNV;
}

static int sameDirectResult(const EspMapGiveMapDirectResult* a,
                            const EspMapGiveMapDirectResult* b) {
    return a != NULL && b != NULL &&
           a->lineTargetCount == b->lineTargetCount &&
           a->spriteTargetCount == b->spriteTargetCount &&
           a->entranceTargetCount == b->entranceTargetCount &&
           a->linesMutated == b->linesMutated &&
           a->spritesMutated == b->spritesMutated &&
           a->tilesMutated == b->tilesMutated;
}

void EspPostLoadGiveMap_reset(void) {
    memset(&postLoadGiveMapState, 0, sizeof(postLoadGiveMapState));
}

int EspPostLoadGiveMap_isReady(void) {
    return postLoadGiveMapState.active == 1U;
}

const EspPostLoadGiveMapState* EspPostLoadGiveMap_view(void) {
    return EspPostLoadGiveMap_isReady() ? &postLoadGiveMapState : NULL;
}

EspPostLoadGiveMapStatus EspPostLoadGiveMap_prepare(
    const EspHudPostLoadClearState* hudClear,
    EspPostLoadGiveMapState* outState) {
    EspMapGiveMapDirectResult plan;
    EspMapGiveMapStatus worldStatus;
    EspPostLoadGiveMapState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (hudClear == NULL || outState == NULL) {
        return ESP_POST_LOAD_GIVEMAP_INVALID;
    }
    if (!hudClearCanonical(hudClear)) {
        return ESP_POST_LOAD_GIVEMAP_HUD_CLEAR_INVALID;
    }

    /* Only the currently recovered fresh Junction caller branch is enabled. */
    if (hudClear->targetMapId != ESP_POST_LOAD_GIVEMAP_JUNCTION_TARGET_MAP ||
        hudClear->gameplayLoadMapId != ESP_POST_LOAD_GIVEMAP_JUNCTION_GAMEPLAY_MAP ||
        hudClear->loadType != 0U) {
        return ESP_POST_LOAD_GIVEMAP_UNSUPPORTED_CONTEXT;
    }
    if (EspPostLoadGiveMap_isReady()) {
        return ESP_POST_LOAD_GIVEMAP_UNSUPPORTED_ORDER;
    }
    if (!giveMapJunctionWorldAtCallerBoundary()) {
        return ESP_POST_LOAD_GIVEMAP_WORLD_NOT_READY;
    }

    memset(&plan, 0, sizeof(plan));
    worldStatus = EspMapAutomapState_planGiveMapDirect(&plan);
    if (worldStatus != ESP_MAP_GIVEMAP_OK) {
        return ESP_POST_LOAD_GIVEMAP_WORLD_NOT_READY;
    }

    memset(&next, 0, sizeof(next));
    next.lineTargetCount = plan.lineTargetCount;
    next.spriteTargetCount = plan.spriteTargetCount;
    next.entranceTargetCount = plan.entranceTargetCount;
    next.linesMutated = plan.linesMutated;
    next.spritesMutated = plan.spritesMutated;
    next.tilesMutated = plan.tilesMutated;
    next.targetMapId = hudClear->targetMapId;
    next.gameplayLoadMapId = hudClear->gameplayLoadMapId;
    next.loadType = hudClear->loadType;
    next.active = 1U;
    *outState = next;
    return ESP_POST_LOAD_GIVEMAP_OK;
}

EspPostLoadGiveMapStatus EspPostLoadGiveMap_route(void) {
    EspPostLoadGiveMapState prepared;
    EspMapGiveMapDirectResult actual;
    EspMapGiveMapDirectResult expected;
    EspPostLoadGiveMapStatus status;
    EspMapGiveMapStatus worldStatus;

    if (EspPostLoadGiveMap_isReady()) {
        return ESP_POST_LOAD_GIVEMAP_ALREADY_ACTIVE;
    }

    status = EspPostLoadGiveMap_prepare(EspHudPostLoadClear_view(), &prepared);
    if (status != ESP_POST_LOAD_GIVEMAP_OK) return status;

    expected.lineTargetCount = prepared.lineTargetCount;
    expected.spriteTargetCount = prepared.spriteTargetCount;
    expected.entranceTargetCount = prepared.entranceTargetCount;
    expected.linesMutated = prepared.linesMutated;
    expected.spritesMutated = prepared.spritesMutated;
    expected.tilesMutated = prepared.tilesMutated;

    memset(&actual, 0, sizeof(actual));
    worldStatus = EspMapAutomapState_applyGiveMapDirect(&actual);
    if (worldStatus != ESP_MAP_GIVEMAP_OK ||
        !sameDirectResult(&expected, &actual)) {
        return ESP_POST_LOAD_GIVEMAP_APPLY_FAILED;
    }

    postLoadGiveMapState = prepared;
    return ESP_POST_LOAD_GIVEMAP_OK;
}


/* Consolidated from esp_post_load_idle_time_state.c; public API intentionally unchanged. */
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

static int idleTimeJunctionWorldAtCallerBoundary(void) {
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
    if (!idleTimeJunctionWorldAtCallerBoundary()) {
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


/* Consolidated from esp_post_load_initial_save_intent.c; public API intentionally unchanged. */
#include <stddef.h>
#include <string.h>

#include "esp_map_automap_state.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_post_load_initial_save_intent.h"

#define ESP_POST_LOAD_SAVE_JUNCTION_TARGET_MAP 9U
#define ESP_POST_LOAD_SAVE_JUNCTION_GAMEPLAY_MAP 2U
#define ESP_POST_LOAD_SAVE_JUNCTION_SOURCE_BYTES 21051U
#define ESP_POST_LOAD_SAVE_JUNCTION_SOURCE_CRC32 0x4a2c5800U
#define ESP_POST_LOAD_SAVE_JUNCTION_RUNTIME_FNV 0xbc432a0fU
#define ESP_POST_LOAD_SAVE_JUNCTION_MAP_FNV 0x8dba0bb4U
#define ESP_POST_LOAD_SAVE_JUNCTION_AUTOMAP_FNV 0xb699bd75U

static EspPostLoadInitialSaveIntentState initialSaveIntentState;

static int weaponSelectCanonical(
    const EspPostLoadWeaponSelectState* weaponSelect) {
    return weaponSelect != NULL && weaponSelect->active == 1U &&
           weaponSelect->targetMapId == ESP_POST_LOAD_SAVE_JUNCTION_TARGET_MAP &&
           weaponSelect->gameplayLoadMapId ==
               ESP_POST_LOAD_SAVE_JUNCTION_GAMEPLAY_MAP &&
           weaponSelect->loadType == 0U &&
           weaponSelect->weaponBefore == weaponSelect->requestedWeapon &&
           weaponSelect->requestedWeapon == weaponSelect->weaponAfter &&
           weaponSelect->weaponAfter <= 11U &&
           weaponSelect->viewInvalidationRequested == 0U;
}

static int playerViewCanonical(const EspPlayerViewState* playerView) {
    return playerView != NULL && playerView->active == 1U &&
           playerView->targetMapId == ESP_POST_LOAD_SAVE_JUNCTION_TARGET_MAP &&
           playerView->gameplayLoadMapId ==
               ESP_POST_LOAD_SAVE_JUNCTION_GAMEPLAY_MAP &&
           playerView->loadType == 0U && playerView->spawnApplied == 1U &&
           playerView->hudRefreshPending == 0U &&
           playerView->facingRefreshPending == 0U &&
           playerView->playerSetupPending == 0U &&
           playerView->tileEnterPending == 0U;
}

static int initialSaveJunctionWorldAtCallerBoundary(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapStateView* mapState = EspMapState_view();
    const EspMapAutomapStateView* automap = EspMapAutomapState_view();

    return runtime != NULL && mapState != NULL && automap != NULL &&
           runtime->sourceBytes == ESP_POST_LOAD_SAVE_JUNCTION_SOURCE_BYTES &&
           runtime->sourceCrc32 == ESP_POST_LOAD_SAVE_JUNCTION_SOURCE_CRC32 &&
           runtime->arenaFNV1a == ESP_POST_LOAD_SAVE_JUNCTION_RUNTIME_FNV &&
           mapState->stateFNV1a == ESP_POST_LOAD_SAVE_JUNCTION_MAP_FNV &&
           automap->stateFNV1a == ESP_POST_LOAD_SAVE_JUNCTION_AUTOMAP_FNV;
}

void EspPostLoadInitialSaveIntent_reset(void) {
    memset(&initialSaveIntentState, 0, sizeof(initialSaveIntentState));
}

int EspPostLoadInitialSaveIntent_isReady(void) {
    return initialSaveIntentState.active == 1U;
}

const EspPostLoadInitialSaveIntentState* EspPostLoadInitialSaveIntent_view(void) {
    return EspPostLoadInitialSaveIntent_isReady() ? &initialSaveIntentState : NULL;
}

EspPostLoadInitialSaveIntentStatus EspPostLoadInitialSaveIntent_prepare(
    const EspPostLoadWeaponSelectState* weaponSelect,
    const EspPlayerViewState* playerView,
    uint8_t isLoadedBefore,
    EspPostLoadInitialSaveIntentState* outState) {
    EspPostLoadInitialSaveIntentState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (weaponSelect == NULL || playerView == NULL || outState == NULL) {
        return ESP_POST_LOAD_INITIAL_SAVE_INTENT_INVALID;
    }
    if (!weaponSelectCanonical(weaponSelect)) {
        return ESP_POST_LOAD_INITIAL_SAVE_INTENT_WEAPON_INVALID;
    }
    if (!playerViewCanonical(playerView) ||
        playerView->targetMapId != weaponSelect->targetMapId ||
        playerView->gameplayLoadMapId != weaponSelect->gameplayLoadMapId ||
        playerView->loadType != weaponSelect->loadType) {
        return ESP_POST_LOAD_INITIAL_SAVE_INTENT_VIEW_INVALID;
    }
    if (isLoadedBefore > 1U) {
        return ESP_POST_LOAD_INITIAL_SAVE_INTENT_UNSUPPORTED_CONTEXT;
    }
    if (isLoadedBefore != 0U) {
        return ESP_POST_LOAD_INITIAL_SAVE_INTENT_LOADED_CONTEXT_DEFERRED;
    }
    if (EspPostLoadInitialSaveIntent_isReady()) {
        return ESP_POST_LOAD_INITIAL_SAVE_INTENT_ALREADY_ACTIVE;
    }
    if (!initialSaveJunctionWorldAtCallerBoundary()) {
        return ESP_POST_LOAD_INITIAL_SAVE_INTENT_WORLD_NOT_READY;
    }

    memset(&next, 0, sizeof(next));
    next.viewX = playerView->viewX;
    next.viewY = playerView->viewY;
    next.viewAngle = playerView->viewAngle;
    next.mapId = playerView->targetMapId;
    next.isLoadedBefore = isLoadedBefore;
    next.saveMode = 0U;
    next.saveRequired = 1U;
    next.componentMask = ESP_POST_LOAD_SAVE_COMPONENT_ALL;
    next.persistenceDeferred = 1U;
    next.presentationDeferred = 1U;
    next.active = 1U;
    *outState = next;
    return ESP_POST_LOAD_INITIAL_SAVE_INTENT_OK;
}

EspPostLoadInitialSaveIntentStatus EspPostLoadInitialSaveIntent_route(
    uint8_t isLoadedBefore) {
    EspPostLoadInitialSaveIntentState prepared;
    EspPostLoadInitialSaveIntentStatus status;

    if (EspPostLoadInitialSaveIntent_isReady()) {
        return ESP_POST_LOAD_INITIAL_SAVE_INTENT_ALREADY_ACTIVE;
    }

    status = EspPostLoadInitialSaveIntent_prepare(
        EspPostLoadWeaponSelect_view(), EspPlayerView_view(), isLoadedBefore,
        &prepared);
    if (status != ESP_POST_LOAD_INITIAL_SAVE_INTENT_OK) return status;

    initialSaveIntentState = prepared;
    return ESP_POST_LOAD_INITIAL_SAVE_INTENT_OK;
}


/* Consolidated from esp_post_load_playing_transition_state.c; public API intentionally unchanged. */
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

static int playingTransitionJunctionWorldAtCallerBoundary(void) {
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
    if (!playingTransitionJunctionWorldAtCallerBoundary()) {
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


/* Consolidated from esp_post_load_view_invalidation_state.c; public API intentionally unchanged. */
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

static int viewInvalidationJunctionWorldAtCallerBoundary(void) {
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
    if (!viewInvalidationJunctionWorldAtCallerBoundary()) {
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


/* Consolidated from esp_post_load_weapon_select_state.c; public API intentionally unchanged. */
#include <stddef.h>
#include <string.h>

#include "esp_map_automap_state.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"
#include "esp_post_load_weapon_select_state.h"

#define ESP_POST_LOAD_WEAPON_JUNCTION_TARGET_MAP 9U
#define ESP_POST_LOAD_WEAPON_JUNCTION_GAMEPLAY_MAP 2U
#define ESP_POST_LOAD_WEAPON_JUNCTION_SOURCE_BYTES 21051U
#define ESP_POST_LOAD_WEAPON_JUNCTION_SOURCE_CRC32 0x4a2c5800U
#define ESP_POST_LOAD_WEAPON_JUNCTION_RUNTIME_FNV 0xbc432a0fU
#define ESP_POST_LOAD_WEAPON_JUNCTION_MAP_FNV 0x8dba0bb4U
#define ESP_POST_LOAD_WEAPON_JUNCTION_AUTOMAP_FNV 0xb699bd75U

#define ESP_POST_LOAD_WEAPON_LINE_TARGETS 198U
#define ESP_POST_LOAD_WEAPON_SPRITE_TARGETS 48U
#define ESP_POST_LOAD_WEAPON_ENTRANCE_TARGETS 15U
#define ESP_POST_LOAD_WEAPON_MAX_INDEX 11U

static EspPostLoadWeaponSelectState postLoadWeaponSelectState;

static int giveMapCanonical(const EspPostLoadGiveMapState* giveMap) {
    return giveMap != NULL && giveMap->active == 1U &&
           giveMap->targetMapId == ESP_POST_LOAD_WEAPON_JUNCTION_TARGET_MAP &&
           giveMap->gameplayLoadMapId == ESP_POST_LOAD_WEAPON_JUNCTION_GAMEPLAY_MAP &&
           giveMap->loadType == 0U &&
           giveMap->lineTargetCount == ESP_POST_LOAD_WEAPON_LINE_TARGETS &&
           giveMap->spriteTargetCount == ESP_POST_LOAD_WEAPON_SPRITE_TARGETS &&
           giveMap->entranceTargetCount == ESP_POST_LOAD_WEAPON_ENTRANCE_TARGETS &&
           giveMap->linesMutated == ESP_POST_LOAD_WEAPON_LINE_TARGETS &&
           giveMap->spritesMutated == ESP_POST_LOAD_WEAPON_SPRITE_TARGETS &&
           giveMap->tilesMutated == ESP_POST_LOAD_WEAPON_ENTRANCE_TARGETS;
}

static int weaponSelectJunctionWorldAtCallerBoundary(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    const EspMapStateView* mapState = EspMapState_view();
    const EspMapAutomapStateView* automap = EspMapAutomapState_view();

    return runtime != NULL && mapState != NULL && automap != NULL &&
           runtime->sourceBytes == ESP_POST_LOAD_WEAPON_JUNCTION_SOURCE_BYTES &&
           runtime->sourceCrc32 == ESP_POST_LOAD_WEAPON_JUNCTION_SOURCE_CRC32 &&
           runtime->arenaFNV1a == ESP_POST_LOAD_WEAPON_JUNCTION_RUNTIME_FNV &&
           mapState->stateFNV1a == ESP_POST_LOAD_WEAPON_JUNCTION_MAP_FNV &&
           automap->stateFNV1a == ESP_POST_LOAD_WEAPON_JUNCTION_AUTOMAP_FNV;
}

void EspPostLoadWeaponSelect_reset(void) {
    memset(&postLoadWeaponSelectState, 0, sizeof(postLoadWeaponSelectState));
}

int EspPostLoadWeaponSelect_isReady(void) {
    return postLoadWeaponSelectState.active == 1U;
}

const EspPostLoadWeaponSelectState* EspPostLoadWeaponSelect_view(void) {
    return EspPostLoadWeaponSelect_isReady() ? &postLoadWeaponSelectState : NULL;
}

EspPostLoadWeaponSelectStatus EspPostLoadWeaponSelect_prepare(
    const EspPostLoadGiveMapState* giveMap,
    uint8_t currentWeapon,
    EspPostLoadWeaponSelectState* outState) {
    EspPostLoadWeaponSelectState next;

    if (outState != NULL) memset(outState, 0, sizeof(*outState));
    if (giveMap == NULL || outState == NULL) {
        return ESP_POST_LOAD_WEAPON_SELECT_INVALID;
    }
    if (!giveMapCanonical(giveMap)) {
        if (giveMap->active != 1U) {
            return ESP_POST_LOAD_WEAPON_SELECT_GIVEMAP_INVALID;
        }
        return ESP_POST_LOAD_WEAPON_SELECT_UNSUPPORTED_CONTEXT;
    }
    if (currentWeapon > ESP_POST_LOAD_WEAPON_MAX_INDEX) {
        return ESP_POST_LOAD_WEAPON_SELECT_WEAPON_INVALID;
    }
    if (EspPostLoadWeaponSelect_isReady()) {
        return ESP_POST_LOAD_WEAPON_SELECT_UNSUPPORTED_ORDER;
    }
    if (!weaponSelectJunctionWorldAtCallerBoundary()) {
        return ESP_POST_LOAD_WEAPON_SELECT_WORLD_NOT_READY;
    }

    memset(&next, 0, sizeof(next));
    next.weaponBefore = currentWeapon;
    next.requestedWeapon = currentWeapon;
    next.weaponAfter = currentWeapon;
    next.viewInvalidationRequested = 0U;
    next.targetMapId = giveMap->targetMapId;
    next.gameplayLoadMapId = giveMap->gameplayLoadMapId;
    next.loadType = giveMap->loadType;
    next.active = 1U;
    *outState = next;
    return ESP_POST_LOAD_WEAPON_SELECT_OK;
}

EspPostLoadWeaponSelectStatus EspPostLoadWeaponSelect_route(
    uint8_t currentWeapon) {
    EspPostLoadWeaponSelectState prepared;
    EspPostLoadWeaponSelectStatus status;

    if (EspPostLoadWeaponSelect_isReady()) {
        return ESP_POST_LOAD_WEAPON_SELECT_ALREADY_ACTIVE;
    }

    status = EspPostLoadWeaponSelect_prepare(
        EspPostLoadGiveMap_view(), currentWeapon, &prepared);
    if (status != ESP_POST_LOAD_WEAPON_SELECT_OK) return status;

    postLoadWeaponSelectState = prepared;
    return ESP_POST_LOAD_WEAPON_SELECT_OK;
}

