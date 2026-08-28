#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_event_chain.h"
#include "esp_native_resident_gameplay.h"

#define CHAIN_REMOVE_FLAG 0x00000200UL
#define CHAIN_OPCODE_LIMIT 64U

typedef struct ChainCommand_s {
    uint16_t global;
    uint8_t offset;
    uint8_t codeId;
    uint8_t removedBefore;
    uint8_t reserved;
} ChainCommand;

typedef struct ChainPlan_s {
    ChainCommand command[ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS];
    uint8_t count;
    uint8_t reserved[3];
} ChainPlan;

typedef struct ChainStateUndo_s {
    EspMapOpcodeExecResult result;
} ChainStateUndo;

typedef struct ChainUnlockUndo_s {
    EspMapLineUnlockResult result;
} ChainUnlockUndo;

typedef struct ChainRemovedUndo_s {
    uint16_t global;
    uint8_t before;
    uint8_t after;
} ChainRemovedUndo;

typedef struct ChainTransaction_s {
    ChainStateUndo states[ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS];
    ChainUnlockUndo unlocks[ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS];
    ChainRemovedUndo removed[ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS];
    EspMapSpriteTopologyView topologyViewBefore;
    uint8_t* topologyBytes;
    uint32_t topologyCapacity;
    uint32_t topologyBytesUsed;
    uint16_t sourceEventIndex;
    uint16_t firstGlobal;
    uint8_t stateCount;
    uint8_t unlockCount;
    uint8_t removedCount;
    uint8_t topologyCaptured;
    uint8_t active;
    uint8_t mutated;
    uint8_t reserved[2];
} ChainTransaction;

/* The rollback journal is gameplay-only and comparatively large.  Keep only a
 * pointer in BSS so menu/map startup retains its proven contiguous heap margin;
 * acquire the reusable owner before the first dialog with a real continuation. */
static ChainTransaction* transactionOwner;
#define transaction (*transactionOwner)

static uint8_t corpusLogged;

static int ensureTransactionOwner(void) {
    if (transactionOwner != NULL) return 1;
    transactionOwner = (ChainTransaction*)SDL_calloc(1, sizeof(*transactionOwner));
    if (transactionOwner == NULL) {
        printf("[DIALOGCHAIN] DEFER reason=owner-allocation bytes=%u\n",
               (unsigned int)sizeof(*transactionOwner));
        return 0;
    }
    printf("[DIALOGCHAIN] OWNER bytes=%u allocation=lazy-gameplay\n",
           (unsigned int)sizeof(*transactionOwner));
    return 1;
}

static int eventDescriptorForIndex(uint16_t eventIndex,
                                   EspMapEventDescriptor* outDescriptor) {
    EspMapEventRef ref;
    uint32_t eventValue;
    if (outDescriptor != NULL) memset(outDescriptor, 0, sizeof(*outDescriptor));
    if (outDescriptor == NULL ||
        !EspMapRuntime_getEvent(eventIndex, &eventValue)) return 0;
    ref.index = eventIndex;
    ref.tileIndex = (uint16_t)(eventValue & ESP_MAP_EVENT_TILE_MASK);
    ref.value = eventValue;
    return EspMapEvents_describe(&ref, outDescriptor);
}

static int chainOpcodeSupported(uint8_t codeId) {
    return codeId == ESP_MAP_OPCODE_SHOW ||
           codeId == ESP_MAP_OPCODE_HIDE ||
           codeId == ESP_MAP_OPCODE_UNLOCK ||
           EspMapOpcodeExecutor_supports(codeId);
}

static EspNativeGameplayEventChainPreflightStatus buildPlan(
    uint16_t eventIndex,
    uint8_t resumeCommandOffset,
    uint32_t runFlags,
    ChainPlan* outPlan) {
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan filterPlan;
    EspMapEventCommandFilterResult filtered;
    uint8_t currentState;
    uint32_t offset;

    if (outPlan != NULL) memset(outPlan, 0, sizeof(*outPlan));
    if (outPlan == NULL ||
        !eventDescriptorForIndex(eventIndex, &descriptor) ||
        resumeCommandOffset > descriptor.commandCount ||
        !EspMapScriptState_getEventState(eventIndex, &currentState) ||
        !EspMapEventFilter_prepare(&descriptor, currentState,
                                   resumeCommandOffset, runFlags, 0U,
                                   &filterPlan)) {
        return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_INVALID;
    }

    for (offset = resumeCommandOffset; offset < descriptor.commandCount; ++offset) {
        uint32_t global = (uint32_t)descriptor.firstCommandIndex + offset;
        uint8_t removed;
        ChainCommand* command;

        if (global > UINT16_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(&descriptor, &filterPlan, offset,
                                        removed, &filtered)) {
            return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_INVALID;
        }
        if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
        if (!chainOpcodeSupported(filtered.codeId) ||
            outPlan->count >= ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS) {
            printf("[DIALOGCHAIN] PREFLIGHT-DEFER event=%u start=%u off=%u opcode=%u count=%u reason=%s\n",
                   (unsigned int)eventIndex,
                   (unsigned int)resumeCommandOffset,
                   (unsigned int)offset,
                   (unsigned int)filtered.codeId,
                   (unsigned int)outPlan->count,
                   chainOpcodeSupported(filtered.codeId) ? "chain-cap" : "unsupported-opcode");
            return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_UNSUPPORTED;
        }
        if ((filtered.codeId == ESP_MAP_OPCODE_SHOW ||
             filtered.codeId == ESP_MAP_OPCODE_HIDE) &&
            !EspMapSpriteTopology_isReady()) {
            return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY;
        }
        if (filtered.codeId == ESP_MAP_OPCODE_UNLOCK &&
            (!EspMapLineState_isReady() ||
             !EspMapLineTextureState_isReady())) {
            return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY;
        }

        command = &outPlan->command[outPlan->count++];
        command->global = filtered.globalCommandIndex;
        command->offset = (uint8_t)offset;
        command->codeId = filtered.codeId;
        command->removedBefore = removed;
    }
    return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK;
}

EspNativeGameplayEventChainPreflightStatus
EspNativeGameplayEventChain_maskForDialogBegin(
    uint16_t eventIndex,
    uint8_t resumeCommandOffset,
    uint32_t runFlags,
    EspNativeGameplayEventChainMask* outMask) {
    ChainPlan plan;
    EspNativeGameplayEventChainPreflightStatus status;
    uint8_t i;

    if (outMask != NULL) memset(outMask, 0, sizeof(*outMask));
    if (outMask == NULL) return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_INVALID;
    status = buildPlan(eventIndex, resumeCommandOffset, runFlags, &plan);
    if (status != ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK) return status;

    /* Prove the rollback owner exists before presentation.  A close/resume must
     * never discover an allocation failure after the player has seen dialog UI. */
    if (plan.count != 0U && !ensureTransactionOwner()) {
        return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY;
    }

    for (i = 0U; i < plan.count; ++i) {
        outMask->global[i] = plan.command[i].global;
        outMask->removedBefore[i] = plan.command[i].removedBefore;
        if (plan.command[i].removedBefore == 0U &&
            !EspMapScriptState_setCommandRemoved(plan.command[i].global, 1U)) {
            outMask->count = i;
            outMask->active = 1U;
            (void)EspNativeGameplayEventChain_restoreDialogMask(outMask);
            return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_INVALID;
        }
        outMask->count = (uint8_t)(i + 1U);
    }
    outMask->active = 1U;
    return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK;
}

int EspNativeGameplayEventChain_restoreDialogMask(
    EspNativeGameplayEventChainMask* mask) {
    int ok = 1;
    int i;
    if (mask == NULL || mask->active != 1U) return 0;
    for (i = (int)mask->count - 1; i >= 0; --i) {
        if (!EspMapScriptState_setCommandRemoved(mask->global[i],
                                                  mask->removedBefore[i])) {
            ok = 0;
        }
    }
    mask->active = 0U;
    return ok;
}

static void clearTransaction(void) {
    uint8_t* backup;
    uint32_t capacity;
    if (transactionOwner == NULL) return;
    backup = transaction.topologyBytes;
    capacity = transaction.topologyCapacity;
    memset(&transaction, 0, sizeof(transaction));
    transaction.topologyBytes = backup;
    transaction.topologyCapacity = capacity;
}

static int captureTopology(void) {
    const EspMapSpriteTopologyView* view = EspMapSpriteTopology_view();
    uint8_t* next;
    if (transactionOwner == NULL) return 0;
    if (transaction.topologyCaptured) return 1;
    if (view == NULL || view->storage == NULL || view->storageBytes == 0U) return 0;
    if (transaction.topologyCapacity < view->storageBytes) {
        next = (uint8_t*)SDL_realloc(transaction.topologyBytes,
                                    view->storageBytes);
        if (next == NULL) return 0;
        transaction.topologyBytes = next;
        transaction.topologyCapacity = view->storageBytes;
        printf("[DIALOGCHAIN] TOPOLOGY-SNAPSHOT ownerBytes=%u allocation=lazy-gameplay\n",
               (unsigned int)view->storageBytes);
    }
    memcpy(transaction.topologyBytes, view->storage, view->storageBytes);
    transaction.topologyViewBefore = *view;
    transaction.topologyBytesUsed = view->storageBytes;
    transaction.topologyCaptured = 1U;
    return 1;
}

static int restoreTransaction(void) {
    int ok = 1;
    int i;

    if (transactionOwner == NULL) return 0;
    for (i = (int)transaction.removedCount - 1; i >= 0; --i) {
        if (!EspMapScriptState_setCommandRemoved(
                transaction.removed[i].global,
                transaction.removed[i].before)) {
            ok = 0;
        }
    }
    for (i = (int)transaction.stateCount - 1; i >= 0; --i) {
        const EspMapOpcodeExecResult* result = &transaction.states[i].result;
        if (result->mutated != 0U &&
            !EspMapScriptState_setEventState(result->targetEventIndex,
                                              result->stateBefore)) {
            ok = 0;
        }
    }
    for (i = (int)transaction.unlockCount - 1; i >= 0; --i) {
        const EspMapLineUnlockResult* result = &transaction.unlocks[i].result;
        if (!EspMapLineState_setLocked(result->lineIndex,
                                       result->lockedBefore) ||
            !EspMapLineTextureState_setDoorTexture(result->lineIndex,
                                                    result->textureBefore)) {
            ok = 0;
        }
    }
    if (transaction.topologyCaptured) {
        const EspMapSpriteTopologyView* view = EspMapSpriteTopology_view();
        if (view == NULL || view->storage == NULL ||
            view->storageBytes != transaction.topologyBytesUsed) {
            ok = 0;
        }
        else {
            memcpy((void*)(uintptr_t)view->storage,
                   transaction.topologyBytes,
                   transaction.topologyBytesUsed);
            *(EspMapSpriteTopologyView*)(uintptr_t)view =
                transaction.topologyViewBefore;
        }
    }
    return ok;
}

static int recordRemoved(uint16_t global, uint8_t before, uint8_t after) {
    ChainRemovedUndo* undo;
    if (transactionOwner == NULL) return 0;
    if (before == after) return 1;
    if (transaction.removedCount >= ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS)
        return 0;
    undo = &transaction.removed[transaction.removedCount++];
    undo->global = global;
    undo->before = before;
    undo->after = after;
    return 1;
}

static EspNativeGameplayDialogResumeStatus executeChain(
    const EspNativeGameplayDialogClose* close,
    EspNativeGameplayDialogResumeResult* outResult) {
    EspMapEventDescriptor descriptor;
    ChainPlan plan;
    EspNativeGameplayEventChainPreflightStatus preflight;
    uint8_t i;
    uint8_t showCount = 0U;
    uint8_t hideCount = 0U;
    uint8_t unlockCount = 0U;
    uint8_t stateCount = 0U;
    uint8_t removedCount = 0U;
    uint8_t anyMutation = 0U;
    EspMapOpcodeExecResult lastState;

    memset(&lastState, 0, sizeof(lastState));
    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (close == NULL || outResult == NULL || close->resumeRequested != 1U ||
        EspNativeGameplayDialog_isActive() || !eventDescriptorForIndex(
            close->sourceEventIndex, &descriptor)) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID;
    }

    preflight = buildPlan(close->sourceEventIndex,
                          close->resumeCommandOffset,
                          close->runFlags, &plan);
    if (preflight != ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID;
    }
    if (plan.count == 0U) return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND;
    if (!ensureTransactionOwner()) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED;
    }

    clearTransaction();
    transaction.active = 1U;
    transaction.sourceEventIndex = close->sourceEventIndex;
    transaction.firstGlobal = plan.command[0].global;

    for (i = 0U; i < plan.count; ++i) {
        EspMapByteCode command;
        uint8_t removedAfter = plan.command[i].removedBefore;
        if (!EspMapEvents_getCommand(&descriptor, plan.command[i].offset,
                                     &command) ||
            command.id != plan.command[i].codeId) {
            goto failed;
        }

        if (command.id == ESP_MAP_OPCODE_SHOW) {
            EspMapShowResult result;
            if (!captureTopology()) goto failed;
            memset(&result, 0, sizeof(result));
            if (EspMapSpriteTopology_applyShow(&descriptor,
                                               plan.command[i].offset,
                                               &result) !=
                ESP_MAP_SPRITE_TOPOLOGY_OK) goto failed;
            ++showCount;
            if (result.visualBefore != result.visualAfter ||
                result.targetLinkedBefore != result.targetLinkedAfter ||
                result.blockersRemoved != 0U) anyMutation = 1U;
        }
        else if (command.id == ESP_MAP_OPCODE_HIDE) {
            EspMapHideResult result;
            if (!captureTopology()) goto failed;
            memset(&result, 0, sizeof(result));
            if (EspMapSpriteTopology_applyHide(&descriptor,
                                               plan.command[i].offset,
                                               &result) !=
                ESP_MAP_SPRITE_TOPOLOGY_OK) goto failed;
            ++hideCount;
            if (result.effectFlags != 0U) anyMutation = 1U;
        }
        else if (command.id == ESP_MAP_OPCODE_UNLOCK) {
            EspMapLineUnlockResult result;
            if (transaction.unlockCount >=
                ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS) goto failed;
            memset(&result, 0, sizeof(result));
            if (EspMapLineTextureState_applyUnlockCommand(
                    &descriptor, plan.command[i].offset, &result) !=
                ESP_MAP_LINE_UNLOCK_OK) goto failed;
            transaction.unlocks[transaction.unlockCount++].result = result;
            ++unlockCount;
            if (result.lockMutated != 0U || result.textureMutated != 0U)
                anyMutation = 1U;
        }
        else {
            EspMapOpcodeExecResult exec;
            if (transaction.stateCount >=
                ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS) goto failed;
            memset(&exec, 0, sizeof(exec));
            if (EspMapOpcodeExecutor_execute(&command, &exec) !=
                ESP_MAP_OPCODE_EXEC_OK) goto failed;
            transaction.states[transaction.stateCount++].result = exec;
            lastState = exec;
            ++stateCount;
            if (exec.mutated != 0U) anyMutation = 1U;
        }

        if ((command.arg2 & CHAIN_REMOVE_FLAG) != 0U &&
            plan.command[i].removedBefore == 0U) {
            if (!EspMapScriptState_setCommandRemoved(plan.command[i].global,
                                                      1U) ||
                !recordRemoved(plan.command[i].global,
                               plan.command[i].removedBefore, 1U)) {
                goto failed;
            }
            removedAfter = 1U;
            ++removedCount;
            anyMutation = 1U;
        }
        (void)removedAfter;
    }

    transaction.mutated = anyMutation;
    outResult->opcode = lastState;
    outResult->globalCommandIndex = plan.command[0].global;
    outResult->codeId = plan.command[plan.count - 1U].codeId;
    outResult->removedBefore = plan.command[0].removedBefore;
    outResult->removedAfter = plan.command[0].removedBefore;
    outResult->mutated = anyMutation;
    outResult->rollbackAvailable = anyMutation;

    printf("[DIALOGCHAIN] RESUME event=%u start=%u handled=%u show=%u hide=%u unlock=%u state=%u removed=%u mutation=%u topologySnapshot=%uB\n",
           (unsigned int)close->sourceEventIndex,
           (unsigned int)close->resumeCommandOffset,
           (unsigned int)plan.count,
           (unsigned int)showCount,
           (unsigned int)hideCount,
           (unsigned int)unlockCount,
           (unsigned int)stateCount,
           (unsigned int)removedCount,
           (unsigned int)anyMutation,
           (unsigned int)transaction.topologyBytesUsed);
    return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK;

failed:
    if (!restoreTransaction()) {
        printf("[DIALOGCHAIN] FAILED rollback event=%u start=%u\n",
               (unsigned int)close->sourceEventIndex,
               (unsigned int)close->resumeCommandOffset);
    }
    clearTransaction();
    memset(outResult, 0, sizeof(*outResult));
    return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED;
}

EspNativeGameplayDialogResumeStatus __wrap_EspNativeGameplayDialog_resume(
    const EspNativeGameplayDialogClose* close,
    EspNativeGameplayDialogResumeResult* outResult) {
    return executeChain(close, outResult);
}

int __wrap_EspNativeGameplayDialog_rollbackResume(
    const EspNativeGameplayDialogResumeResult* result) {
    int ok;
    if (result == NULL || result->rollbackAvailable != 1U ||
        transactionOwner == NULL || transaction.active != 1U ||
        transaction.mutated != 1U ||
        result->globalCommandIndex != transaction.firstGlobal) {
        return 0;
    }
    ok = restoreTransaction();
    printf("[DIALOGCHAIN] ROLLBACK event=%u firstGlobal=%u exact=%s\n",
           (unsigned int)transaction.sourceEventIndex,
           (unsigned int)transaction.firstGlobal,
           ok ? "yes" : "NO");
    clearTransaction();
    return ok;
}

void EspNativeGameplayEventChain_logCorpus(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    uint16_t counts[CHAIN_OPCODE_LIMIT];
    uint32_t eventIndex;
    uint32_t commands = 0U;
    uint32_t owned = 0U;
    uint32_t deferred = 0U;
    uint32_t dialogs = 0U;
    uint32_t i;

    if (corpusLogged || runtime == NULL) return;
    memset(counts, 0, sizeof(counts));
    for (eventIndex = 0U; eventIndex < runtime->eventCount; ++eventIndex) {
        uint32_t value;
        EspMapEventRef ref;
        EspMapEventDescriptor descriptor;
        uint32_t offset;
        if (!EspMapRuntime_getEvent(eventIndex, &value)) return;
        ref.index = (uint16_t)eventIndex;
        ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
        ref.value = value;
        if (!EspMapEvents_describe(&ref, &descriptor)) return;
        for (offset = 0U; offset < descriptor.commandCount; ++offset) {
            EspMapByteCode command;
            if (!EspMapEvents_getCommand(&descriptor, offset, &command)) return;
            ++commands;
            if (command.id < CHAIN_OPCODE_LIMIT &&
                counts[command.id] != UINT16_MAX) ++counts[command.id];
            if (command.id == 8U || command.id == 26U) ++dialogs;
            if (chainOpcodeSupported(command.id) || command.id == 8U ||
                command.id == 26U || command.id == 15U || command.id == 16U ||
                command.id == 24U || command.id == 40U) {
                ++owned;
            }
            else {
                ++deferred;
            }
        }
    }
    printf("[INTERACTCORPUS] READY events=%u commands=%u dialogs=%u owned=%u deferred=%u chainCap=%u\n",
           (unsigned int)runtime->eventCount,
           (unsigned int)commands,
           (unsigned int)dialogs,
           (unsigned int)owned,
           (unsigned int)deferred,
           (unsigned int)ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS);
    for (i = 0U; i < CHAIN_OPCODE_LIMIT; ++i) {
        if (counts[i] == 0U) continue;
        printf("[INTERACTCORPUS] OPCODE id=%u count=%u route=%s\n",
               (unsigned int)i,
               (unsigned int)counts[i],
               (chainOpcodeSupported((uint8_t)i) || i == 8U || i == 26U ||
                i == 15U || i == 16U || i == 24U || i == 40U)
                   ? "owned/bounded" : "DEFERRED");
    }
    corpusLogged = 1U;
}

void __real_EspNativeResidentGameplay_service(struct DoomRPG_s* doomRpg);
void __wrap_EspNativeResidentGameplay_service(struct DoomRPG_s* doomRpg) {
    EspNativeGameplayEventChain_logCorpus();
    __real_EspNativeResidentGameplay_service(doomRpg);
}
