#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_line_state.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_native_gameplay_dispatch.h"
#include "esp_native_gameplay_move_events.h"

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

typedef struct MoveEventTransaction_s {
    uint32_t sequence;
    EspNativeGameplayMoveEventResult exitResult;
    EspNativeGameplayMoveEventResult enterResult;
    uint8_t exitMutated;
    uint8_t enterMutated;
    uint8_t active;
    uint8_t reserved;
} MoveEventTransaction;

static MoveEventTransaction transaction;

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

static int phaseFatal(EspNativeGameplayMoveEventStatus status) {
    return status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID ||
           status == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY;
}

static void logPhase(const char* phase,
                     uint32_t sequence,
                     EspNativeGameplayMoveEventStatus status,
                     const EspNativeGameplayMoveEventResult* result) {
    if (phase == NULL || result == NULL) return;
    printf("[MOVEEVENT] %s seq=%u tile=%u flags=%08x status=%s event=%u eligible=%u opcode=%u unsupported=%u line=%u open=%u->%u locked=%u removed=%u->%u mutation=%s\n",
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
           (unsigned int)result->removedBefore,
           (unsigned int)result->removedAfter,
           result->mutated != 0U ? "yes" : "no");
}

EspNativeGameplayMoveEventStatus EspNativeGameplayMoveEvents_executeDoorPhase(
    uint16_t tile,
    uint32_t runFlags,
    EspNativeGameplayMoveEventResult* outResult) {
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    EspMapLineDoorResult door;
    EspMapLineDoorStatus doorStatus;
    uint32_t selectedOffset = UINT32_MAX;
    uint16_t selectedGlobal = 0U;
    uint8_t selectedRemoved = 0U;
    uint8_t currentState;
    uint8_t eligibleCount = 0U;
    uint32_t offset;

    if (outResult == NULL) return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    memset(outResult, 0, sizeof(*outResult));
    outResult->tile = tile;
    outResult->runFlags = runFlags;
    outResult->eventIndex = UINT16_MAX;
    outResult->globalCommandIndex = UINT16_MAX;
    outResult->lineIndex = UINT16_MAX;

    if (tile >= 1024U || runFlags == 0U) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    }
    if (!EspMapRuntime_isLoaded() || !EspMapScriptState_isReady() ||
        !EspMapLineState_isReady()) {
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
            filtered.codeId != ESP_MAP_OPCODE_CLOSELINE) {
            outResult->unsupportedCodeId = filtered.codeId;
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_UNSUPPORTED;
        }
        if (selectedOffset != UINT32_MAX) {
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
        }
        selectedOffset = offset;
        selectedGlobal = filtered.globalCommandIndex;
        selectedRemoved = removed;
    }

    if (selectedOffset == UINT32_MAX || eligibleCount == 0U) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NO_ELIGIBLE;
    }
    if (eligibleCount != 1U) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_COMPLEX;
    }

    memset(&door, 0, sizeof(door));
    doorStatus = EspMapLineState_applyDoorCommand(
        &descriptor, selectedOffset, &door);

    outResult->globalCommandIndex = selectedGlobal;
    outResult->lineIndex = door.lineIndex;
    outResult->soundId = door.soundId;
    outResult->commandOffset = (uint8_t)selectedOffset;
    outResult->codeId = door.codeId;
    outResult->openBefore = door.openBefore;
    outResult->openAfter = door.openAfter;
    outResult->locked = door.locked;
    outResult->mutated = door.mutated;
    outResult->removeIfHandled = door.removeCommandIfHandled;
    outResult->removedBefore = selectedRemoved;
    outResult->removedAfter = selectedRemoved;

    if (doorStatus == ESP_MAP_LINE_DOOR_LOCKED) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_LOCKED;
    }
    if (doorStatus == ESP_MAP_LINE_DOOR_ALREADY_TARGET) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_ALREADY_TARGET;
    }
    if (doorStatus == ESP_MAP_LINE_DOOR_NOT_READY) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_NOT_READY;
    }
    if (doorStatus != ESP_MAP_LINE_DOOR_OK || door.mutated != 1U) {
        return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
    }

    if (door.removeCommandIfHandled != 0U) {
        if (!EspMapScriptState_setCommandRemoved(selectedGlobal, 1U)) {
            (void)EspMapLineState_setOpen(door.lineIndex, door.openBefore);
            memset(outResult, 0, sizeof(*outResult));
            outResult->tile = tile;
            outResult->runFlags = runFlags;
            outResult->eventIndex = descriptor.eventIndex;
            outResult->globalCommandIndex = UINT16_MAX;
            outResult->lineIndex = UINT16_MAX;
            return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_INVALID;
        }
        outResult->removedAfter = 1U;
    }

    outResult->rollbackAvailable = 1U;
    return ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK;
}

int EspNativeGameplayMoveEvents_rollbackDoorPhase(
    const EspNativeGameplayMoveEventResult* result) {
    uint8_t openNow;
    uint8_t removedNow;

    if (result == NULL || result->mutated != 1U ||
        result->rollbackAvailable != 1U ||
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
    default: return "UNKNOWN";
    }
}

static int rollbackTransaction(void) {
    if (!transaction.active) return 1;
    if (transaction.enterMutated != 0U &&
        !EspNativeGameplayMoveEvents_rollbackDoorPhase(
            &transaction.enterResult)) {
        return 0;
    }
    if (transaction.exitMutated != 0U &&
        !EspNativeGameplayMoveEvents_rollbackDoorPhase(
            &transaction.exitResult)) {
        return 0;
    }
    memset(&transaction, 0, sizeof(transaction));
    return 1;
}

void EspNativeGameplayMoveEvents_onFrameResult(int renderOk) {
    if (!transaction.active) return;
    if (renderOk) {
        printf("[MOVEEVENT] COMMIT seq=%u exitDoor=%u enterDoor=%u render=ok rollbackLease=closed\n",
               (unsigned int)transaction.sequence,
               (unsigned int)transaction.exitMutated,
               (unsigned int)transaction.enterMutated);
        memset(&transaction, 0, sizeof(transaction));
    }
    else {
        printf("[MOVEEVENT] FRAME-FAILED seq=%u exitDoor=%u enterDoor=%u rollbackLease=pending\n",
               (unsigned int)transaction.sequence,
               (unsigned int)transaction.exitMutated,
               (unsigned int)transaction.enterMutated);
    }
}

EspNativeGameplayDispatchStatus __wrap_EspNativeGameplayDispatch_commitMove(
    const EspPlayerViewState* expectedBeforeView,
    const EspPlayerViewState* preparedAfterView,
    EspNativeGameplayMoveResult* ioResult) {
    EspNativeGameplayMoveEventResult exitResult;
    EspNativeGameplayMoveEventResult enterResult;
    EspNativeGameplayMoveEventStatus exitStatus;
    EspNativeGameplayMoveEventStatus enterStatus;
    EspNativeGameplayDispatchStatus dispatchStatus;
    uint32_t exitFlags;
    uint32_t enterFlags;

    if (expectedBeforeView == NULL || preparedAfterView == NULL ||
        ioResult == NULL || transaction.active ||
        !movementFlags(ioResult, preparedAfterView, &exitFlags, &enterFlags)) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_INVALID;
    }

    memset(&exitResult, 0, sizeof(exitResult));
    exitStatus = EspNativeGameplayMoveEvents_executeDoorPhase(
        ioResult->sourceTile, exitFlags, &exitResult);
    logPhase("EXIT", ioResult->sequence, exitStatus, &exitResult);
    if (phaseFatal(exitStatus)) {
        return ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
    }

    dispatchStatus = __real_EspNativeGameplayDispatch_commitMove(
        expectedBeforeView, preparedAfterView, ioResult);
    if (dispatchStatus != ESP_NATIVE_GAMEPLAY_DISPATCH_OK) {
        if (exitStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK &&
            !EspNativeGameplayMoveEvents_rollbackDoorPhase(&exitResult)) {
            return ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
        }
        return dispatchStatus;
    }

    memset(&enterResult, 0, sizeof(enterResult));
    enterStatus = EspNativeGameplayMoveEvents_executeDoorPhase(
        ioResult->destTile, enterFlags, &enterResult);
    logPhase("ENTER", ioResult->sequence, enterStatus, &enterResult);
    if (phaseFatal(enterStatus)) {
        EspNativeGameplayDispatchStatus rollbackStatus =
            __real_EspNativeGameplayDispatch_rollbackMove(
                preparedAfterView, expectedBeforeView, ioResult);
        if (rollbackStatus != ESP_NATIVE_GAMEPLAY_DISPATCH_ROLLED_BACK ||
            (exitStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK &&
             !EspNativeGameplayMoveEvents_rollbackDoorPhase(&exitResult))) {
            return ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
        }
        return ESP_NATIVE_GAMEPLAY_DISPATCH_COMMIT_FAILED;
    }

    if (exitStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK ||
        enterStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK) {
        memset(&transaction, 0, sizeof(transaction));
        transaction.sequence = ioResult->sequence;
        transaction.exitResult = exitResult;
        transaction.enterResult = enterResult;
        transaction.exitMutated =
            exitStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK ? 1U : 0U;
        transaction.enterMutated =
            enterStatus == ESP_NATIVE_GAMEPLAY_MOVE_EVENT_DOOR_OK ? 1U : 0U;
        transaction.active = 1U;
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
        printf("[MOVEEVENT] ROLLBACK seq=%u view=restored doors=restored\n",
               (unsigned int)sequence);
    }
    return status;
}
