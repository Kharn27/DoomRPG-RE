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

static int junctionWorldAtCallerBoundary(void) {
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
    if (!junctionWorldAtCallerBoundary()) {
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
