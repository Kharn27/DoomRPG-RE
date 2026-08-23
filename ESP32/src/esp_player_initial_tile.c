#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_script_state.h"
#include "esp_player_fresh_map_state.h"
#include "esp_player_initial_tile.h"
#include "esp_player_view_state.h"

#define INITIAL_TILE_MAX_COMMANDS 63U
#define INITIAL_TILE_MAP_WIDTH 32U
#define INITIAL_TILE_WORLD_SIZE (INITIAL_TILE_MAP_WIDTH * 64U)
#define INITIAL_TILE_REMOVE_FLAG 0x00000200UL

static EspPlayerInitialTileState initialTileState;

static void zeroDiagnostics(uint8_t* codeId, uint8_t* commandOffset) {
    if (codeId != NULL) *codeId = 0U;
    if (commandOffset != NULL) *commandOffset = 0U;
}

static int ownersMatch(const EspPlayerViewState* playerView,
                       const EspPlayerFreshMapState* freshMap) {
    return playerView != NULL && freshMap != NULL &&
           playerView->targetMapId == freshMap->targetMapId &&
           playerView->gameplayLoadMapId == freshMap->gameplayLoadMapId &&
           playerView->loadType == freshMap->loadType;
}

static int viewOrderReady(const EspPlayerViewState* view) {
    return view != NULL && view->active == 1U && view->spawnApplied == 1U &&
           view->hudRefreshPending == 0U &&
           view->facingRefreshPending == 1U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 1U;
}

static int freshMapReady(const EspPlayerFreshMapState* freshMap) {
    return freshMap != NULL && freshMap->active == 1U &&
           freshMap->setupApplied == 1U && freshMap->loadType == 0U;
}

static int coordinatesReady(const EspPlayerViewState* view,
                            uint16_t* outTileIndex) {
    uint32_t tileX;
    uint32_t tileY;

    if (view == NULL || outTileIndex == NULL || view->viewX != view->destX ||
        view->viewY != view->destY || view->viewAngle != view->destAngle ||
        view->destX < 0 || view->destY < 0 ||
        (uint32_t)view->destX >= INITIAL_TILE_WORLD_SIZE ||
        (uint32_t)view->destY >= INITIAL_TILE_WORLD_SIZE) {
        return 0;
    }

    tileX = (uint32_t)view->destX >> 6U;
    tileY = (uint32_t)view->destY >> 6U;
    if (tileX >= INITIAL_TILE_MAP_WIDTH || tileY >= INITIAL_TILE_MAP_WIDTH) {
        return 0;
    }

    *outTileIndex = (uint16_t)(tileY * INITIAL_TILE_MAP_WIDTH + tileX);
    return 1;
}

void EspPlayerInitialTile_reset(void) {
    memset(&initialTileState, 0, sizeof(initialTileState));
}

int EspPlayerInitialTile_isReady(void) {
    return initialTileState.active == 1U;
}

const EspPlayerInitialTileState* EspPlayerInitialTile_view(void) {
    return EspPlayerInitialTile_isReady() ? &initialTileState : NULL;
}

EspPlayerInitialTileStatus EspPlayerInitialTile_prepare(
    const EspPlayerViewState* playerView,
    const EspPlayerFreshMapState* freshMap,
    uint32_t playerKeys,
    uint8_t executionBlocked,
    EspPlayerInitialTileState* outState,
    uint8_t* outDeferredCodeId,
    uint8_t* outDeferredCommandOffset) {
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan filterPlan;
    EspMapEventCommandFilterResult filterResult;
    uint16_t tileIndex;
    uint8_t eventState;
    uint8_t removed;
    uint32_t commandOffset;

    zeroDiagnostics(outDeferredCodeId, outDeferredCommandOffset);
    if (outState != NULL) memset(outState, 0, sizeof(*outState));

    if (playerView == NULL || freshMap == NULL || outState == NULL) {
        return ESP_PLAYER_INITIAL_TILE_INVALID;
    }
    if (playerView->active != 1U || playerView->spawnApplied != 1U) {
        return ESP_PLAYER_INITIAL_TILE_VIEW_INVALID;
    }
    if (!freshMapReady(freshMap) || !ownersMatch(playerView, freshMap)) {
        return ESP_PLAYER_INITIAL_TILE_SETUP_INVALID;
    }
    if (executionBlocked > 1U || executionBlocked != 0U ||
        playerView->loadType != 0U || playerView->destAngle != 64) {
        return ESP_PLAYER_INITIAL_TILE_UNSUPPORTED_CONTEXT;
    }
    if (!viewOrderReady(playerView)) {
        return ESP_PLAYER_INITIAL_TILE_UNSUPPORTED_ORDER;
    }
    if (!coordinatesReady(playerView, &tileIndex) ||
        !EspMapScriptState_isReady()) {
        return ESP_PLAYER_INITIAL_TILE_EVENT_INVALID;
    }

    outState->inputFlags =
        ESP_PLAYER_INITIAL_TILE_BASE_FLAGS |
        ESP_PLAYER_INITIAL_TILE_FACING_64_FLAG;
    outState->tileIndex = tileIndex;
    outState->eventIndex = ESP_PLAYER_INITIAL_TILE_NO_EVENT;
    outState->targetMapId = playerView->targetMapId;
    outState->gameplayLoadMapId = playerView->gameplayLoadMapId;
    outState->loadType = playerView->loadType;
    outState->skipAdvanceTurn = 0U;

    if (!EspMapEvents_findByTile(tileIndex, &eventRef)) {
        outState->active = 1U;
        return ESP_PLAYER_INITIAL_TILE_OK;
    }
    if (!EspMapEvents_describe(&eventRef, &descriptor) ||
        descriptor.commandCount > INITIAL_TILE_MAX_COMMANDS ||
        !EspMapScriptState_getEventState(eventRef.index, &eventState) ||
        !EspMapEventFilter_prepare(&descriptor, eventState, 0U,
                                   outState->inputFlags, playerKeys,
                                   &filterPlan)) {
        memset(outState, 0, sizeof(*outState));
        return ESP_PLAYER_INITIAL_TILE_EVENT_INVALID;
    }

    outState->eventFound = 1U;
    outState->eventIndex = eventRef.index;
    outState->eventState = eventState;
    outState->eventFlags = descriptor.flags;
    outState->eventBlocked = filterPlan.eventBlocked;

    for (commandOffset = 0U; commandOffset < descriptor.commandCount;
         ++commandOffset) {
        if (!EspMapScriptState_isCommandRemoved(
                (uint32_t)descriptor.firstCommandIndex + commandOffset,
                &removed) ||
            !EspMapEventFilter_evaluate(&descriptor, &filterPlan,
                                        commandOffset, removed,
                                        &filterResult)) {
            memset(outState, 0, sizeof(*outState));
            return ESP_PLAYER_INITIAL_TILE_EVENT_INVALID;
        }

        if (filterResult.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
        if (outState->eligibleCommands == 0xffU) {
            memset(outState, 0, sizeof(*outState));
            return ESP_PLAYER_INITIAL_TILE_EVENT_INVALID;
        }
        ++outState->eligibleCommands;

        if (!EspMapOpcodeExecutor_supports(filterResult.codeId)) {
            if (outDeferredCodeId != NULL) {
                *outDeferredCodeId = filterResult.codeId;
            }
            if (outDeferredCommandOffset != NULL) {
                *outDeferredCommandOffset = filterResult.commandOffset;
            }
            memset(outState, 0, sizeof(*outState));
            return ESP_PLAYER_INITIAL_TILE_OPCODE_DEFERRED;
        }
    }

    outState->active = 1U;
    return ESP_PLAYER_INITIAL_TILE_OK;
}

static void rollbackScript(const uint16_t* mutatedEvents,
                           const uint8_t* priorStates,
                           uint32_t mutationCount,
                           const uint16_t* removedCommands,
                           uint32_t removedCount) {
    while (removedCount != 0U) {
        --removedCount;
        (void)EspMapScriptState_setCommandRemoved(
            removedCommands[removedCount], 0U);
    }
    while (mutationCount != 0U) {
        --mutationCount;
        (void)EspMapScriptState_setEventState(
            mutatedEvents[mutationCount], priorStates[mutationCount]);
    }
}

EspPlayerInitialTileStatus EspPlayerInitialTile_route(
    uint32_t playerKeys,
    uint8_t executionBlocked,
    uint8_t* outDeferredCodeId,
    uint8_t* outDeferredCommandOffset) {
    const EspPlayerViewState* playerView;
    const EspPlayerFreshMapState* freshMap;
    EspPlayerInitialTileState prepared;
    EspPlayerInitialTileStatus status;
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan filterPlan;
    EspMapEventCommandFilterResult filterResult;
    EspMapByteCode command;
    EspMapOpcodeExecResult execResult;
    uint16_t mutatedEvents[INITIAL_TILE_MAX_COMMANDS];
    uint8_t priorStates[INITIAL_TILE_MAX_COMMANDS];
    uint16_t removedCommands[INITIAL_TILE_MAX_COMMANDS];
    uint32_t mutationCount = 0U;
    uint32_t removedCount = 0U;
    uint32_t commandOffset;
    uint8_t removed;
    uint8_t initialEventState;

    zeroDiagnostics(outDeferredCodeId, outDeferredCommandOffset);
    if (EspPlayerInitialTile_isReady()) {
        return ESP_PLAYER_INITIAL_TILE_ALREADY_ACTIVE;
    }

    playerView = EspPlayerView_view();
    freshMap = EspPlayerFreshMap_view();
    status = EspPlayerInitialTile_prepare(
        playerView, freshMap, playerKeys, executionBlocked, &prepared,
        outDeferredCodeId, outDeferredCommandOffset);
    if (status != ESP_PLAYER_INITIAL_TILE_OK) return status;

    if (prepared.eventFound != 0U && prepared.eventBlocked == 0U) {
        if (!EspMapEvents_findByTile(prepared.tileIndex, &eventRef) ||
            eventRef.index != prepared.eventIndex ||
            !EspMapEvents_describe(&eventRef, &descriptor) ||
            !EspMapScriptState_getEventState(eventRef.index,
                                             &initialEventState) ||
            initialEventState != prepared.eventState ||
            !EspMapEventFilter_prepare(&descriptor, initialEventState, 0U,
                                       prepared.inputFlags, playerKeys,
                                       &filterPlan)) {
            return ESP_PLAYER_INITIAL_TILE_EVENT_INVALID;
        }

        for (commandOffset = 0U; commandOffset < descriptor.commandCount;
             ++commandOffset) {
            if (!EspMapScriptState_isCommandRemoved(
                    (uint32_t)descriptor.firstCommandIndex + commandOffset,
                    &removed) ||
                !EspMapEventFilter_evaluate(&descriptor, &filterPlan,
                                            commandOffset, removed,
                                            &filterResult)) {
                rollbackScript(mutatedEvents, priorStates, mutationCount,
                               removedCommands, removedCount);
                return ESP_PLAYER_INITIAL_TILE_EXEC_FAILED;
            }
            if (filterResult.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) {
                continue;
            }
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command) ||
                !EspMapOpcodeExecutor_supports(command.id)) {
                rollbackScript(mutatedEvents, priorStates, mutationCount,
                               removedCommands, removedCount);
                return ESP_PLAYER_INITIAL_TILE_EXEC_FAILED;
            }

            if (EspMapOpcodeExecutor_execute(&command, &execResult) !=
                ESP_MAP_OPCODE_EXEC_OK) {
                rollbackScript(mutatedEvents, priorStates, mutationCount,
                               removedCommands, removedCount);
                return ESP_PLAYER_INITIAL_TILE_EXEC_FAILED;
            }

            if (execResult.mutated != 0U) {
                if (mutationCount >= INITIAL_TILE_MAX_COMMANDS) {
                    rollbackScript(mutatedEvents, priorStates, mutationCount,
                                   removedCommands, removedCount);
                    return ESP_PLAYER_INITIAL_TILE_EXEC_FAILED;
                }
                mutatedEvents[mutationCount] = execResult.targetEventIndex;
                priorStates[mutationCount] = execResult.stateBefore;
                ++mutationCount;
            }

            if ((command.arg2 & INITIAL_TILE_REMOVE_FLAG) != 0U) {
                if (removedCount >= INITIAL_TILE_MAX_COMMANDS ||
                    !EspMapScriptState_setCommandRemoved(
                        filterResult.globalCommandIndex, 1U)) {
                    rollbackScript(mutatedEvents, priorStates, mutationCount,
                                   removedCommands, removedCount);
                    return ESP_PLAYER_INITIAL_TILE_EXEC_FAILED;
                }
                removedCommands[removedCount++] =
                    filterResult.globalCommandIndex;
            }
            ++prepared.executedCommands;
        }
    }

    prepared.removedCommands = (uint8_t)removedCount;
    if (!EspPlayerView_consumeTileEnter(prepared.targetMapId,
                                        prepared.gameplayLoadMapId,
                                        prepared.loadType)) {
        rollbackScript(mutatedEvents, priorStates, mutationCount,
                       removedCommands, removedCount);
        return ESP_PLAYER_INITIAL_TILE_VIEW_CONSUME_FAILED;
    }

    initialTileState = prepared;
    return ESP_PLAYER_INITIAL_TILE_OK;
}
