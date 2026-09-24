#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_line_state.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_ui_intent.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_move_events.h"
#include "esp_native_gameplay_status_message.h"

#define MOVE_BLOCK_INPUT_FLAG 0x00000400UL
#define MOVE_EXIT_POS_X       0x00000020UL
#define MOVE_ENTER_POS_X      0x00000008UL
#define MOVE_EXIT_NEG_X       0x00000080UL
#define MOVE_ENTER_NEG_X      0x00000002UL
#define MOVE_EXIT_NEG_Y       0x00000010UL
#define MOVE_ENTER_NEG_Y      0x00000004UL
#define MOVE_EXIT_POS_Y       0x00000040UL
#define MOVE_ENTER_POS_Y      0x00000001UL
#define FACING_NORTH_FLAG     0x10000000UL
#define FACING_EAST_FLAG      0x20000000UL
#define FACING_SOUTH_FLAG     0x40000000UL
#define FACING_WEST_FLAG      0x80000000UL

typedef struct MoveShowStep_s {
    EspMapShowResult show;
    uint8_t removedBefore;
    uint8_t removedAfter;
    uint8_t reserved[2];
} MoveShowStep;

typedef struct MoveShowBatchOwner_s {
    MoveShowStep step[ESP_NATIVE_GAMEPLAY_MOVE_SHOW_BATCH_MAX];
    uint16_t eventIndex;
    uint16_t firstGlobal;
    uint8_t count;
    uint8_t committedCount;
    uint8_t previewValid;
    uint8_t active;
} MoveShowBatchOwner;

typedef struct MoveEventTransaction_s {
    uint32_t sequence;
    EspNativeGameplayMoveEventResult exitResult;
    EspNativeGameplayMoveEventResult enterResult;
    uint8_t exitRollback;
    uint8_t enterRollback;
    uint8_t dialogPending;
    uint8_t worldRendered;
    uint8_t active;
    uint8_t reserved[3];
} MoveEventTransaction;

static MoveEventTransaction transaction;
static MoveShowBatchOwner showBatchOwner;
static uint8_t showBatchOwnerLogged;

EspNativeGameplayDispatchStatus __real_EspNativeGameplayDispatch_commitMove(
    const EspPlayerViewState* expectedBeforeView,
    const EspPlayerViewState* preparedAfterView,
    EspNativeGameplayMoveResult* ioResult);
EspNativeGameplayDispatchStatus __real_EspNativeGameplayDispatch_rollbackMove(
    const EspPlayerViewState* expectedAfterView,
    const EspPlayerViewState* restoreBeforeView,
    EspNativeGameplayMoveResult* ioResult);

static int movementFlags(const EspNativeGameplayMoveResult* move,
                         const EspPlayerViewState* afterView,
                         uint32_t* outExitFlags,
                         uint32_t* outEnterFlags) {
    uint32_t exitFlag;
    uint32_t enterFlag;
    uint32_t facingFlag;
    uint8_t angle;

    if (move == NULL || afterView == NULL || outExitFlags == NULL ||
        outEnterFlags == NULL) {
        return 0;
    }

    if (move->deltaX == 64 && move->deltaY == 0) {
        exitFlag = MOVE_EXIT_POS_X;
        enterFlag = MOVE_ENTER_POS_X;
    }
    else if (move->deltaX == -64 && move->deltaY == 0) {
        exitFlag = MOVE_EXIT_NEG_X;
        enterFlag = MOVE_ENTER_NEG_X;
    }
    else if (move->deltaX == 0 && move->deltaY == -64) {
        exitFlag = MOVE_EXIT_NEG_Y;
        enterFlag = MOVE_ENTER_NEG_Y;
    }
    else if (move->deltaX == 0 && move->deltaY == 64) {
        exitFlag = MOVE_EXIT_POS_Y;
        enterFlag = MOVE_ENTER_POS_Y;
    }
    else {
        return 0;
    }

    if (afterView->viewAngle < 0 || afterView->viewAngle > 255 ||
        afterView->viewAngle != afterView->destAngle ||
        (afterView->viewAngle & 63) != 0) {
        return 0;
    }
    angle = (uint8_t)afterView->viewAngle;
    switch (angle) {
    case 64U: facingFlag = FACING_NORTH_FLAG; break;
    case 0U: facingFlag = FACING_EAST_FLAG; break;
    case 192U: facingFlag = FACING_SOUTH_FLAG; break;
    case 128U: facingFlag = FACING_WEST_FLAG; break;
    default: return 0;
    }

    *outExitFlags = exitFlag | MOVE_BLOCK_INPUT_FLAG;
    *outEnterFlags = enterFlag | facingFlag | MOVE_BLOCK_INPUT_FLAG;
    return 1;
}

static int phaseUnsafe(EspNativeGameplayMoveEventStatus status) {
    return status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID ||
           status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY ||
           status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED ||
           status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
}

static int phaseHandled(EspNativeGameplayMoveEventStatus status) {
    return status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK ||
           status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_FORCE_MESSAGE_OK ||
           status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SCRIPT_STATE_OK ||
           status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SHOW_OK;
}

static int isDialogCode(uint8_t codeId) {
    return codeId == ESP_MAP_OPCODE_DIALOG ||
           codeId == ESP_MAP_OPCODE_DIALOG_NO_BACK;
}

static int isStateCode(uint8_t codeId) {
    return EspMapOpcodeExecutor_supports(codeId) != 0;
}

static void logComplexSequence(const EspMapEventDescriptor* descriptor,
                               const EspMapEventFilterPlan* plan,
                               uint16_t tile,
                               uint32_t runFlags) {
    uint32_t offset;
    uint32_t limit;

    if (descriptor == NULL || plan == NULL) return;
    limit = descriptor->commandCount;
    if (limit > 16U) limit = 16U;

    printf("[MOVEEVENTTRACE] COMPLEX event=%u tile=%u flags=%08x commands=%u dump=%u raw-sequence\n",
           (unsigned int)descriptor->eventIndex,
           (unsigned int)tile,
           (unsigned int)runFlags,
           (unsigned int)descriptor->commandCount,
           (unsigned int)limit);

    for (offset = 0U; offset < limit; ++offset) {
        EspMapByteCode command;
        EspMapEventCommandFilterResult filtered;
        uint32_t global = (uint32_t)descriptor->firstCommandIndex + offset;
        uint8_t removed = 0U;
        int commandOk;
        int removedOk;
        int filterOk;

        memset(&command, 0, sizeof(command));
        memset(&filtered, 0, sizeof(filtered));
        commandOk = EspMapEvents_getCommand(descriptor, offset, &command);
        removedOk = global <= UINT16_MAX &&
                    EspMapScriptState_isCommandRemoved(global, &removed);
        filterOk = commandOk && removedOk &&
                   EspMapEventFilter_evaluate(descriptor,
                                              plan,
                                              offset,
                                              removed,
                                              &filtered);
        printf("[MOVEEVENTTRACE] off=%u global=%u id=%u a1=%08x a2=%08x removed=%u decision=%u eval=%s\n",
               (unsigned int)offset,
               (unsigned int)global,
               (unsigned int)command.id,
               (unsigned int)command.arg1,
               (unsigned int)command.arg2,
               (unsigned int)removed,
               (unsigned int)(filterOk ? filtered.decision : 255U),
               filterOk ? "ok" : "fail");
    }
    if ((uint32_t)descriptor->commandCount > limit) {
        printf("[MOVEEVENTTRACE] truncated remaining=%u mutation=no\n",
               (unsigned int)((uint32_t)descriptor->commandCount - limit));
    }
}

static void clearShowBatchOwner(void) {
    memset(&showBatchOwner, 0, sizeof(showBatchOwner));
}

static void releaseShowBatchOwnerForTransaction(const char* reason) {
    if ((transaction.exitResult.codeId == ESP_MAP_OPCODE_SHOW ||
         transaction.enterResult.codeId == ESP_MAP_OPCODE_SHOW) &&
        showBatchOwner.active != 0U) {
        printf("[MOVEEVENT] SHOW-LEASE RELEASE seq=%u reason=%s event=%u count=%u active=1->0 dialogPending=%u worldRendered=%u\n",
               (unsigned int)transaction.sequence,
               reason != NULL ? reason : "unknown",
               (unsigned int)showBatchOwner.eventIndex,
               (unsigned int)showBatchOwner.count,
               (unsigned int)transaction.dialogPending,
               (unsigned int)transaction.worldRendered);
        clearShowBatchOwner();
    }
}

static int rollbackShowBatchPrefix(uint8_t count) {
    uint8_t removedNow;
    int i;

    if (count == 0U ||
        count > showBatchOwner.count ||
        count > ESP_NATIVE_GAMEPLAY_MOVE_SHOW_BATCH_MAX) {
        return 0;
    }

    for (i = 0; i < (int)count; ++i) {
        const MoveShowStep* step = &showBatchOwner.step[i];
        if (!EspMapScriptState_isCommandRemoved(
                step->show.globalCommandIndex, &removedNow) ||
            removedNow != step->removedAfter) {
            return 0;
        }
    }

    for (i = (int)count - 1; i >= 0; --i) {
        const MoveShowStep* step = &showBatchOwner.step[i];
        if (step->removedBefore != step->removedAfter &&
            !EspMapScriptState_setCommandRemoved(
                step->show.globalCommandIndex, step->removedBefore)) {
            return 0;
        }
        if (!EspMapSpriteTopology_rollbackShow(&step->show)) {
            if (step->removedBefore != step->removedAfter) {
                (void)EspMapScriptState_setCommandRemoved(
                    step->show.globalCommandIndex, step->removedAfter);
            }
            return 0;
        }
    }

    for (i = 0; i < (int)count; ++i) {
        const MoveShowStep* step = &showBatchOwner.step[i];
        if (!EspMapScriptState_isCommandRemoved(
                step->show.globalCommandIndex, &removedNow) ||
            removedNow != step->removedBefore) {
            return 0;
        }
    }
    return 1;
}

static EspNativeGameplayMoveEventStatus preflightShowBatch(
    const EspMapEventDescriptor* descriptor,
    const uint8_t* offsets,
    const uint8_t* removedBefore,
    uint8_t count) {
    uint8_t i;

    if (descriptor == NULL || offsets == NULL || removedBefore == NULL ||
        count == 0U || count > ESP_NATIVE_GAMEPLAY_MOVE_SHOW_BATCH_MAX ||
        !EspMapSpriteTopology_isReady()) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY;
    }
    if (showBatchOwner.active != 0U) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY;
    }

    clearShowBatchOwner();
    showBatchOwner.eventIndex = descriptor->eventIndex;
    showBatchOwner.count = count;
    for (i = 0U; i < count; ++i) {
        MoveShowStep* step = &showBatchOwner.step[i];
        EspMapSpriteTopologyStatus showStatus;

        step->removedBefore = removedBefore[i];
        step->removedAfter = removedBefore[i];
        showStatus = EspMapSpriteTopology_applyShow(
            descriptor, offsets[i], &step->show);
        if (showStatus != ESP_MAP_SPRITE_TOPOLOGY_OK ||
            step->show.sourceEventIndex != descriptor->eventIndex ||
            step->show.sourceCommandOffset != offsets[i]) {
            showBatchOwner.count = i;
            if (i != 0U && !rollbackShowBatchPrefix(i)) {
                clearShowBatchOwner();
                return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
            }
            clearShowBatchOwner();
            return showStatus == ESP_MAP_SPRITE_TOPOLOGY_NOT_READY
                       ? ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY
                       : ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED;
        }
        if (i == 0U) showBatchOwner.firstGlobal = step->show.globalCommandIndex;
        showBatchOwner.committedCount = (uint8_t)(i + 1U);
    }

    if (!rollbackShowBatchPrefix(count)) {
        clearShowBatchOwner();
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    }
    showBatchOwner.committedCount = 0U;
    showBatchOwner.previewValid = 1U;
    if (showBatchOwnerLogged == 0U) {
        showBatchOwnerLogged = 1U;
        printf("[MOVEEVENT] SHOW-OWNER resultBytes=%u ownerBytes=%u max=%u storage=static stackBatchBytes=0\n",
               (unsigned int)sizeof(EspNativeGameplayMoveEventResult),
               (unsigned int)sizeof(showBatchOwner),
               (unsigned int)ESP_NATIVE_GAMEPLAY_MOVE_SHOW_BATCH_MAX);
    }
    printf("[MOVEEVENT] SHOW-BATCH-PREFLIGHT event=%u count=%u topologyProbe=apply+reverse-rollback exact=yes mutation=no storage=static\n",
           (unsigned int)descriptor->eventIndex,
           (unsigned int)count);
    return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SHOW_OK;
}

static void logPhase(const char* phase,
                     uint32_t sequence,
                     EspNativeGameplayMoveEventStatus status,
                     const EspNativeGameplayMoveEventResult* result) {
    const EspMapStatusMessageState* msg;
    uint16_t msgString = 0U;
    uint8_t msgActive = 0U;
    if (phase == NULL || result == NULL) return;
    msg = &result->statusMessage.after;
    if (status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_FORCE_MESSAGE_OK) {
        msgActive = msg->active;
        msgString = msg->active != 0U ? msg->text.index : 0U;
    }
    printf("[MOVEEVENT] %s seq=%u tile=%u flags=%08x status=%s event=%u eligible=%u opcode=%u unsupported=%u line=%u open=%u->%u locked=%u statusMsg=%u/string%u stateEvent=%u state=%u->%u removed=%u->%u mutation=%s rollback=%u\n",
           phase,
           (unsigned int)sequence,
           (unsigned int)result->tile,
           (unsigned int)result->runFlags,
           EspNativeGameplayMoveEvents_statusName(status),
           (unsigned int)result->eventIndex,
           (unsigned int)result->eligibleCount,
           (unsigned int)result->codeId,
           (unsigned int)result->unsupportedCodeId,
           (unsigned int)result->lineIndex,
           (unsigned int)result->openBefore,
           (unsigned int)result->openAfter,
           (unsigned int)result->locked,
           (unsigned int)msgActive,
           (unsigned int)msgString,
           (unsigned int)result->targetEventIndex,
           (unsigned int)result->stateBefore,
           (unsigned int)result->stateAfter,
           (unsigned int)result->removedBefore,
           (unsigned int)result->removedAfter,
           result->mutated != 0U ? "yes" : "no",
           (unsigned int)result->rollbackAvailable);
}

static EspNativeGameplayMoveEventStatus inspectPhase(
    uint16_t tile,
    uint32_t runFlags,
    EspNativeGameplayMoveEventResult* outResult) {
    const EspMapLineStateView* lineState;
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    EspMapByteCode command;
    EspMapUiIntent intent;
    uint32_t selectedOffset = UINT32_MAX;
    uint16_t selectedGlobal = 0U;
    uint8_t selectedRemoved = 0U;
    uint8_t selectedCodeId = 0U;
    uint8_t currentState;
    uint8_t eligibleCount = 0U;
    uint8_t showOffsets[ESP_NATIVE_GAMEPLAY_MOVE_SHOW_BATCH_MAX];
    uint8_t showRemovedBefore[ESP_NATIVE_GAMEPLAY_MOVE_SHOW_BATCH_MAX];
    uint8_t showCount = 0U;
    uint8_t openBefore;
    uint8_t locked;
    uint8_t targetOpen;
    uint32_t offset;

    if (outResult == NULL) return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    memset(outResult, 0, sizeof(*outResult));
    memset(showOffsets, 0, sizeof(showOffsets));
    memset(showRemovedBefore, 0, sizeof(showRemovedBefore));
    outResult->tile = tile;
    outResult->runFlags = runFlags;
    outResult->eventIndex = UINT16_MAX;
    outResult->globalCommandIndex = UINT16_MAX;
    outResult->lineIndex = UINT16_MAX;
    outResult->targetEventIndex = UINT16_MAX;

    if (tile >= 1024U || runFlags == 0U) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    }
    if (!EspNativeGameplayStatusMessage_isReady()) {
        EspNativeGameplayStatusMessage_reset();
        EspNativeGameplayStatusMessage_logCorpus();
    }
    if (!EspMapRuntime_isLoaded() || !EspMapScriptState_isReady() ||
        !EspMapLineState_isReady() ||
        !EspNativeGameplayStatusMessage_isReady()) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY;
    }

    memset(&eventRef, 0, sizeof(eventRef));
    if (!EspMapEvents_findByTile(tile, &eventRef)) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_EVENT;
    }
    memset(&descriptor, 0, sizeof(descriptor));
    if (!EspMapEvents_describe(&eventRef, &descriptor) ||
        !EspMapScriptState_getEventState(descriptor.eventIndex, &currentState)) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    }
    outResult->eventIndex = descriptor.eventIndex;

    memset(&plan, 0, sizeof(plan));
    if (!EspMapEventFilter_prepare(&descriptor,
                                   currentState,
                                   0U,
                                   runFlags,
                                   0U,
                                   &plan)) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    }

    for (offset = 0U; offset < descriptor.commandCount; ++offset) {
        uint32_t global = (uint32_t)descriptor.firstCommandIndex + offset;
        uint8_t removed;

        if (global > UINT16_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(&descriptor,
                                        &plan,
                                        offset,
                                        removed,
                                        &filtered)) {
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
        }
        if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;

        if (eligibleCount != UINT8_MAX) ++eligibleCount;
        outResult->eligibleCount = eligibleCount;
        if (filtered.codeId != ESP_MAP_OPCODE_OPENLINE &&
            filtered.codeId != ESP_MAP_OPCODE_CLOSELINE &&
            filtered.codeId != ESP_MAP_OPCODE_FORCE_MESSAGE &&
            filtered.codeId != ESP_MAP_OPCODE_DIALOG &&
            filtered.codeId != ESP_MAP_OPCODE_DIALOG_NO_BACK &&
            filtered.codeId != ESP_MAP_OPCODE_SHOW &&
            !isStateCode(filtered.codeId)) {
            outResult->unsupportedCodeId = filtered.codeId;
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED;
        }

        if (selectedOffset == UINT32_MAX) {
            selectedOffset = offset;
            selectedGlobal = filtered.globalCommandIndex;
            selectedRemoved = removed;
            selectedCodeId = filtered.codeId;
            if (filtered.codeId == ESP_MAP_OPCODE_SHOW) {
                if (offset > UINT8_MAX) {
                    return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
                }
                showOffsets[0] = (uint8_t)offset;
                showRemovedBefore[0] = removed;
                showCount = 1U;
                continue;
            }

            /* Legacy Game_runEvent() stops the current pass as soon as a dialog
             * sets saveTileEvent. Commands after the dialog belong to the saved
             * continuation and must not make this movement preflight look complex.
             * EspNativeGameplayDialog preflights/resumes them after close. */
            if (isDialogCode(filtered.codeId)) {
                break;
            }
            continue;
        }

        if (selectedCodeId == ESP_MAP_OPCODE_SHOW &&
            filtered.codeId == ESP_MAP_OPCODE_SHOW &&
            showCount < ESP_NATIVE_GAMEPLAY_MOVE_SHOW_BATCH_MAX &&
            offset <= UINT8_MAX) {
            showOffsets[showCount] = (uint8_t)offset;
            showRemovedBefore[showCount] = removed;
            ++showCount;
            continue;
        }

        logComplexSequence(&descriptor, &plan, tile, runFlags);
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
    }

    if (selectedOffset == UINT32_MAX || eligibleCount == 0U) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_ELIGIBLE;
    }

    outResult->globalCommandIndex = selectedGlobal;
    outResult->commandOffset = (uint8_t)selectedOffset;
    outResult->codeId = selectedCodeId;
    outResult->removedBefore = selectedRemoved;
    outResult->removedAfter = selectedRemoved;

    if (selectedCodeId == ESP_MAP_OPCODE_SHOW) {
        EspNativeGameplayMoveEventStatus showStatus;
        if (showCount == 0U || showCount != eligibleCount) {
            logComplexSequence(&descriptor, &plan, tile, runFlags);
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
        }
        showStatus = preflightShowBatch(&descriptor,
                                       showOffsets,
                                       showRemovedBefore,
                                       showCount);
        if (showStatus != ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SHOW_OK) {
            return showStatus;
        }
        if (showBatchOwner.previewValid == 0U ||
            showBatchOwner.eventIndex != descriptor.eventIndex ||
            showBatchOwner.count != showCount ||
            showBatchOwner.firstGlobal != selectedGlobal) {
            clearShowBatchOwner();
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
        }
        outResult->showBatchCount = showCount;
        outResult->removeIfHandled =
            showBatchOwner.step[0].show.removeCommandIfHandled;
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SHOW_OK;
    }

    if (eligibleCount != 1U || selectedOffset > UINT8_MAX ||
        !EspMapEvents_getCommand(&descriptor, selectedOffset, &command)) {
        logComplexSequence(&descriptor, &plan, tile, runFlags);
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
    }

    outResult->codeId = command.id;
    outResult->removeIfHandled =
        (uint8_t)((command.arg2 & ESP_MAP_COMMAND_FLAG_REMOVE) != 0U ? 1U : 0U);

    if (command.id == ESP_MAP_OPCODE_FORCE_MESSAGE) {
        memset(&intent, 0, sizeof(intent));
        if (EspMapUiIntent_build(&descriptor, selectedOffset, &intent) !=
                ESP_MAP_UI_INTENT_OK ||
            intent.kind != ESP_MAP_UI_INTENT_FORCE_MESSAGE ||
            intent.codeId != ESP_MAP_OPCODE_FORCE_MESSAGE) {
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
        }
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_FORCE_MESSAGE_OK;
    }

    if (isDialogCode(command.id)) {
        memset(&intent, 0, sizeof(intent));
        if (EspMapUiIntent_build(&descriptor, selectedOffset, &intent) !=
                ESP_MAP_UI_INTENT_OK ||
            intent.kind != ESP_MAP_UI_INTENT_DIALOG ||
            intent.codeId != command.id ||
            (intent.flags & ESP_MAP_UI_INTENT_FLAG_PAUSE_SCRIPT) == 0U ||
            (intent.flags & ESP_MAP_UI_INTENT_FLAG_SKIP_ADVANCE_TURN) == 0U) {
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
        }
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY;
    }

    if (isStateCode(command.id)) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SCRIPT_STATE_OK;
    }

    lineState = EspMapLineState_view();
    if (lineState == NULL || command.arg1 > UINT16_MAX ||
        command.arg1 >= lineState->lineCount ||
        !EspMapLineState_getOpen(command.arg1, &openBefore) ||
        !EspMapLineState_getLocked(command.arg1, &locked)) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    }

    targetOpen = command.id == ESP_MAP_OPCODE_OPENLINE ? 1U : 0U;
    outResult->lineIndex = (uint16_t)command.arg1;
    outResult->openBefore = openBefore;
    outResult->openAfter = targetOpen;
    outResult->locked = locked;
    outResult->soundId =
        targetOpen != 0U ? ESP_MAP_LINE_SOUND_OPEN : ESP_MAP_LINE_SOUND_CLOSE;

    if (locked != 0U) {
        outResult->openAfter = openBefore;
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_LOCKED;
    }
    if (openBefore == targetOpen) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_ALREADY_TARGET;
    }
    return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK;
}

EspNativeGameplayMoveEventStatus EspNativeGameplayMoveEvents_executePhase(
    uint16_t tile,
    uint32_t runFlags,
    EspNativeGameplayMoveEventResult* outResult) {
    EspNativeGameplayMoveEventResult inspected;
    EspNativeGameplayMoveEventStatus status;
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;

    if (outResult == NULL) return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    memset(&inspected, 0, sizeof(inspected));
    status = inspectPhase(tile, runFlags, &inspected);
    *outResult = inspected;

    if (status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY) {
        if (inspected.removeIfHandled != 0U) {
            if (!EspMapScriptState_setCommandRemoved(
                    inspected.globalCommandIndex, 1U)) {
                return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
            }
            outResult->removedAfter = 1U;
            outResult->mutated = 1U;
            outResult->rollbackAvailable = 1U;
        }
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY;
    }

    if (!phaseHandled(status)) return status;

    memset(&eventRef, 0, sizeof(eventRef));
    if (!EspMapEvents_findByTile(tile, &eventRef) ||
        eventRef.index != inspected.eventIndex ||
        !EspMapEvents_describe(&eventRef, &descriptor)) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    }

    if (status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SCRIPT_STATE_OK) {
        EspMapByteCode command;
        EspMapOpcodeExecResult script;
        EspMapOpcodeExecStatus scriptStatus;
        uint8_t removedAfter = inspected.removedBefore;

        memset(&command, 0, sizeof(command));
        memset(&script, 0, sizeof(script));
        if (!EspMapEvents_getCommand(&descriptor,
                                     inspected.commandOffset,
                                     &command) ||
            command.id != inspected.codeId ||
            !isStateCode(command.id)) {
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
        }

        scriptStatus = EspMapOpcodeExecutor_execute(&command, &script);
        if (scriptStatus != ESP_MAP_OPCODE_EXEC_OK ||
            script.status != ESP_MAP_OPCODE_EXEC_OK ||
            script.codeId != inspected.codeId ||
            script.targetEventIndex == UINT16_MAX) {
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
        }

        outResult->targetEventIndex = script.targetEventIndex;
        outResult->stateBefore = script.stateBefore;
        outResult->stateAfter = script.stateAfter;

        if (inspected.removeIfHandled != 0U &&
            inspected.removedBefore == 0U) {
            if (!EspMapScriptState_setCommandRemoved(
                    inspected.globalCommandIndex, 1U)) {
                if (script.mutated != 0U) {
                    (void)EspMapScriptState_setEventState(
                        script.targetEventIndex, script.stateBefore);
                }
                return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
            }
            removedAfter = 1U;
        }

        outResult->removedAfter = removedAfter;
        outResult->mutated =
            (uint8_t)((script.mutated != 0U ||
                       removedAfter != inspected.removedBefore)
                          ? 1U
                          : 0U);
        outResult->rollbackAvailable = outResult->mutated;
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SCRIPT_STATE_OK;
    }

    if (status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SHOW_OK) {
        uint8_t anyMutation = 0U;
        uint8_t i;

        if (inspected.showBatchCount == 0U ||
            inspected.showBatchCount > ESP_NATIVE_GAMEPLAY_MOVE_SHOW_BATCH_MAX ||
            showBatchOwner.active != 0U ||
            showBatchOwner.previewValid == 0U ||
            showBatchOwner.eventIndex != inspected.eventIndex ||
            showBatchOwner.firstGlobal != inspected.globalCommandIndex ||
            showBatchOwner.count != inspected.showBatchCount) {
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY;
        }

        showBatchOwner.committedCount = 0U;
        for (i = 0U; i < inspected.showBatchCount; ++i) {
            MoveShowStep* step = &showBatchOwner.step[i];
            EspMapShowResult applied;
            EspMapSpriteTopologyStatus showStatus;
            uint8_t removedBefore = step->removedBefore;

            memset(&applied, 0, sizeof(applied));
            showStatus = EspMapSpriteTopology_applyShow(
                &descriptor, step->show.sourceCommandOffset, &applied);
            if (showStatus != ESP_MAP_SPRITE_TOPOLOGY_OK ||
                memcmp(&applied, &step->show, sizeof(applied)) != 0) {
                if (showStatus == ESP_MAP_SPRITE_TOPOLOGY_OK) {
                    step->show = applied;
                    step->removedBefore = removedBefore;
                    step->removedAfter = removedBefore;
                    showBatchOwner.committedCount = (uint8_t)(i + 1U);
                }
                if (showBatchOwner.committedCount != 0U &&
                    !rollbackShowBatchPrefix(
                        showBatchOwner.committedCount)) {
                    clearShowBatchOwner();
                    return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
                }
                clearShowBatchOwner();
                return showStatus == ESP_MAP_SPRITE_TOPOLOGY_NOT_READY
                           ? ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY
                           : ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED;
            }

            step->show = applied;
            step->removedBefore = removedBefore;
            step->removedAfter = removedBefore;
            showBatchOwner.committedCount = (uint8_t)(i + 1U);
            if (step->show.removeCommandIfHandled != 0U &&
                step->removedBefore == 0U) {
                if (!EspMapScriptState_setCommandRemoved(
                        step->show.globalCommandIndex, 1U)) {
                    if (!rollbackShowBatchPrefix(
                            showBatchOwner.committedCount)) {
                        clearShowBatchOwner();
                        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
                    }
                    clearShowBatchOwner();
                    return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
                }
                step->removedAfter = 1U;
            }

            if (step->show.effectFlags != 0U ||
                step->removedAfter != step->removedBefore) {
                anyMutation = 1U;
            }
            printf("[MOVEEVENT] SHOW-BATCH-STEP event=%u cmd=%u sprite=%u tile=%u flags=%u visual=%u->%u linked=%u->%u blockers=%u/%u effects=%04x removed=%u->%u\n",
                   (unsigned int)step->show.sourceEventIndex,
                   (unsigned int)step->show.sourceCommandOffset,
                   (unsigned int)step->show.spriteIndex,
                   (unsigned int)step->show.tileIndex,
                   (unsigned int)step->show.showFlags,
                   (unsigned int)step->show.visualBefore,
                   (unsigned int)step->show.visualAfter,
                   (unsigned int)step->show.targetLinkedBefore,
                   (unsigned int)step->show.targetLinkedAfter,
                   (unsigned int)step->show.blockersRemoved,
                   (unsigned int)step->show.blockersFound,
                   (unsigned int)step->show.effectFlags,
                   (unsigned int)step->removedBefore,
                   (unsigned int)step->removedAfter);
        }

        showBatchOwner.previewValid = 0U;
        showBatchOwner.active = 1U;
        outResult->show = showBatchOwner.step[0].show;
        outResult->showBatchCount = showBatchOwner.count;
        outResult->globalCommandIndex =
            showBatchOwner.step[0].show.globalCommandIndex;
        outResult->commandOffset =
            showBatchOwner.step[0].show.sourceCommandOffset;
        outResult->codeId = ESP_MAP_OPCODE_SHOW;
        outResult->removedBefore = showBatchOwner.step[0].removedBefore;
        outResult->removedAfter = showBatchOwner.step[0].removedAfter;
        outResult->removeIfHandled =
            showBatchOwner.step[0].show.removeCommandIfHandled;
        outResult->mutated = anyMutation;
        outResult->rollbackAvailable = anyMutation;
        printf("[MOVEEVENT] SHOW-BATCH event=%u count=%u eligible=%u mutation=%s removedCommands=%u rollback=%u storage=static\n",
               (unsigned int)inspected.eventIndex,
               (unsigned int)showBatchOwner.count,
               (unsigned int)inspected.eligibleCount,
               anyMutation != 0U ? "yes" : "no",
               (unsigned int)showBatchOwner.count,
               (unsigned int)outResult->rollbackAvailable);
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SHOW_OK;
    }

    if (status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_FORCE_MESSAGE_OK) {
        EspNativeGameplayStatusMessageResult message;
        EspNativeGameplayStatusMessageApplyStatus messageStatus;
        memset(&message, 0, sizeof(message));
        messageStatus = EspNativeGameplayStatusMessage_apply(
            &descriptor, inspected.commandOffset, &message);
        if (messageStatus != ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_OK ||
            message.eventIndex != inspected.eventIndex ||
            message.globalCommandIndex != inspected.globalCommandIndex ||
            message.commandOffset != inspected.commandOffset ||
            message.codeId != inspected.codeId) {
            if (messageStatus == ESP_NATIVE_GAMEPLAY_STATUS_MESSAGE_OK &&
                message.rollbackAvailable != 0U) {
                (void)EspNativeGameplayStatusMessage_rollback(&message);
            }
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
        }
        outResult->statusMessage = message;
        outResult->removedBefore = message.removedBefore;
        outResult->removedAfter = message.removedAfter;
        outResult->removeIfHandled = message.removeIfHandled;
        outResult->mutated = message.ownerChanged;
        outResult->rollbackAvailable = message.rollbackAvailable;
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_FORCE_MESSAGE_OK;
    }
    else {
        EspMapLineDoorResult door;
        EspMapLineDoorStatus doorStatus;
        memset(&door, 0, sizeof(door));
        doorStatus = EspMapLineState_applyDoorCommand(
            &descriptor, inspected.commandOffset, &door);
        if (doorStatus != ESP_MAP_LINE_DOOR_OK || door.mutated != 1U ||
            door.lineIndex != inspected.lineIndex ||
            door.globalCommandIndex != inspected.globalCommandIndex ||
            door.codeId != inspected.codeId ||
            door.openBefore != inspected.openBefore ||
            door.openAfter != inspected.openAfter ||
            door.locked != inspected.locked) {
            if (doorStatus == ESP_MAP_LINE_DOOR_OK && door.mutated != 0U) {
                (void)EspMapLineState_setOpen(door.lineIndex, door.openBefore);
            }
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
        }

        outResult->mutated = 1U;
        outResult->soundId = door.soundId;
        outResult->removeIfHandled = door.removeCommandIfHandled;
        if (door.removeCommandIfHandled != 0U) {
            if (!EspMapScriptState_setCommandRemoved(
                    door.globalCommandIndex, 1U)) {
                (void)EspMapLineState_setOpen(door.lineIndex, door.openBefore);
                outResult->mutated = 0U;
                outResult->openAfter = door.openBefore;
                return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
            }
            outResult->removedAfter = 1U;
        }
        outResult->rollbackAvailable = 1U;
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK;
    }
}

int EspNativeGameplayMoveEvents_rollbackPhase(
    const EspNativeGameplayMoveEventResult* result) {
    uint8_t openNow;
    uint8_t removedNow;

    if (result == NULL) return 0;
    if (result->codeId == ESP_MAP_OPCODE_FORCE_MESSAGE) {
        return result->rollbackAvailable == 0U
                   ? 1
                   : EspNativeGameplayStatusMessage_rollback(
                         &result->statusMessage);
    }

    if (result->codeId == ESP_MAP_OPCODE_SHOW) {
        int ok;
        if (result->rollbackAvailable == 0U) return 1;
        if (showBatchOwner.active == 0U ||
            showBatchOwner.eventIndex != result->eventIndex ||
            showBatchOwner.firstGlobal != result->globalCommandIndex ||
            showBatchOwner.count != result->showBatchCount ||
            showBatchOwner.committedCount != result->showBatchCount) {
            return 0;
        }
        ok = rollbackShowBatchPrefix(showBatchOwner.committedCount);
        clearShowBatchOwner();
        return ok;
    }

    if (isDialogCode(result->codeId)) {
        if (result->rollbackAvailable == 0U) return 1;
        if (result->removedBefore == result->removedAfter ||
            !EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                                 &removedNow) ||
            removedNow != result->removedAfter ||
            !EspMapScriptState_setCommandRemoved(result->globalCommandIndex,
                                                  result->removedBefore)) {
            return 0;
        }
        return EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                                   &removedNow) &&
               removedNow == result->removedBefore;
    }

    if (isStateCode(result->codeId)) {
        uint8_t stateNow;
        if (result->rollbackAvailable == 0U) return 1;
        if (result->targetEventIndex == UINT16_MAX ||
            !EspMapScriptState_getEventState(result->targetEventIndex,
                                             &stateNow) ||
            !EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                                 &removedNow) ||
            stateNow != result->stateAfter ||
            removedNow != result->removedAfter) {
            return 0;
        }

        if (result->stateBefore != result->stateAfter &&
            !EspMapScriptState_setEventState(result->targetEventIndex,
                                             result->stateBefore)) {
            return 0;
        }
        if (result->removedBefore != result->removedAfter &&
            !EspMapScriptState_setCommandRemoved(result->globalCommandIndex,
                                                  result->removedBefore)) {
            if (result->stateBefore != result->stateAfter) {
                (void)EspMapScriptState_setEventState(result->targetEventIndex,
                                                      result->stateAfter);
            }
            return 0;
        }

        return EspMapScriptState_getEventState(result->targetEventIndex,
                                               &stateNow) &&
               EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                                   &removedNow) &&
               stateNow == result->stateBefore &&
               removedNow == result->removedBefore;
    }

    if (result->mutated != 1U || result->rollbackAvailable != 1U ||
        (result->codeId != ESP_MAP_OPCODE_OPENLINE &&
         result->codeId != ESP_MAP_OPCODE_CLOSELINE)) {
        return 0;
    }
    if (!EspMapLineState_getOpen(result->lineIndex, &openNow) ||
        !EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                             &removedNow) ||
        openNow != result->openAfter || removedNow != result->removedAfter) {
        return 0;
    }

    if (!EspMapLineState_setOpen(result->lineIndex, result->openBefore)) {
        return 0;
    }
    if (result->removedBefore != result->removedAfter &&
        !EspMapScriptState_setCommandRemoved(result->globalCommandIndex,
                                              result->removedBefore)) {
        (void)EspMapLineState_setOpen(result->lineIndex, result->openAfter);
        return 0;
    }

    return EspMapLineState_getOpen(result->lineIndex, &openNow) &&
           EspMapScriptState_isCommandRemoved(result->globalCommandIndex,
                                               &removedNow) &&
           openNow == result->openBefore &&
           removedNow == result->removedBefore;
}

const char* EspNativeGameplayMoveEvents_statusName(
    EspNativeGameplayMoveEventStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_EVENT: return "NO_EVENT";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_ELIGIBLE: return "NO_ELIGIBLE";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED: return "UNSUPPORTED";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX: return "COMPLEX";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_LOCKED: return "DOOR_LOCKED";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_ALREADY_TARGET:
        return "DOOR_ALREADY_TARGET";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK: return "DOOR_OK";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_FORCE_MESSAGE_OK:
        return "FORCE_MESSAGE_OK";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY:
        return "DIALOG_READY";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SCRIPT_STATE_OK:
        return "SCRIPT_STATE_OK";
    case ESP_NATIVE_GAMEPLAY_MOVE_EVENT_SHOW_OK:
        return "SHOW_OK";
    default: return "UNKNOWN";
    }
}

static int rollbackTransaction(void) {
    if (!transaction.active) return 1;
    if (transaction.enterRollback != 0U &&
        !EspNativeGameplayMoveEvents_rollbackPhase(
            &transaction.enterResult)) {
        return 0;
    }
    if (transaction.exitRollback != 0U &&
        !EspNativeGameplayMoveEvents_rollbackPhase(
            &transaction.exitResult)) {
        return 0;
    }
    memset(&transaction, 0, sizeof(transaction));
    return 1;
}

void EspNativeGameplayMoveEvents_onFrameResult(int renderOk) {
    if (!transaction.active) return;
    if (renderOk) {
        if (transaction.dialogPending != 0U) {
            transaction.worldRendered = 1U;
            printf("[MOVEEVENT] WORLD-READY seq=%u enterDialog=opcode%u/event%u/cmd%u rollbackLease=pending showOwnerActive=%u showEvent=%u showCount=%u\n",
                   (unsigned int)transaction.sequence,
                   (unsigned int)transaction.enterResult.codeId,
                   (unsigned int)transaction.enterResult.eventIndex,
                   (unsigned int)transaction.enterResult.commandOffset,
                   (unsigned int)showBatchOwner.active,
                   (unsigned int)(showBatchOwner.active != 0U
                                      ? showBatchOwner.eventIndex
                                      : UINT16_MAX),
                   (unsigned int)(showBatchOwner.active != 0U
                                      ? showBatchOwner.count
                                      : 0U));
            return;
        }
        printf("[MOVEEVENT] COMMIT seq=%u exitEffect=%u enterEffect=%u render=ok rollbackLease=closed\n",
               (unsigned int)transaction.sequence,
               (unsigned int)transaction.exitRollback,
               (unsigned int)transaction.enterRollback);
        releaseShowBatchOwnerForTransaction("frame-commit");
        memset(&transaction, 0, sizeof(transaction));
    }
    else {
        printf("[MOVEEVENT] FRAME-FAILED seq=%u exitEffect=%u enterEffect=%u dialog=%u rollbackLease=pending\n",
               (unsigned int)transaction.sequence,
               (unsigned int)transaction.exitRollback,
               (unsigned int)transaction.enterRollback,
               (unsigned int)transaction.dialogPending);
    }
}

int EspNativeGameplayMoveEvents_pendingDialog(
    uint32_t sequence,
    EspNativeGameplayMoveDialogIntent* outIntent) {
    if (outIntent != NULL) memset(outIntent, 0, sizeof(*outIntent));
    if (outIntent == NULL || !transaction.active ||
        transaction.sequence != sequence ||
        transaction.dialogPending == 0U ||
        transaction.worldRendered == 0U ||
        !isDialogCode(transaction.enterResult.codeId)) {
        return 0;
    }
    outIntent->runFlags = transaction.enterResult.runFlags;
    outIntent->eventIndex = transaction.enterResult.eventIndex;
    outIntent->commandOffset = transaction.enterResult.commandOffset;
    outIntent->codeId = transaction.enterResult.codeId;
    return 1;
}

int EspNativeGameplayMoveEvents_finishPendingDialog(uint32_t sequence) {
    if (!transaction.active || transaction.sequence != sequence ||
        transaction.dialogPending == 0U ||
        transaction.worldRendered == 0U ||
        !isDialogCode(transaction.enterResult.codeId)) {
        return 0;
    }
    printf("[MOVEEVENT] COMMIT seq=%u exitEffect=%u enterEffect=%u dialog=opened opcode=%u event=%u cmd=%u rollbackLease=closed\n",
           (unsigned int)transaction.sequence,
           (unsigned int)transaction.exitRollback,
           (unsigned int)transaction.enterRollback,
           (unsigned int)transaction.enterResult.codeId,
           (unsigned int)transaction.enterResult.eventIndex,
           (unsigned int)transaction.enterResult.commandOffset);
    releaseShowBatchOwnerForTransaction("dialog-finish");
    memset(&transaction, 0, sizeof(transaction));
    return 1;
}

EspNativeGameplayDispatchStatus __wrap_EspNativeGameplayDispatch_commitMove(
    const EspPlayerViewState* expectedBeforeView,
    const EspPlayerViewState* preparedAfterView,
    EspNativeGameplayMoveResult* ioResult) {
    EspNativeGameplayMoveEventResult exitPreflight;
    EspNativeGameplayMoveEventResult enterPreflight;
    EspNativeGameplayMoveEventResult exitResult;
    EspNativeGameplayMoveEventResult enterResult;
    EspNativeGameplayMoveEventStatus exitPreflightStatus;
    EspNativeGameplayMoveEventStatus enterPreflightStatus;
    EspNativeGameplayMoveEventStatus exitStatus;
    EspNativeGameplayMoveEventStatus enterStatus;
    EspNativeGameplayDispatchStatus dispatchStatus;
    uint32_t exitFlags;
    uint32_t enterFlags;

    if (expectedBeforeView == NULL || preparedAfterView == NULL ||
        ioResult == NULL ||
        !movementFlags(ioResult, preparedAfterView, &exitFlags, &enterFlags)) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID;
    }
    if (transaction.active || showBatchOwner.active) {
        printf("[MOVEEVENT] BLOCK seq=%u reason=%s transactionActive=%u transactionSeq=%u showOwnerActive=%u showEvent=%u showCount=%u failClosed=yes\n",
               (unsigned int)ioResult->sequence,
               showBatchOwner.active != 0U && transaction.active == 0U
                   ? "stale-show-owner"
                   : "transaction-busy",
               (unsigned int)transaction.active,
               (unsigned int)transaction.sequence,
               (unsigned int)showBatchOwner.active,
               (unsigned int)(showBatchOwner.active != 0U
                                  ? showBatchOwner.eventIndex
                                  : UINT16_MAX),
               (unsigned int)(showBatchOwner.active != 0U
                                  ? showBatchOwner.count
                                  : 0U));
        return ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID;
    }
    clearShowBatchOwner();

    memset(&exitPreflight, 0, sizeof(exitPreflight));
    memset(&enterPreflight, 0, sizeof(enterPreflight));
    exitPreflightStatus = inspectPhase(
        ioResult->sourceTile, exitFlags, &exitPreflight);
    enterPreflightStatus = inspectPhase(
        ioResult->destTile, enterFlags, &enterPreflight);
    logPhase("EXIT-PREFLIGHT", ioResult->sequence,
             exitPreflightStatus, &exitPreflight);
    logPhase("ENTER-PREFLIGHT", ioResult->sequence,
             enterPreflightStatus, &enterPreflight);

    /* A dialog on EXIT starts before legacy destX/destY publication and would
     * require a separate paused-move boundary. Keep that case fail-closed.
     * ENTER dialog is the recovered finishMovement route and is supported. */
    if (phaseUnsafe(exitPreflightStatus) ||
        phaseUnsafe(enterPreflightStatus) ||
        exitPreflightStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY) {
        printf("[MOVEEVENT] DEFER seq=%u reason=unsupported-complex-or-exit-dialog exit=%s enter=%s mutation=no moveCommit=no\n",
               (unsigned int)ioResult->sequence,
               EspNativeGameplayMoveEvents_statusName(exitPreflightStatus),
               EspNativeGameplayMoveEvents_statusName(enterPreflightStatus));
        return ESP_NATIVE_GAMEPLAY_DISPATCH_DEFERRED;
    }

    memset(&exitResult, 0, sizeof(exitResult));
    exitStatus = EspNativeGameplayMoveEvents_executePhase(
        ioResult->sourceTile, exitFlags, &exitResult);
    logPhase("EXIT", ioResult->sequence, exitStatus, &exitResult);
    if (phaseUnsafe(exitStatus) ||
        exitStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
    }

    dispatchStatus = __real_EspNativeGameplayDispatch_commitMove(
        expectedBeforeView, preparedAfterView, ioResult);
    if (dispatchStatus != ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {
        if (exitResult.rollbackAvailable != 0U &&
            !EspNativeGameplayMoveEvents_rollbackPhase(&exitResult)) {
            return ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
        }
        return dispatchStatus;
    }

    memset(&enterResult, 0, sizeof(enterResult));
    enterStatus = EspNativeGameplayMoveEvents_executePhase(
        ioResult->destTile, enterFlags, &enterResult);
    logPhase("ENTER", ioResult->sequence, enterStatus, &enterResult);
    if (phaseUnsafe(enterStatus)) {
        EspNativeGameplayDispatchStatus rollbackStatus =
            __real_EspNativeGameplayDispatch_rollbackMove(
                preparedAfterView, expectedBeforeView, ioResult);
        if (rollbackStatus != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||
            (exitResult.rollbackAvailable != 0U &&
             !EspNativeGameplayMoveEvents_rollbackPhase(&exitResult))) {
            return ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
        }
        return ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
    }

    if (exitResult.rollbackAvailable != 0U ||
        enterResult.rollbackAvailable != 0U ||
        enterStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY) {
        memset(&transaction, 0, sizeof(transaction));
        transaction.sequence = ioResult->sequence;
        transaction.exitResult = exitResult;
        transaction.enterResult = enterResult;
        transaction.exitRollback = exitResult.rollbackAvailable;
        transaction.enterRollback = enterResult.rollbackAvailable;
        transaction.dialogPending =
            (uint8_t)(enterStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DIALOG_READY
                          ? 1U : 0U);
        transaction.active = 1U;

        if (transaction.dialogPending != 0U &&
            exitResult.codeId == ESP_MAP_OPCODE_SHOW) {
            printf("[MOVEEVENT] SHOW-DIALOG-LEASE ARM seq=%u sourceTile=%u destTile=%u exitEvent=%u showCount=%u enterEvent=%u enterOpcode=%u enterCmd=%u showOwnerActive=%u rollbackLease=pending\n",
                   (unsigned int)ioResult->sequence,
                   (unsigned int)ioResult->sourceTile,
                   (unsigned int)ioResult->destTile,
                   (unsigned int)exitResult.eventIndex,
                   (unsigned int)exitResult.showBatchCount,
                   (unsigned int)enterResult.eventIndex,
                   (unsigned int)enterResult.codeId,
                   (unsigned int)enterResult.commandOffset,
                   (unsigned int)showBatchOwner.active);
        }
    }

    return ESP_NATIVE_GAMEPLAY_DISPATCH_OK;
}

EspNativeGameplayDispatchStatus __wrap_EspNativeGameplayDispatch_rollbackMove(
    const EspPlayerViewState* expectedAfterView,
    const EspPlayerViewState* restoreBeforeView,
    EspNativeGameplayMoveResult* ioResult) {
    EspNativeGameplayDispatchStatus status;
    uint32_t sequence = ioResult != NULL ? ioResult->sequence : 0U;

    if (transaction.active && transaction.sequence != sequence) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID;
    }

    status = __real_EspNativeGameplayDispatch_rollbackMove(
        expectedAfterView, restoreBeforeView, ioResult);
    if (status != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK) return status;

    if (transaction.active && !rollbackTransaction()) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
    }
    if (sequence != 0U) {
        printf("[MOVEEVENT] ROLLBACK seq=%u view=restored effects=restored\n",
               (unsigned int)sequence);
    }
    return status;
}
