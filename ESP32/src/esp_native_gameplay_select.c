#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_select.h"
#include "esp_player_view_state.h"

#define TILE_SIZE 64
#define TILE_CENTER 32
#define MAP_WIDTH 32
#define MAP_MAX_CENTER (((MAP_WIDTH - 1) * TILE_SIZE) + TILE_CENTER)

typedef char EspNativeGameplaySelectResult_must_be_28_bytes[
    sizeof(EspNativeGameplaySelectResult) == 28U ? 1 : -1];

static int centeredCoordinate(int32_t value) {
    return value >= TILE_CENTER && value <= MAP_MAX_CENTER &&
           (value & (TILE_SIZE - 1)) == TILE_CENTER;
}

static int tileIndexFor(int32_t x, int32_t y, uint16_t* outTile) {
    uint32_t tileX;
    uint32_t tileY;
    if (outTile == NULL || !centeredCoordinate(x) || !centeredCoordinate(y)) {
        return 0;
    }
    tileX = (uint32_t)x >> 6;
    tileY = (uint32_t)y >> 6;
    if (tileX >= MAP_WIDTH || tileY >= MAP_WIDTH) return 0;
    *outTile = (uint16_t)(tileY * MAP_WIDTH + tileX);
    return 1;
}

static int selectIntentValid(const EspNativeGameplayInputState* intent) {
    return intent != NULL && intent->active == 1U && intent->pending == 1U &&
           intent->sequence != 0U &&
           intent->action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT;
}

static int cardinalStepValid(const EspNativeGameplayTurnState* turn) {
    if (turn == NULL || turn->active != 1U) return 0;
    return (turn->viewStepX == TILE_SIZE && turn->viewStepY == 0) ||
           (turn->viewStepX == -TILE_SIZE && turn->viewStepY == 0) ||
           (turn->viewStepX == 0 && turn->viewStepY == TILE_SIZE) ||
           (turn->viewStepX == 0 && turn->viewStepY == -TILE_SIZE);
}

EspNativeGameplaySelectStatus EspNativeGameplaySelect_resolve(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplaySelectResult* outResult) {
    const EspPlayerViewState* view;
    const EspNativeGameplayTurnState* turn;
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    uint8_t currentState;
    int32_t frontX;
    int32_t frontY;
    uint16_t frontTile;

    if (outResult == NULL) return ESP_NATIVE_GAMEPLAY_SELECT_INVALID;
    memset(outResult, 0, sizeof(*outResult));
    outResult->eventIndex = ESP_NATIVE_GAMEPLAY_SELECT_NO_EVENT;

    if (!selectIntentValid(intent)) return ESP_NATIVE_GAMEPLAY_SELECT_INVALID;
    if (!EspMapRuntime_isLoaded() || !EspMapScriptState_isReady() ||
        !EspNativeGameplayDispatch_isReady()) {
        return ESP_NATIVE_GAMEPLAY_SELECT_NOT_READY;
    }

    view = EspPlayerView_view();
    turn = EspNativeGameplayDispatch_view();
    if (view == NULL || view->active != 1U || view->spawnApplied != 1U ||
        view->viewX != view->destX || view->viewY != view->destY ||
        view->viewAngle != view->destAngle ||
        !centeredCoordinate(view->destX) || !centeredCoordinate(view->destY) ||
        !cardinalStepValid(turn) || turn->destAngle != (uint8_t)view->destAngle) {
        return ESP_NATIVE_GAMEPLAY_SELECT_NOT_READY;
    }

    frontX = view->destX + turn->viewStepX;
    frontY = view->destY + turn->viewStepY;

    outResult->sequence = intent->sequence;
    outResult->inputFlags = ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS;
    outResult->frontX = frontX;
    outResult->frontY = frontY;

    if (!tileIndexFor(frontX, frontY, &frontTile)) {
        return ESP_NATIVE_GAMEPLAY_SELECT_OUT_OF_BOUNDS;
    }
    outResult->frontTile = frontTile;

    memset(&eventRef, 0, sizeof(eventRef));
    if (!EspMapEvents_findByTile(frontTile, &eventRef)) {
        return ESP_NATIVE_GAMEPLAY_SELECT_NO_TILE_EVENT;
    }
    if (!EspMapEvents_describe(&eventRef, &descriptor) ||
        !EspMapScriptState_getEventState(descriptor.eventIndex, &currentState)) {
        memset(outResult, 0, sizeof(*outResult));
        outResult->eventIndex = ESP_NATIVE_GAMEPLAY_SELECT_NO_EVENT;
        return ESP_NATIVE_GAMEPLAY_SELECT_INVALID;
    }

    outResult->eventIndex = descriptor.eventIndex;
    outResult->firstCommandIndex = descriptor.firstCommandIndex;
    outResult->commandEndIndex = descriptor.commandEndIndex;
    outResult->commandCount = descriptor.commandCount;
    outResult->currentState = currentState;
    outResult->eventFlags = descriptor.flags;
    outResult->eventFound = 1U;
    return ESP_NATIVE_GAMEPLAY_SELECT_TILE_EVENT;
}

const char* EspNativeGameplaySelect_statusName(
    EspNativeGameplaySelectStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_SELECT_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_SELECT_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_SELECT_OUT_OF_BOUNDS: return "OUT_OF_BOUNDS";
    case ESP_NATIVE_GAMEPLAY_SELECT_NO_TILE_EVENT: return "NO_TILE_EVENT";
    case ESP_NATIVE_GAMEPLAY_SELECT_TILE_EVENT: return "TILE_EVENT";
    default: return "UNKNOWN";
    }
}
