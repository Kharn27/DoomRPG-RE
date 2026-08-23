#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_script_state.h"
#include "esp_player_finish_rotation_tile.h"

#define FINISH_ROTATION_TILE_MAX_COMMANDS 63U
#define FINISH_ROTATION_TILE_MAP_WIDTH 32U
#define FINISH_ROTATION_TILE_WORLD_SIZE (FINISH_ROTATION_TILE_MAP_WIDTH * 64U)
#define FINISH_ROTATION_TILE_REMOVE_FLAG 0x00000200UL

static EspPlayerFinishRotationTileState finishRotationTileState;

static void zeroDiagnostics(uint8_t* codeId, uint8_t* commandOffset) {
    if (codeId != NULL) *codeId = 0U;
    if (commandOffset != NULL) *commandOffset = 0U;
}

static int viewReady(const EspPlayerViewState* view) {
    return view != NULL && view->active == 1U && view->spawnApplied == 1U &&
           view->loadType == 0U && view->hudRefreshPending == 0U &&
           view->playerSetupPending == 0U && view->tileEnterPending == 0U &&
           view->facingRefreshPending == 1U;
}

static int initialMatchesView(const EspPlayerInitialTileState* tile,
                              const EspPlayerViewState* view) {
    return tile != NULL && view != NULL && tile->active == 1U &&
           tile->targetMapId == view->targetMapId &&
           tile->gameplayLoadMapId == view->gameplayLoadMapId &&
           tile->loadType == view->loadType;
}

static int orientationMatchesView(const EspPlayerOrientationState* orientation,
                                  const EspPlayerViewState* view) {
    return orientation != NULL && view != NULL && orientation->active == 1U &&
           orientation->prepared == 1U &&
           orientation->targetMapId == view->targetMapId &&
           orientation->gameplayLoadMapId == view->gameplayLoadMapId &&
           orientation->loadType == view->loadType &&
           orientation->destAngle == (uint8_t)view->destAngle;
}

static int orientationValuesReady(const EspPlayerOrientationState* orientation) {
    return orientation != NULL && orientation->destAngle == 64U &&
           orientation->viewSin == 65536 && orientation->viewCos == 0 &&
           orientation->viewStepX == 0 && orientation->viewStepY == -64;
}

static int coordinatesReady(const EspPlayerViewState* view,
                            uint16_t* outTileIndex) {
    uint32_t tileX;
    uint32_t tileY;

    if (view == NULL || outTileIndex == NULL || view->viewX != view->destX ||
        view->viewY != view->destY || view->viewAngle != view->destAngle ||
        view->destX < 0 || view->destY < 0 ||
        (uint32_t)view->destX >= FINISH_ROTATION_TILE_WORLD_SIZE ||
        (uint32_t)view->destY >= FINISH_ROTATION_TILE_WORLD_SIZE) {
        return 0;
    }

    tileX = (uint32_t)view->destX >> 6U;
    tileY = (uint32_t)view->destY >> 6U;
    if (tileX >= FINISH_ROTATION_TILE_MAP_WIDTH ||
        tileY >= FINISH_ROTATION_TILE_MAP_WIDTH) {
        return 0;
    }

    *outTileIndex =
        (uint16_t)(tileY * FINISH_ROTATION_TILE_MAP_WIDTH + tileX);
    return 1;
}

void EspPlayerFinishRotationTile_reset(void) {
    memset(&finishRotationTileState, 0, sizeof(finishRotationTileState));
}

int EspPlayerFinishRotationTile_isReady(void) {
    return finishRotationTileState.active == 1U;
}

const EspPlayerFinishRotationTileState* EspPlayerFinishRotationTile_view(void) {
    return EspPlayerFinishRotationTile_isReady() ? &finishRotationTileState : NULL;
}

EspPlayerFinishRotationTileStatus EspPlayerFinishRotationTile_prepare(
    const EspPlayerViewState* playerView,
    const EspPlayerInitialTileState* initialTile,
    const EspPlayerOrientationState* orientation,
    uint32_t playerKeys,
    uint8_t executionBlocked,
    EspPlayerFinishRotationTileState* outState,
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

    if (playerView == NULL || initialTile == NULL || orientation == NULL ||
        outState == NULL) {
        return ESP_PLAYER_FINISH_ROTATION_TILE_INVALID;
    }
    if (playerView->active != 1U || playerView->spawnApplied != 1U) {
        return ESP_PLAYER_FINISH_ROTATION_TILE_VIEW_INVALID;
    }
    if (initialTile->active != 1U || !initialMatchesView(initialTile, playerView)) {
        return ESP_PLAYER_FINISH_ROTATION_TILE_INITIAL_INVALID;
    }
    if (orientation->active != 1U || orientation->prepared != 1U ||
        !orientationMatchesView(orientation, playerView)) {
        return ESP_PLAYER_FINISH_ROTATION_TILE_ORIENTATION_INVALID;
    }
    if (executionBlocked > 1U || executionBlocked != 0U ||
        playerView->loadType != 0U || playerView->destAngle != 64 ||
        playerView->viewAngle != playerView->destAngle ||
        !orientationValuesReady(orientation)) {
        return ESP_PLAYER_FINISH_ROTATION_TILE_UNSUPPORTED_CONTEXT;
    }
    if (!viewReady(playerView)) {
        return ESP_PLAYER_FINISH_ROTATION_TILE_UNSUPPORTED_ORDER;
    }
    if (!coordinatesReady(playerView, &tileIndex) ||
        !EspMapScriptState_isReady()) {
        return ESP_PLAYER_FINISH_ROTATION_TILE_EVENT_INVALID;
    }

    outState->inputFlags = ESP_PLAYER_FINISH_ROTATION_TILE_FLAGS;
    outState->tileIndex = tileIndex;
    outState->eventIndex = ESP_PLAYER_FINISH_ROTATION_TILE_NO_EVENT;
    outState->targetMapId = playerView->targetMapId;
    outState->gameplayLoadMapId = playerView->gameplayLoadMapId;
    outState->loadType = playerView->loadType;
    outState->skipAdvanceTurn = 0U;

    if (!EspMapEvents_findByTile(tileIndex, &eventRef)) {
        outState->active = 1U;
        return ESP_PLAYER_FINISH_ROTATION_TILE_OK;
    }
    if (!EspMapEvents_describe(&eventRef, &descriptor) ||
        descriptor.commandCount > FINISH_ROTATION_TILE_MAX_COMMANDS ||
        !EspMapScriptState_getEventState(eventRef.index, &eventState) ||
        !EspMapEventFilter_prepare(&descriptor, eventState, 0U,
                                   outState->inputFlags, playerKeys,
                                   &filterPlan)) {
        memset(outState, 0, sizeof(*outState));
        return ESP_PLAYER_FINISH_ROTATION_TILE_EVENT_INVALID;
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
            return ESP_PLAYER_FINISH_ROTATION_TILE_EVENT_INVALID;
        }

        if (filterResult.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
        if (outState->eligibleCommands == 0xffU) {
            memset(outState, 0, sizeof(*outState));
            return ESP_PLAYER_FINISH_ROTATION_TILE_EVENT_INVALID;
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
            return ESP_PLAYER_FINISH_ROTATION_TILE_OPCODE_DEFERRED;
        }
    }

    outState->active = 1U;
    return ESP_PLAYER_FINISH_ROTATION_TILE_OK;
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

EspPlayerFinishRotationTileStatus EspPlayerFinishRotationTile_route(
    uint32_t playerKeys,
    uint8_t executionBlocked,
    uint8_t* outDeferredCodeId,
    uint8_t* outDeferredCommandOffset) {
    const EspPlayerViewState* playerView;
    const EspPlayerInitialTileState* initialTile;
    const EspPlayerOrientationState* orientation;
    EspPlayerFinishRotationTileState prepared;
    EspPlayerFinishRotationTileStatus status;
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan filterPlan;
    EspMapEventCommandFilterResult filterResult;
    EspMapByteCode command;
    EspMapOpcodeExecResult execResult;
    uint16_t mutatedEvents[FINISH_ROTATION_TILE_MAX_COMMANDS];
    uint8_t priorStates[FINISH_ROTATION_TILE_MAX_COMMANDS];
    uint16_t removedCommands[FINISH_ROTATION_TILE_MAX_COMMANDS];
    uint32_t mutationCount = 0U;
    uint32_t removedCount = 0U;
    uint32_t commandOffset;
    uint8_t removed;
    uint8_t initialEventState;

    zeroDiagnostics(outDeferredCodeId, outDeferredCommandOffset);
    if (EspPlayerFinishRotationTile_isReady()) {
        return ESP_PLAYER_FINISH_ROTATION_TILE_ALREADY_ACTIVE;
    }

    playerView = EspPlayerView_view();
    initialTile = EspPlayerInitialTile_view();
    orientation = EspPlayerOrientation_view();
    status = EspPlayerFinishRotationTile_prepare(
        playerView, initialTile, orientation, playerKeys, executionBlocked,
        &prepared, outDeferredCodeId, outDeferredCommandOffset);
    if (status != ESP_PLAYER_FINISH_ROTATION_TILE_OK) return status;

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
            return ESP_PLAYER_FINISH_ROTATION_TILE_EVENT_INVALID;
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
                return ESP_PLAYER_FINISH_ROTATION_TILE_EXEC_FAILED;
            }
            if (filterResult.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) {
                continue;
            }
            if (!EspMapEvents_getCommand(&descriptor, commandOffset, &command) ||
                !EspMapOpcodeExecutor_supports(command.id)) {
                rollbackScript(mutatedEvents, priorStates, mutationCount,
                               removedCommands, removedCount);
                return ESP_PLAYER_FINISH_ROTATION_TILE_EXEC_FAILED;
            }

            if (EspMapOpcodeExecutor_execute(&command, &execResult) !=
                ESP_MAP_OPCODE_EXEC_OK) {
                rollbackScript(mutatedEvents, priorStates, mutationCount,
                               removedCommands, removedCount);
                return ESP_PLAYER_FINISH_ROTATION_TILE_EXEC_FAILED;
            }

            if (execResult.mutated != 0U) {
                if (mutationCount >= FINISH_ROTATION_TILE_MAX_COMMANDS) {
                    rollbackScript(mutatedEvents, priorStates, mutationCount,
                                   removedCommands, removedCount);
                    return ESP_PLAYER_FINISH_ROTATION_TILE_EXEC_FAILED;
                }
                mutatedEvents[mutationCount] = execResult.targetEventIndex;
                priorStates[mutationCount] = execResult.stateBefore;
                ++mutationCount;
            }

            if ((command.arg2 & FINISH_ROTATION_TILE_REMOVE_FLAG) != 0U) {
                if (removedCount >= FINISH_ROTATION_TILE_MAX_COMMANDS ||
                    !EspMapScriptState_setCommandRemoved(
                        filterResult.globalCommandIndex, 1U)) {
                    rollbackScript(mutatedEvents, priorStates, mutationCount,
                                   removedCommands, removedCount);
                    return ESP_PLAYER_FINISH_ROTATION_TILE_EXEC_FAILED;
                }
                removedCommands[removedCount++] =
                    filterResult.globalCommandIndex;
            }
            ++prepared.executedCommands;
        }
    }

    prepared.removedCommands = (uint8_t)removedCount;
    finishRotationTileState = prepared;
    return ESP_PLAYER_FINISH_ROTATION_TILE_OK;
}
