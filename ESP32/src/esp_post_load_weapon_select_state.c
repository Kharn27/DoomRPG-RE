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

static int junctionWorldAtCallerBoundary(void) {
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
    if (!junctionWorldAtCallerBoundary()) {
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
