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

static EspPostLoadFlagCleanupState cleanupState;

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

static int junctionWorldAtCallerBoundary(void) {
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
    memset(&cleanupState, 0, sizeof(cleanupState));
}

int EspPostLoadFlagCleanup_isReady(void) {
    return cleanupState.active == 1U;
}

const EspPostLoadFlagCleanupState* EspPostLoadFlagCleanup_view(void) {
    return EspPostLoadFlagCleanup_isReady() ? &cleanupState : NULL;
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
    if (!junctionWorldAtCallerBoundary()) {
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

    cleanupState = prepared;
    return ESP_POST_LOAD_FLAG_CLEANUP_OK;
}
