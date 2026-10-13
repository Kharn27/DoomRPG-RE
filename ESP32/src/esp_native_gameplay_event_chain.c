#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "doomrpg_log.h"
#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_map_automap_state.h"
#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_line_state.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_opcode_executor.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_ui_intent.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_event_chain.h"
#include "esp_native_gameplay_facing_label.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_gib_fx.h"
#include "esp_native_gameplay_note_prefix.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_level_gate.h"
#include "esp_native_resident_gameplay.h"
#include "esp_native_audio_intent.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"

/* Keep ESP-IDF stdbool after the legacy DoomRPG boolean enum. */
#include <esp_heap_caps.h>

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

/* Script lifetime spans committed dialog/WAIT boundaries; the journal below
 * still owns only one synchronous transaction. No map-specific state or BSS. */
typedef struct ChainContinuation_s {
    uint32_t runFlags;
    uint32_t startedMs;
    uint32_t durationMs;
    uint32_t lastPaintMs;
    uint32_t particles;
    uint16_t eventIndex;
    uint8_t offset;
    uint8_t active;
    uint8_t waiting;
    uint8_t finished;
    uint8_t waitPresented;
    uint8_t retry;
} ChainContinuation;

typedef struct ChainTransaction_s {
    ChainStateUndo states[ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS];
    ChainUnlockUndo unlocks[ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS];
    ChainRemovedUndo removed[ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS];
    EspMapAutomapSnapshot automapBefore;
    EspMapSpriteTopologyView topologyViewBefore;
    EspNativeGameplayPlayerState playerBefore;
    ChainContinuation continuation;
    uint16_t sounds[ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS];
    uint8_t* topologyBytes;
    uint32_t topologyCapacity;
    uint32_t topologyBytesUsed;
    uint16_t sourceEventIndex;
    uint16_t firstGlobal;
    uint8_t stateCount;
    uint8_t unlockCount;
    uint8_t removedCount;
    uint8_t topologyCaptured;
    uint8_t automapCaptured;
    uint8_t active;
    uint8_t mutated;
    uint8_t playerCaptured;
    uint8_t noteStaged;
    uint8_t soundCount;
} ChainTransaction;

/* The rollback journal is gameplay-only and comparatively large.  Keep only a
 * pointer in BSS so menu/map startup retains its proven contiguous heap margin;
 * acquire the reusable owner before the first dialog with a real continuation. */
static ChainTransaction* transactionOwner;
#define transaction (*transactionOwner)

static EspNativeGameplayDialogResumeStatus beginInteraction(uint16_t eventIndex,
    uint8_t offset, uint32_t runFlags, EspNativeGameplayDialogResumeResult* result);
static EspNativeGameplayDialogResumeStatus resumeInteraction(uint16_t eventIndex,
    uint8_t offset, uint32_t runFlags, EspNativeGameplayDialogResumeResult* result);
int __wrap_EspNativeGameplayDialog_rollbackResume(
    const EspNativeGameplayDialogResumeResult* result);
extern int __real_Esp32PlatformVideo_present(void);

#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_DEBUG
static uint8_t corpusLogged;
#endif

static int currentPlayerKeys(uint32_t* outKeys) {
    const EspNativeGameplayPlayerState* player =
        EspNativeGameplayPlayerState_view();
    if (outKeys != NULL) *outKeys = 0U;
    if (outKeys == NULL || player == NULL || player->active != 1U) return 0;
    *outKeys = player->keys;
    return 1;
}

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

/*
 * The chain journal belongs to the native gameplay session, not to the whole
 * process. Session teardown is reached only after active gameplay consumers
 * return; a pending render-failure rollback is discarded with that session.
 * Free the optional topology snapshot before the journal itself.
 */
void EspNativeGameplayEventChain_reset(void) {
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t topologyCapacity;
    uint8_t pendingRollback;

    if (transactionOwner == NULL) return;
    heapBefore = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    topologyCapacity = transaction.topologyCapacity;
    pendingRollback = transaction.active;
    if (transaction.noteStaged) EspNativeGameplayNotePrefix_cancel();
    SDL_free(transaction.topologyBytes);
    transaction.topologyBytes = NULL;
    transaction.topologyCapacity = 0U;
    SDL_free(transactionOwner);
    transactionOwner = NULL;
    heapAfter = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
    printf("[DIALOGCHAIN] OWNER-RELEASE journal=%u topologyCapacity=%u activeAtTeardown=%u heap8=%u->%u recovered=%d owner=none\n",
           (unsigned int)sizeof(ChainTransaction),
           (unsigned int)topologyCapacity,
           (unsigned int)pendingRollback,
           (unsigned int)heapBefore,
           (unsigned int)heapAfter,
           (int)heapAfter - (int)heapBefore);
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
           codeId == ESP_MAP_OPCODE_GIVEMAP ||
           EspMapOpcodeExecutor_supports(codeId);
}

static int interactionOpcodeSupported(uint8_t codeId) {
    return EspMapOpcodeExecutor_supports(codeId) ||
           codeId == ESP_MAP_OPCODE_SHOW || codeId == ESP_MAP_OPCODE_HIDE || codeId == 22U ||
           codeId == 26U || codeId == 35U || codeId == 36U ||
           codeId == 37U || codeId == 40U || codeId == 42U;
}

static int interactionOwns(uint16_t eventIndex) {
    return transactionOwner != NULL && transaction.continuation.active &&
           transaction.continuation.eventIndex == eventIndex;
}

extern EspNativeGameplayDialogBeginStatus
__real_EspNativeGameplayDialog_begin(uint16_t eventIndex,
                                     uint8_t commandOffset,
                                     uint32_t runFlags);

static EspNativeGameplayDialogBeginStatus validateDialogCommand(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags) {
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan filterPlan;
    EspMapEventCommandFilterResult filtered;
    EspMapUiIntent intent;
    uint8_t currentState;
    uint8_t removed;
    uint32_t global;
    uint32_t playerKeys;

    if (!eventDescriptorForIndex(eventIndex, &descriptor) ||
        commandOffset >= descriptor.commandCount ||
        !EspMapScriptState_getEventState(eventIndex, &currentState) ||
        !currentPlayerKeys(&playerKeys) ||
        !EspMapEventFilter_prepare(&descriptor, currentState,
                                   commandOffset, runFlags, playerKeys,
                                   &filterPlan)) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY;
    }

    global = (uint32_t)descriptor.firstCommandIndex + commandOffset;
    if (global > UINT16_MAX ||
        !EspMapScriptState_isCommandRemoved(global, &removed) ||
        !EspMapEventFilter_evaluate(&descriptor, &filterPlan,
                                    commandOffset, removed, &filtered)) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
    }
    if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE ||
        (filtered.codeId != ESP_MAP_OPCODE_DIALOG &&
         filtered.codeId != ESP_MAP_OPCODE_DIALOG_NO_BACK &&
         filtered.codeId != 39U /* EV_CHECK_COMPLETED_LEVEL warning */)) {
        printf("[DIALOGCHAIN] FILTER-DEFER event=%u cmd=%u opcode=%u decision=%u keys=%02x effective=%08x runFlags=%08x\n",
               (unsigned int)eventIndex,
               (unsigned int)commandOffset,
               (unsigned int)filtered.codeId,
               (unsigned int)filtered.decision,
               (unsigned int)playerKeys,
               (unsigned int)filterPlan.effectiveFlags,
               (unsigned int)runFlags);
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
    }

    if (filtered.codeId == 39U) {
        EspMapByteCode gate;
        EspMapStringRef text;
        uint16_t eligibleMask = 0U;
        uint8_t warning = 0U;
        memset(&gate, 0, sizeof(gate));
        memset(&text, 0, sizeof(text));
        /* Re-check the exact immutable operand and the same player completion
         * predicate as MOVE preflight; do not accept arbitrary event39 dialogs
         * or invent a pass for a save with unknown completion history. */
        if (!EspMapEvents_getCommand(&descriptor, commandOffset, &gate) ||
            gate.id != 39U ||
            !EspNativeLevelGate_evaluate(
                gate.arg1,
                EspNativeGameplayPlayerState_completedLevels(),
                &eligibleMask, &warning) ||
            warning == 0U ||
            !EspMapStrings_getRef(gate.arg1 & 0xffffU, &text) ||
            text.length == 0U) {
            printf("[DIALOGCHAIN] LEVELGATE-DEFER event=%u cmd=%u reason=operand-or-gate-mismatch mutation=no\n",
                   (unsigned int)eventIndex, (unsigned int)commandOffset);
            return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
        }
        if (text.length + 1U > ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY) {
            return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_TEXT_TOO_LARGE;
        }
        printf("[DIALOGCHAIN] LEVELGATE-READY event=%u cmd=%u dialogString=%u eligibleMask=%04x completed=%04x continuation=preflight\n",
               (unsigned int)eventIndex, (unsigned int)commandOffset,
               (unsigned int)text.index, (unsigned int)eligibleMask,
               (unsigned int)EspNativeGameplayPlayerState_completedLevels());
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK;
    }

    memset(&intent, 0, sizeof(intent));
    if (EspMapUiIntent_build(&descriptor, commandOffset, &intent) !=
            ESP_MAP_UI_INTENT_OK ||
        intent.kind != ESP_MAP_UI_INTENT_DIALOG ||
        (intent.codeId != ESP_MAP_OPCODE_DIALOG &&
         intent.codeId != ESP_MAP_OPCODE_DIALOG_NO_BACK) ||
        intent.text.length + 1U > ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY) {
        return intent.text.length + 1U >
                       ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY
                   ? ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_TEXT_TOO_LARGE
                   : ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
    }

    return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK;
}

static EspNativeGameplayEventChainPreflightStatus buildPlan(
    uint16_t eventIndex,
    uint8_t resumeCommandOffset,
    uint16_t endCommandOffsetExclusive,
    uint32_t runFlags,
    ChainPlan* outPlan) {
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan filterPlan;
    EspMapEventCommandFilterResult filtered;
    uint8_t currentState;
    uint32_t playerKeys;
    uint32_t offset;

    if (outPlan != NULL) memset(outPlan, 0, sizeof(*outPlan));
    if (outPlan == NULL ||
        !eventDescriptorForIndex(eventIndex, &descriptor)) {
        return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_INVALID;
    }
    if (endCommandOffsetExclusive == UINT16_MAX) {
        endCommandOffsetExclusive = descriptor.commandCount;
    }
    if (resumeCommandOffset > descriptor.commandCount ||
        endCommandOffsetExclusive > descriptor.commandCount ||
        resumeCommandOffset > endCommandOffsetExclusive ||
        !EspMapScriptState_getEventState(eventIndex, &currentState) ||
        !currentPlayerKeys(&playerKeys) ||
        !EspMapEventFilter_prepare(&descriptor, currentState,
                                   resumeCommandOffset, runFlags, playerKeys,
                                   &filterPlan)) {
        return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_INVALID;
    }

    for (offset = resumeCommandOffset;
         offset < endCommandOffsetExclusive;
         ++offset) {
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
        if (!(chainOpcodeSupported(filtered.codeId) ||
              (interactionOwns(eventIndex) &&
               interactionOpcodeSupported(filtered.codeId))) ||
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
        if (filtered.codeId == ESP_MAP_OPCODE_GIVEMAP &&
            (!EspMapAutomapState_isReady() || !EspMapState_isReady())) {
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
    status = buildPlan(eventIndex, resumeCommandOffset, UINT16_MAX,
              runFlags, &plan);
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

EspNativeGameplayEventChainPreflightStatus
EspNativeGameplayEventChain_preflight(
    uint16_t eventIndex,
    uint8_t resumeCommandOffset,
    uint32_t runFlags) {
    ChainPlan plan;
    EspNativeGameplayEventChainPreflightStatus status =
        buildPlan(eventIndex, resumeCommandOffset, UINT16_MAX,
              runFlags, &plan);

    if (status != ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK) {
        return status;
    }
    if (plan.count != 0U && !ensureTransactionOwner()) {
        return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY;
    }
    return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK;
}

EspNativeGameplayEventChainPreflightStatus
EspNativeGameplayEventChain_preflightRange(
    uint16_t eventIndex,
    uint8_t resumeCommandOffset,
    uint16_t endCommandOffsetExclusive,
    uint32_t runFlags) {
    ChainPlan plan;
    EspNativeGameplayEventChainPreflightStatus status =
        buildPlan(eventIndex, resumeCommandOffset,
                  endCommandOffsetExclusive, runFlags, &plan);

    if (status != ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK) {
        return status;
    }
    if (plan.count != 0U && !ensureTransactionOwner()) {
        return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY;
    }
    return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK;
}

EspNativeGameplayDialogBeginStatus
EspNativeGameplayEventChain_preflightDialogCommand(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags) {
    EspNativeGameplayEventChainMask mask;
    EspNativeGameplayEventChainPreflightStatus chainStatus;
    EspNativeGameplayDialogBeginStatus dialogStatus;

    dialogStatus = validateDialogCommand(eventIndex, commandOffset, runFlags);
    if (dialogStatus != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
        return dialogStatus;
    }

    memset(&mask, 0, sizeof(mask));
    chainStatus = EspNativeGameplayEventChain_maskForDialogBegin(
        eventIndex, (uint8_t)(commandOffset + 1U), runFlags, &mask);
    if (chainStatus != ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK) {
        return chainStatus == ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY
                   ? ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY
                   : ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_UNSUPPORTED_RESUME;
    }
    if (!EspNativeGameplayEventChain_restoreDialogMask(&mask)) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
    }
    return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK;
}

EspNativeGameplayDialogBeginStatus
EspNativeGameplayEventChain_beginDialogCommand(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags) {
    EspNativeGameplayEventChainMask mask;
    EspNativeGameplayEventChainPreflightStatus chainStatus;
    EspNativeGameplayDialogBeginStatus dialogStatus;

    dialogStatus = validateDialogCommand(eventIndex, commandOffset, runFlags);
    if (dialogStatus != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
        return dialogStatus;
    }

    memset(&mask, 0, sizeof(mask));
    chainStatus = EspNativeGameplayEventChain_maskForDialogBegin(
        eventIndex, (uint8_t)(commandOffset + 1U), runFlags, &mask);
    if (chainStatus != ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK) {
        printf("[DIALOGCHAIN] BEGIN-DEFER event=%u dialogCmd=%u status=%d mutation=no\n",
               (unsigned int)eventIndex,
               (unsigned int)commandOffset,
               (int)chainStatus);
        return chainStatus == ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY
                   ? ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY
                   : ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_UNSUPPORTED_RESUME;
    }

    dialogStatus = __real_EspNativeGameplayDialog_begin(eventIndex,
                                                        commandOffset,
                                                        runFlags);
    if (!EspNativeGameplayEventChain_restoreDialogMask(&mask)) {
        printf("[DIALOGCHAIN] FAILED begin-mask-restore event=%u dialogCmd=%u\n",
               (unsigned int)eventIndex,
               (unsigned int)commandOffset);
        if (dialogStatus == ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
            EspNativeGameplayDialog_reset();
        }
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
    }
    return dialogStatus;
}

static void clearTransaction(void) {
    uint8_t* backup;
    uint32_t capacity;
    ChainContinuation continuation;
    if (transactionOwner == NULL) return;
    backup = transaction.topologyBytes;
    capacity = transaction.topologyCapacity;
    continuation = transaction.continuation;
    memset(&transaction, 0, sizeof(transaction));
    transaction.topologyBytes = backup;
    transaction.topologyCapacity = capacity;
    transaction.continuation = continuation;
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

static int captureAutomap(void) {
    if (transactionOwner == NULL) return 0;
    if (transaction.automapCaptured) return 1;
    memset(&transaction.automapBefore, 0,
           sizeof(transaction.automapBefore));
    if (!EspMapAutomapState_snapshot(&transaction.automapBefore)) return 0;
    transaction.automapCaptured = 1U;
    printf("[DIALOGCHAIN] AUTOMAP-SNAPSHOT bytes=%u allocation=journal-owner\n",
           (unsigned int)sizeof(transaction.automapBefore));
    return 1;
}

static int restoreTransaction(void) {
    int ok = 1;
    int i;

    if (transactionOwner == NULL) return 0;
    if (transaction.noteStaged) EspNativeGameplayNotePrefix_cancel();
    if (transaction.playerCaptured &&
        !EspNativeGameplayPlayerState_restore(&transaction.playerBefore)) ok = 0;
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
    if (transaction.automapCaptured &&
        !EspMapAutomapState_restore(&transaction.automapBefore)) {
        ok = 0;
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
    uint16_t endCommandOffsetExclusive,
    EspNativeGameplayDialogResumeResult* outResult) {
    EspMapEventDescriptor descriptor;
    ChainPlan plan;
    EspNativeGameplayEventChainPreflightStatus preflight;
    uint8_t i;
    uint8_t showCount = 0U;
    uint8_t hideCount = 0U;
    uint8_t unlockCount = 0U;
    uint8_t giveMapCount = 0U;
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
                          endCommandOffsetExclusive,
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
        else if (command.id == ESP_MAP_OPCODE_GIVEMAP) {
            EspMapGiveMapResult result;
            if (!captureAutomap()) goto failed;
            memset(&result, 0, sizeof(result));
            if (EspMapAutomapState_applyGiveMapCommand(
                    &descriptor, plan.command[i].offset, &result) !=
                ESP_MAP_GIVEMAP_OK) goto failed;
            ++giveMapCount;
            if (result.mutated != 0U) anyMutation = 1U;
            printf("[GIVEMAP] COMMIT event=%u cmd=%u lines=%u/%u sprites=%u/%u entrances=%u/%u remove=%u\n",
                   (unsigned int)result.sourceEventIndex,
                   (unsigned int)result.sourceCommandOffset,
                   (unsigned int)result.linesMutated,
                   (unsigned int)result.lineTargetCount,
                   (unsigned int)result.spritesMutated,
                   (unsigned int)result.spriteTargetCount,
                   (unsigned int)result.tilesMutated,
                   (unsigned int)result.entranceTargetCount,
                   (unsigned int)result.removeCommandIfHandled);
        }
        else if (interactionOwns(close->sourceEventIndex) && command.id == 22U) {
            EspNativeGameplayPlayerState next;
            uint32_t amount = (command.arg1 >> 8U) & 255U;
            if ((command.arg1 & 255U) != 2U || command.arg1 >> 16U ||
                !EspNativeGameplayPlayerState_snapshot(&next) ||
                next.credits < amount) goto failed;
            if (!transaction.playerCaptured) {
                transaction.playerBefore = next;
                transaction.playerCaptured = 1U;
            }
            next.credits -= amount;
            if (!EspNativeGameplayPlayerState_restore(&next)) goto failed;
            if (amount != 0U) anyMutation = 1U;
            printf("[SCRIPTSTAT] DEBIT event=%u cmd=%u credits=%u->%u amount=%u provisional=yes rng=untouched\n",
                   (unsigned)close->sourceEventIndex, (unsigned)plan.command[i].offset,
                   (unsigned)(next.credits + amount), (unsigned)next.credits,
                   (unsigned)amount);
        }
        else if (interactionOwns(close->sourceEventIndex) && command.id == 40U) {
            if (transaction.noteStaged ||
                !EspNativeGameplayNotePrefix_prepare(close->sourceEventIndex,
                                                      plan.command[i].offset)) goto failed;
            transaction.noteStaged = 1U;
            anyMutation = 1U;
        }
        else if (interactionOwns(close->sourceEventIndex) && command.id == 42U) {
            if (transaction.soundCount >= ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS ||
                (command.arg1 & 0xffffU) == 0U) goto failed;
            transaction.sounds[transaction.soundCount++] = (uint16_t)command.arg1;
        }
        else if (interactionOwns(close->sourceEventIndex) && command.id == 35U) {
            /* Validated capture family, armed by the following WAIT boundary.
             * No legacy ParticleSystem or gameplay RNG is materialized. */
        }
        else if (interactionOwns(close->sourceEventIndex) && command.id == 36U) {
            /* Settled facing/view refresh is performed by the enclosing frame. */
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

    printf("[DIALOGCHAIN] RESUME event=%u start=%u handled=%u show=%u hide=%u unlock=%u givemap=%u state=%u removed=%u mutation=%u topologySnapshot=%uB automapSnapshot=%s\n",
           (unsigned int)close->sourceEventIndex,
           (unsigned int)close->resumeCommandOffset,
           (unsigned int)plan.count,
           (unsigned int)showCount,
           (unsigned int)hideCount,
           (unsigned int)unlockCount,
           (unsigned int)giveMapCount,
           (unsigned int)stateCount,
           (unsigned int)removedCount,
           (unsigned int)anyMutation,
           (unsigned int)transaction.topologyBytesUsed,
           transaction.automapCaptured ? "yes" : "no");
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

/* Full immutable operand/closure proof before a credit can be debited. This
 * deliberately admits only credit-check/debit, one no-back dialog and one
 * capture-particle WAIT. Segment sizes retain the existing 12-command journal. */
EspNativeGameplayEventChainPreflightStatus
EspNativeGameplayEventChain_preflightInteraction(uint16_t eventIndex,
    uint8_t start, uint32_t runFlags) {
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan filter;
    EspMapEventCommandFilterResult eligible;
    EspMapByteCode command;
    EspMapStringRef text;
    uint32_t keys, offset, required = 0U;
    uint8_t state, count = 0U, segment = 0U, dialog = 0U, suffix = 0U;
    uint8_t particles = 0U, wait = 0U, notes = 0U;
    if (!eventDescriptorForIndex(eventIndex, &descriptor) ||
        start >= descriptor.commandCount || !currentPlayerKeys(&keys) ||
        !EspMapScriptState_getEventState(eventIndex, &state) ||
        !EspMapEventFilter_prepare(&descriptor, state, start, runFlags, keys, &filter))
        return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY;
    for (offset = start; offset < descriptor.commandCount; ++offset) {
        uint8_t removed;
        uint32_t global = descriptor.firstCommandIndex + offset;
        if (offset > UINT8_MAX || global > UINT16_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(&descriptor, &filter, offset, removed, &eligible) ||
            !EspMapEvents_getCommand(&descriptor, offset, &command))
            return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_INVALID;
        if (eligible.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
        ++count; ++segment;
        if (dialog) ++suffix;
        if (segment > ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS ||
            suffix > ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_MAX_COMMANDS) goto unsupported;
        if (count == 1U) {
            if (offset != start || command.id != 23U ||
                (command.arg1 & 255U) != 2U || (command.arg2 & CHAIN_REMOVE_FLAG) ||
                !EspMapStrings_getRef(command.arg1 >> 16U, &text) ||
                text.length + 1U > ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY) goto unsupported;
            required = (command.arg1 >> 8U) & 255U;
            continue;
        }
        if (!interactionOpcodeSupported(command.id)) goto unsupported;
        if (command.id == 22U) {
            if (count != 2U || (command.arg1 & 255U) != 2U || command.arg1 >> 16U ||
                ((command.arg1 >> 8U) & 255U) != required) goto unsupported;
        } else if (count == 2U) goto unsupported;
        if (command.id == 26U || command.id == 40U) {
            if (!EspMapStrings_getRef(command.arg1, &text) ||
                text.length + 1U > ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY) goto unsupported;
            if (command.id == 26U) {
                if (dialog++ || particles || wait || (command.arg2 & CHAIN_REMOVE_FLAG)) goto unsupported;
                segment = 0U;
            } else if (notes++) goto unsupported;
        } else if (command.id == 35U) {
            if (!dialog || particles++ || wait || ((command.arg1 >> 24U) & 31U) != 3U)
                goto unsupported;
        } else if (command.id == 37U) {
            if (!particles || wait++ || command.arg1 == 0U || command.arg1 > 10000U ||
                (command.arg2 & CHAIN_REMOVE_FLAG))
                goto unsupported;
            segment = 0U;
        } else if (command.id == 42U) {
            if ((command.arg1 & 0xffffU) == 0U) goto unsupported;
        } else if (command.id == ESP_MAP_OPCODE_SHOW) {
            const EspMapRuntimeView* runtime = EspMapRuntime_view();
            EspMapShowResult preview;
            if (!EspMapSpriteTopology_isReady() || runtime == NULL ||
                (command.arg1 & 0xffffU) >= runtime->mapSpriteCount ||
                EspMapSpriteTopology_previewShow(&descriptor, offset, &preview) !=
                    ESP_MAP_SPRITE_TOPOLOGY_OK) goto unsupported;
        } else if (command.id == ESP_MAP_OPCODE_HIDE) {
            if (!EspMapSpriteTopology_isReady() || (command.arg1 & 255U) > 31U ||
                ((command.arg1 >> 8U) & 255U) > 31U) goto unsupported;
        } else if (EspMapOpcodeExecutor_supports(command.id)) {
            EspMapEventRef target;
            uint32_t x = command.arg1 & 255U, y = (command.arg1 >> 8U) & 255U;
            if (x > 31U || y > 31U || !EspMapEvents_findByTile(x + y * 32U, &target) ||
                (command.id == ESP_MAP_OPCODE_CHANGE_STATE &&
                 ((command.arg1 >> 16U) & 255U) > 15U)) goto unsupported;
        }
    }
    if (count < 3U || dialog != 1U || particles != 1U || wait != 1U) goto unsupported;
    return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK;
unsupported:
    printf("[SCRIPTSTAT] PREFLIGHT-DEFER event=%u start=%u off=%u opcode=%u reason=unsupported-closure mutation=no\n",
           (unsigned)eventIndex, (unsigned)start, (unsigned)offset, (unsigned)command.id);
    return ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_UNSUPPORTED;
}

int EspNativeGameplayEventChain_commit(const EspNativeGameplayDialogResumeResult* result) {
    uint8_t i;
    /* Keep established synchronous families' journal lifetime untouched. Only
     * the new segmented interaction needs an explicit publication commit. */
    if (transactionOwner == NULL || !transaction.continuation.active) return 1;
    if (result == NULL || transactionOwner == NULL || !transaction.active ||
        result->globalCommandIndex != transaction.firstGlobal) return 0;
    if (transaction.noteStaged) EspNativeGameplayNotePrefix_commit();
    for (i = 0U; i < transaction.soundCount; ++i)
        (void)EspNativeAudioIntent_publish(transaction.sounds[i], 0U, 3U);
    if (transaction.continuation.finished) {
        printf("[SCRIPTSTAT] COMPLETE event=%u credits=%u wait=finished note=committed input=world rng=untouched\n",
               (unsigned)transaction.continuation.eventIndex,
               (unsigned)EspNativeGameplayPlayerState_view()->credits);
        memset(&transaction.continuation, 0, sizeof(transaction.continuation));
    }
    clearTransaction();
    return 1;
}

static int readInteractionText(uint32_t index, char* text, size_t capacity) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspMapStringRef ref;
    EspAssetPackEntry entry;
    const char* name;
    int ok;
    if (view == NULL || EspAssetPack_isOpen() ||
        !EspMapStrings_getRef(index, &ref)) return 0;
    name = EspMapCatalog_nameForId(view->targetMapId);
    if (name == NULL || !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
    memset(&entry, 0, sizeof(entry));
    ok = EspAssetPack_findEntry(name, &entry) &&
         EspMapStrings_read(&entry, &ref, text, capacity, NULL) == ESP_MAP_STRING_READ_OK;
    EspAssetPack_close();
    return ok && !EspAssetPack_isOpen();
}

static EspNativeGameplayDialogResumeStatus beginInteraction(uint16_t eventIndex,
    uint8_t offset, uint32_t runFlags, EspNativeGameplayDialogResumeResult* result) {
    EspMapEventDescriptor descriptor;
    EspMapByteCode gate;
    const EspNativeGameplayPlayerState* player = EspNativeGameplayPlayerState_view();
    uint32_t required;
    if (result == NULL) return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID;
    memset(result, 0, sizeof(*result));
    if (player == NULL || (transactionOwner != NULL && transaction.continuation.active) ||
        EspNativeGameplayEventChain_preflightInteraction(eventIndex, offset, runFlags) !=
            ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK ||
        !eventDescriptorForIndex(eventIndex, &descriptor) ||
        !EspMapEvents_getCommand(&descriptor, offset, &gate))
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID;
    required = (gate.arg1 >> 8U) & 255U;
    printf("[SCRIPTSTAT] CHECK event=%u cmd=%u required=%u credits=%u result=%s\n",
           (unsigned)eventIndex, (unsigned)offset, (unsigned)required,
           (unsigned)player->credits, player->credits >= required ? "pass" : "insufficient");
    if (player->credits < required) {
        char text[ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY];
        if (!readInteractionText(gate.arg1 >> 16U, text, sizeof(text)))
            return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED;
        if (text[0] == '\0') strcpy(text, "Insufficient funds!");
        if (EspNativeGameplayDialog_beginStandalone(text) != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK)
            return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED;
        printf("[SCRIPTSTAT] BLOCK event=%u debit=no continuation=no removed=no\n", (unsigned)eventIndex);
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_PAUSED;
    }
    if (!ensureTransactionOwner()) return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED;
    memset(&transaction.continuation, 0, sizeof(transaction.continuation));
    transaction.continuation.eventIndex = eventIndex;
    transaction.continuation.runFlags = runFlags;
    transaction.continuation.active = 1U;
    {
        EspNativeGameplayDialogResumeStatus status =
            resumeInteraction(eventIndex, (uint8_t)(offset + 1U), runFlags, result);
        if (status == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED ||
            status == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID)
            memset(&transaction.continuation, 0, sizeof(transaction.continuation));
        return status;
    }
}

static EspNativeGameplayDialogResumeStatus resumeInteraction(uint16_t eventIndex,
    uint8_t start, uint32_t runFlags, EspNativeGameplayDialogResumeResult* result) {
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan filter;
    EspMapEventCommandFilterResult eligible;
    EspMapByteCode boundary;
    EspNativeGameplayDialogClose close;
    EspNativeGameplayDialogResumeStatus status;
    uint32_t keys, offset;
    uint8_t state, pause = 0U;
    if (!interactionOwns(eventIndex) || EspNativeGameplayDialog_isActive() ||
        !eventDescriptorForIndex(eventIndex, &descriptor) || !currentPlayerKeys(&keys) ||
        !EspMapScriptState_getEventState(eventIndex, &state) ||
        !EspMapEventFilter_prepare(&descriptor, state, start, runFlags, keys, &filter))
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID;
    memset(&boundary, 0, sizeof(boundary));
    for (offset = start; offset < descriptor.commandCount; ++offset) {
        uint8_t removed;
        EspMapByteCode command;
        if (!EspMapScriptState_isCommandRemoved(descriptor.firstCommandIndex + offset, &removed) ||
            !EspMapEventFilter_evaluate(&descriptor, &filter, offset, removed, &eligible) ||
            !EspMapEvents_getCommand(&descriptor, offset, &command))
            return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID;
        if (eligible.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
        if (command.id == 26U || command.id == 37U) {
            boundary = command; pause = command.id; break;
        }
        if (command.id == 35U) transaction.continuation.particles = command.arg1;
    }
    memset(&close, 0, sizeof(close));
    close.sourceEventIndex = eventIndex;
    close.resumeCommandOffset = start;
    close.runFlags = runFlags;
    close.resumeRequested = 1U;
    status = executeChain(&close, (uint16_t)offset, result);
    if (status != ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK &&
        status != ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND) return status;
    if (pause == 26U) {
        if (EspNativeGameplayEventChain_beginDialogCommand(eventIndex, (uint8_t)offset, runFlags) !=
                ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
            if (transaction.active) { (void)restoreTransaction(); clearTransaction(); }
            memset(&transaction.continuation, 0, sizeof(transaction.continuation));
            return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED;
        }
        if (status == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK &&
            !EspNativeGameplayEventChain_commit(result)) return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED;
        transaction.continuation.offset = (uint8_t)(offset + 1U);
        transaction.continuation.waiting = 0U;
        result->rollbackAvailable = 0U;
        printf("[SCRIPTSTAT] PAUSE event=%u dialogCmd=%u next=%u debit=committed-on-dialog-open\n",
               (unsigned)eventIndex, (unsigned)offset, (unsigned)(offset + 1U));
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_PAUSED;
    }
    if (pause == 37U) {
        transaction.continuation.offset = (uint8_t)(offset + 1U);
        transaction.continuation.durationMs = boundary.arg1;
        transaction.continuation.waiting = 1U;
        transaction.continuation.waitPresented = 0U;
        /* No removed WAIT flag is admitted; the boundary is replayed only by
         * the explicit continuation, never by a buffered world SELECT. */
        printf("[SCRIPTWAIT] ARM event=%u cmd=%u duration=%u next=%u input=blocked timer=monotonic\n",
               (unsigned)eventIndex, (unsigned)offset, (unsigned)boundary.arg1,
               (unsigned)(offset + 1U));
        return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_PAUSED;
    }
    transaction.continuation.finished = 1U;
    if (status == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND) {
        memset(&transaction.continuation, 0, sizeof(transaction.continuation));
    }
    return status;
}

int EspNativeGameplayEventChain_isWaiting(void) {
    return transactionOwner != NULL && transaction.continuation.active &&
           transaction.continuation.waiting;
}

int EspNativeGameplayEventChain_serviceWait(struct Render_s* render) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspNativeGameplayFrameStats frame;
    EspNativeGameplayDialogResumeResult result;
    EspNativeGameplayDialogResumeStatus status;
    uint32_t now = DoomRPG_GetUpTimeMS(), age;
    if (!EspNativeGameplayEventChain_isWaiting()) return 1;
    if (render == NULL || view == NULL || !view->active ||
        view->viewAngle != view->destAngle) return 0;
    age = transaction.continuation.waitPresented
        ? (uint32_t)(now - transaction.continuation.startedMs) : 0U;
    if (age < transaction.continuation.durationMs) {
        if (transaction.continuation.waitPresented &&
            now - transaction.continuation.lastPaintMs < 50U) return 1;
        memset(&frame, 0, sizeof(frame));
        if (!EspNativeGameplayFrame_composeTurn(render, (uint8_t)view->viewAngle, &frame)) {
            return 1; /* Retain/retry the unpublished visual, never complete it. */
        }
        if (!EspNativeGameplayGibFx_paintCapture(transaction.continuation.particles,
                age, transaction.continuation.durationMs, transaction.continuation.eventIndex) ||
            !__real_Esp32PlatformVideo_present()) return 1;
        /* Capture owns this physical publish, like other native modals. Do not
         * let old hit/gib/action decorators overwrite its composed pixels. */
        if (!transaction.continuation.waitPresented) {
            transaction.continuation.startedMs = DoomRPG_GetUpTimeMS();
            transaction.continuation.waitPresented = 1U;
            if (transaction.active && !EspNativeGameplayEventChain_commit(&(
                    EspNativeGameplayDialogResumeResult){.globalCommandIndex = transaction.firstGlobal})) return 0;
            printf("[SCRIPTWAIT] PRESENT event=%u particles=45 rng=visual-local gameplayRng=untouched\n",
                   (unsigned)transaction.continuation.eventIndex);
        }
        transaction.continuation.lastPaintMs = now;
        return 1;
    }
    if (transaction.continuation.retry &&
        now - transaction.continuation.lastPaintMs < 100U) return 1;
    memset(&result, 0, sizeof(result));
    status = resumeInteraction(transaction.continuation.eventIndex,
        transaction.continuation.offset, transaction.continuation.runFlags, &result);
    if (status == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_PAUSED) return 1;
    if (status == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED) {
        transaction.continuation.retry = 1U;
        transaction.continuation.lastPaintMs = now;
        printf("[SCRIPTWAIT] RETRY event=%u phase=suffix transaction=rolled-back input=blocked\n",
               (unsigned)transaction.continuation.eventIndex);
        return 1;
    }
    if (status != ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_OK &&
        status != ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND) return 0;
    (void)EspNativeGameplayFacingLabel_refresh("SCRIPT-WAIT-CLOSE");
    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(render, (uint8_t)view->viewAngle, &frame)) {
        if (result.rollbackAvailable && __wrap_EspNativeGameplayDialog_rollbackResume(&result)) {
            transaction.continuation.finished = 0U;
            transaction.continuation.retry = 1U;
            transaction.continuation.lastPaintMs = now;
            return 1;
        }
        return 0;
    }
    return status == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_NO_COMMAND ||
           EspNativeGameplayEventChain_commit(&result);
}

EspNativeGameplayDialogResumeStatus
EspNativeGameplayEventChain_execute(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags,
    EspNativeGameplayDialogResumeResult* outResult) {
    EspNativeGameplayDialogClose close;
    memset(&close, 0, sizeof(close));
    close.sourceEventIndex = eventIndex;
    close.resumeCommandOffset = commandOffset;
    close.runFlags = runFlags;
    close.resumeRequested = 1U;
    {
        EspMapEventDescriptor descriptor;
        EspMapByteCode entry;
        if (eventDescriptorForIndex(eventIndex, &descriptor) &&
            EspMapEvents_getCommand(&descriptor, commandOffset, &entry) &&
            entry.id == 23U) {
            return beginInteraction(eventIndex,
                       commandOffset, runFlags, outResult);
        }
    }
    return executeChain(&close, UINT16_MAX, outResult);
}

EspNativeGameplayDialogResumeStatus
EspNativeGameplayEventChain_executeRange(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint16_t endCommandOffsetExclusive,
    uint32_t runFlags,
    EspNativeGameplayDialogResumeResult* outResult) {
    EspNativeGameplayDialogClose close;
    memset(&close, 0, sizeof(close));
    close.sourceEventIndex = eventIndex;
    close.resumeCommandOffset = commandOffset;
    close.runFlags = runFlags;
    close.resumeRequested = 1U;
    return executeChain(&close, endCommandOffsetExclusive, outResult);
}

EspNativeGameplayDialogResumeStatus __wrap_EspNativeGameplayDialog_resume(
    const EspNativeGameplayDialogClose* close,
    EspNativeGameplayDialogResumeResult* outResult) {
    if (close != NULL && interactionOwns(close->sourceEventIndex)) {
        if (!close->resumeRequested ||
            close->resumeCommandOffset != transaction.continuation.offset ||
            close->runFlags != transaction.continuation.runFlags) {
            return ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID;
        }
        return resumeInteraction(close->sourceEventIndex,
                   close->resumeCommandOffset, close->runFlags, outResult);
    }
    return executeChain(close, UINT16_MAX, outResult);
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

#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_DEBUG
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
#endif
