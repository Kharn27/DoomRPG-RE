#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"
#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_notebook.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"
#include "esp_map_ui_intent.h"
#include "esp_native_gameplay_dialog.h"
#include "esp_native_gameplay_event_chain.h"
#include "esp_player_view_state.h"

#define NOTE_REMOVE_FLAG 0x00000200UL
#define NOTE_SCRATCH_CAPACITY ESP_NATIVE_GAMEPLAY_DIALOG_TEXT_CAPACITY

typedef struct EspNativeGameplayNotePrefixState_s {
    EspMapNotebookState notebook;
    EspMapNotebookState candidate;
    char scratch[NOTE_SCRATCH_CAPACITY];
    uint8_t active;
    uint8_t targetMapId;
    uint8_t busy;
    uint8_t reserved;
} EspNativeGameplayNotePrefixState;

/* NOTE is not needed during boot/prologue.  Keep only a pointer in BSS and
 * acquire this bounded map-local owner on the first actual NOTE+dialog pair.
 * It is then reused across the session/map changes with the existing map-id
 * reset semantics, so no per-event allocation is introduced. */
static EspNativeGameplayNotePrefixState* notePrefix;

extern EspNativeGameplayDialogBeginStatus
__real_EspNativeGameplayDialog_begin(uint16_t eventIndex,
                                     uint8_t commandOffset,
                                     uint32_t runFlags);

static int eventDescriptorForIndex(uint16_t eventIndex,
                                   EspMapEventDescriptor* outDescriptor) {
    EspMapEventRef ref;
    uint32_t eventValue;

    if (outDescriptor != NULL) memset(outDescriptor, 0, sizeof(*outDescriptor));
    if (outDescriptor == NULL ||
        !EspMapRuntime_getEvent(eventIndex, &eventValue)) {
        return 0;
    }
    ref.index = eventIndex;
    ref.tileIndex = (uint16_t)(eventValue & ESP_MAP_EVENT_TILE_MASK);
    ref.value = eventValue;
    return EspMapEvents_describe(&ref, outDescriptor);
}

/* The dialog owner's historical preflight accepts only zero/one state opcode.
 * The permanent chain layer now proves the whole saved continuation first.  We
 * temporarily hide that already-proven continuation from the old preflight,
 * open the dialog, then restore the removed-bit overlay immediately.  No world
 * command executes before the dialog closes. */
static EspNativeGameplayDialogBeginStatus beginWithChainPreflight(
    uint16_t eventIndex,
    uint8_t commandOffset,
    uint32_t runFlags) {
    EspNativeGameplayEventChainMask mask;
    EspNativeGameplayEventChainPreflightStatus chainStatus;
    EspNativeGameplayDialogBeginStatus dialogStatus;

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

/*
 * Return 1 for one canonical NOTE immediately preceding the dialog, 0 when the
 * dialog has no eligible NOTE prefix, and -1 for any unsupported eligible
 * prefix shape. This is a transaction preflight only; no owner or script state
 * is changed here.
 */
static int prepareNotePrefix(uint16_t eventIndex,
                             uint8_t dialogOffset,
                             uint32_t runFlags,
                             EspMapUiIntent* outIntent,
                             uint16_t* outGlobal,
                             uint8_t* outRemoved,
                             uint8_t* outRemoveIfHandled) {
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    EspMapUiIntent dialogIntent;
    uint8_t currentState;
    uint8_t found = 0U;
    uint32_t offset;

    if (outIntent != NULL) memset(outIntent, 0, sizeof(*outIntent));
    if (outGlobal != NULL) *outGlobal = UINT16_MAX;
    if (outRemoved != NULL) *outRemoved = 0U;
    if (outRemoveIfHandled != NULL) *outRemoveIfHandled = 0U;
    if (outIntent == NULL || outGlobal == NULL || outRemoved == NULL ||
        outRemoveIfHandled == NULL || dialogOffset == 0U ||
        !eventDescriptorForIndex(eventIndex, &descriptor) ||
        dialogOffset >= descriptor.commandCount ||
        !EspMapScriptState_getEventState(eventIndex, &currentState) ||
        !EspMapEventFilter_prepare(&descriptor, currentState, 0U,
                                   runFlags, 0U, &plan)) {
        return dialogOffset == 0U ? 0 : -1;
    }

    memset(&dialogIntent, 0, sizeof(dialogIntent));
    if (EspMapUiIntent_build(&descriptor, dialogOffset, &dialogIntent) !=
            ESP_MAP_UI_INTENT_OK ||
        (dialogIntent.codeId != ESP_MAP_OPCODE_DIALOG &&
         dialogIntent.codeId != ESP_MAP_OPCODE_DIALOG_NO_BACK)) {
        return -1;
    }

    for (offset = 0U; offset < dialogOffset; ++offset) {
        uint32_t global = (uint32_t)descriptor.firstCommandIndex + offset;
        uint8_t removed;

        if (global > UINT16_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(&descriptor, &plan, offset,
                                        removed, &filtered)) {
            return -1;
        }
        if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
        if (found != 0U || filtered.codeId != ESP_MAP_OPCODE_NOTE ||
            offset + 1U != dialogOffset ||
            EspMapUiIntent_build(&descriptor, offset, outIntent) !=
                ESP_MAP_UI_INTENT_OK ||
            outIntent->kind != ESP_MAP_UI_INTENT_APPEND_NOTE ||
            outIntent->codeId != ESP_MAP_OPCODE_NOTE) {
            return -1;
        }

        found = 1U;
        *outGlobal = filtered.globalCommandIndex;
        *outRemoved = removed;
        *outRemoveIfHandled =
            (uint8_t)((filtered.arg2 & NOTE_REMOVE_FLAG) != 0U);
    }

    return found != 0U ? 1 : 0;
}

EspNativeGameplayDialogBeginStatus
__wrap_EspNativeGameplayDialog_begin(uint16_t eventIndex,
                                     uint8_t commandOffset,
                                     uint32_t runFlags) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspMapUiIntent noteIntent;
    EspAssetPackEntry mapEntry;
    EspMapNotebookApplyStatus applyStatus;
    EspNativeGameplayDialogBeginStatus dialogStatus;
    EspNativeGameplayNotePrefixState* state = notePrefix;
    const char* mapName;
    size_t readLength = 0U;
    uint16_t global = UINT16_MAX;
    uint16_t beforeLength;
    uint8_t removedBefore = 0U;
    uint8_t removeIfHandled = 0U;
    int prefixStatus;
    int removedChanged = 0;

    if (state != NULL && state->busy != 0U) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
    }

    memset(&noteIntent, 0, sizeof(noteIntent));
    prefixStatus = prepareNotePrefix(eventIndex, commandOffset, runFlags,
                                     &noteIntent, &global, &removedBefore,
                                     &removeIfHandled);
    if (prefixStatus < 0) {
        printf("[NOTE] DEFER event=%u dialogCmd=%u reason=unsupported-prefix\n",
               (unsigned int)eventIndex,
               (unsigned int)commandOffset);
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
    }
    if (prefixStatus == 0) {
        return beginWithChainPreflight(eventIndex, commandOffset, runFlags);
    }

    if (view == NULL || view->active != 1U ||
        !EspMapCatalog_isValidId(view->targetMapId) ||
        EspAssetPack_isOpen()) {
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY;
    }

    if (state == NULL) {
        state = (EspNativeGameplayNotePrefixState*)SDL_calloc(1, sizeof(*state));
        if (state == NULL) {
            printf("[NOTE] DEFER event=%u dialogCmd=%u reason=owner-allocation bytes=%u\n",
                   (unsigned int)eventIndex,
                   (unsigned int)commandOffset,
                   (unsigned int)sizeof(*state));
            return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_NOT_READY;
        }
        notePrefix = state;
        printf("[NOTE] OWNER bytes=%u allocation=lazy-gameplay\n",
               (unsigned int)sizeof(*state));
    }

    if (state->active == 0U ||
        state->targetMapId != view->targetMapId) {
        EspMapNotebook_reset(&state->notebook);
        state->active = 1U;
        state->targetMapId = view->targetMapId;
    }

    state->busy = 1U;
    state->candidate = state->notebook;
    beforeLength = EspMapNotebook_length(&state->notebook);
    mapName = EspMapCatalog_nameForId(view->targetMapId);
    if (mapName == NULL || !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        state->busy = 0U;
        return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED;
    }

    memset(&mapEntry, 0, sizeof(mapEntry));
    applyStatus = ESP_MAP_NOTEBOOK_APPLY_IO_ERROR;
    if (EspAssetPack_findEntry(mapName, &mapEntry)) {
        applyStatus = EspMapNotebook_apply(&state->candidate,
                                           &mapEntry,
                                           &noteIntent,
                                           state->scratch,
                                           sizeof(state->scratch),
                                           &readLength);
    }
    EspAssetPack_close();
    if (applyStatus != ESP_MAP_NOTEBOOK_APPLY_OK || EspAssetPack_isOpen()) {
        printf("[NOTE] DEFER event=%u cmd=%u string=%u apply=%d bytes=%u mutation=no\n",
               (unsigned int)eventIndex,
               (unsigned int)noteIntent.sourceCommandOffset,
               (unsigned int)noteIntent.text.index,
               (int)applyStatus,
               (unsigned int)readLength);
        state->busy = 0U;
        return applyStatus == ESP_MAP_NOTEBOOK_APPLY_BUFFER_TOO_SMALL
                   ? ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_TEXT_TOO_LARGE
                   : ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED;
    }

    if (removeIfHandled != 0U && removedBefore == 0U) {
        if (!EspMapScriptState_setCommandRemoved(global, 1U)) {
            state->busy = 0U;
            return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_INVALID;
        }
        removedChanged = 1;
    }

    dialogStatus = beginWithChainPreflight(eventIndex, commandOffset, runFlags);
    if (dialogStatus != ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK) {
        if (removedChanged != 0 &&
            !EspMapScriptState_setCommandRemoved(global, removedBefore)) {
            printf("[NOTE] FAILED rollback event=%u cmd=%u global=%u\n",
                   (unsigned int)eventIndex,
                   (unsigned int)noteIntent.sourceCommandOffset,
                   (unsigned int)global);
        }
        state->busy = 0U;
        return dialogStatus;
    }

    state->notebook = state->candidate;
    printf("[NOTE] APPEND event=%u cmd=%u global=%u string=%u bytes=%u len=%u->%u removed=%u->%u ownerBytes=%u dialogCmd=%u commit=dialog-open\n",
           (unsigned int)eventIndex,
           (unsigned int)noteIntent.sourceCommandOffset,
           (unsigned int)global,
           (unsigned int)noteIntent.text.index,
           (unsigned int)readLength,
           (unsigned int)beforeLength,
           (unsigned int)EspMapNotebook_length(&state->notebook),
           (unsigned int)removedBefore,
           (unsigned int)(removedChanged != 0 ? 1U : removedBefore),
           (unsigned int)sizeof(EspMapNotebookState),
           (unsigned int)commandOffset);
    state->busy = 0U;
    return dialogStatus;
}
