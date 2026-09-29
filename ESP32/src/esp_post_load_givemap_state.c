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

static int junctionWorldAtCallerBoundary(void) {
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
    if (!junctionWorldAtCallerBoundary()) {
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
