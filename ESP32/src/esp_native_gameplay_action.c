#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_automap_state.h"
#include "esp_map_event_filter.h"
#include "esp_map_events.h"
#include "esp_map_line_state.h"
#include "esp_map_script_state.h"
#include "esp_map_ui_intent.h"
#include "esp_native_gameplay_action.h"
#include "esp_native_gameplay_event_chain.h"
#include "esp_native_door_animator.h"
#include "esp_native_gameplay_password.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_select.h"

#define SELECT_REMOVE_FLAG 0x00000200UL

typedef enum SelectFamily_e {
    SELECT_FAMILY_NONE = 0,
    SELECT_FAMILY_DOOR = 1,
    SELECT_FAMILY_DIALOG = 2,
    SELECT_FAMILY_PASSWORD = 3,
    SELECT_FAMILY_CHAIN = 4,
    SELECT_FAMILY_KEY_DOOR = 5
} SelectFamily;

static int descriptorMatchesSelect(
    const EspMapEventDescriptor* descriptor,
    const EspNativeGameplaySelectResult* select) {
    return descriptor != NULL && select != NULL &&
           descriptor->eventIndex == select->eventIndex &&
           descriptor->tileIndex == select->frontTile &&
           descriptor->firstCommandIndex == select->firstCommandIndex &&
           descriptor->commandEndIndex == select->commandEndIndex &&
           descriptor->commandCount == select->commandCount;
}

static int isDoorOpcode(uint8_t codeId) {
    return codeId == ESP_MAP_OPCODE_MOVELINE ||
           codeId == ESP_MAP_OPCODE_OPENLINE ||
           codeId == ESP_MAP_OPCODE_CLOSELINE ||
           codeId == ESP_MAP_OPCODE_MOVELINE2;
}

static int isDialogOpcode(uint8_t codeId) {
    return codeId == ESP_MAP_OPCODE_DIALOG ||
           codeId == ESP_MAP_OPCODE_DIALOG_NO_BACK;
}

static int isNoteOpcode(uint8_t codeId) {
    return codeId == ESP_MAP_OPCODE_NOTE;
}

static int isPasswordOpcode(uint8_t codeId) {
    return codeId == ESP_MAP_OPCODE_PASSWORD;
}

static int isSynchronousChainEntry(uint8_t codeId) {
    return codeId == ESP_MAP_OPCODE_GIVEMAP;
}

static int decodeKeyCheck(uint32_t arg1, uint8_t* outKeyId,
                          uint8_t* outKeyMask) {
    static const uint8_t masks[4] = {
        ESP_MAP_PLAYER_KEY_GREEN,
        ESP_MAP_PLAYER_KEY_YELLOW,
        ESP_MAP_PLAYER_KEY_BLUE,
        ESP_MAP_PLAYER_KEY_RED
    };
    if (outKeyId == NULL || outKeyMask == NULL || arg1 >= 4U) return 0;
    *outKeyId = (uint8_t)arg1;
    *outKeyMask = masks[arg1];
    return 1;
}

static void copyDoorSummary(EspNativeGameplayActionResult* result,
                            const EspNativeGameplayActionDoorStep* step) {
    if (result == NULL || step == NULL) return;
    result->globalCommandIndex = step->globalCommandIndex;
    result->lineIndex = step->lineIndex;
    result->soundId = step->soundId;
    result->commandOffset = step->commandOffset;
    result->codeId = step->codeId;
    result->openBefore = step->openBefore;
    result->openAfter = step->openAfter;
    result->locked = step->locked;
    result->effectFlags = step->effectFlags;
    result->removedBefore = step->removedBefore;
    result->removedAfter = step->removedAfter;
    result->removeIfHandled = step->removeIfHandled;
}

static void rollbackDoorPrefix(EspNativeGameplayActionResult* result,
                               uint8_t committedCount) {
    int i;
    if (result == NULL) return;
    for (i = (int)committedCount - 1; i >= 0; --i) {
        EspNativeGameplayActionDoorStep* step = &result->doors[i];
        if (step->removedAfter != step->removedBefore) {
            (void)EspMapScriptState_setCommandRemoved(step->globalCommandIndex,
                                                       step->removedBefore);
            step->removedAfter = step->removedBefore;
        }
        (void)EspMapLineState_setOpen(step->lineIndex, step->openBefore);
    }
    /*
     * The wrapped door applier may already have armed one or more visual slots.
     * Restoring the semantic line bits makes those leases stale; validation
     * cancels the complete visual batch without touching world state.
     */
    (void)EspNativeDoorAnimator_validateLineState();
}

EspNativeGameplayActionStatus EspNativeGameplayAction_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult) {
    EspNativeGameplaySelectResult select;
    EspNativeGameplaySelectStatus selectStatus;
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    EspMapEventFilterPlan plan;
    EspMapEventCommandFilterResult filtered;
    const EspNativeGameplayPlayerState* player;
    SelectFamily family = SELECT_FAMILY_NONE;
    uint32_t selectedOffset = UINT32_MAX;
    uint16_t selectedGlobal = 0U;
    uint8_t selectedRemoved = 0U;
    uint8_t selectedCodeId = 0U;
    uint8_t eligibleCount = 0U;
    uint8_t notePrefixEligible = 0U;
    uint32_t offset;

    if (outResult == NULL) return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    memset(outResult, 0, sizeof(*outResult));
    if (intent == NULL || intent->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        intent->active != 1U || intent->pending != 1U ||
        intent->sequence == 0U) {
        return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    }
    if (!EspMapScriptState_isReady() || !EspMapLineState_isReady()) {
        return ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY;
    }
    player = EspNativeGameplayPlayerState_view();
    if (player == NULL || player->active != 1U) {
        return ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY;
    }

    memset(&select, 0, sizeof(select));
    selectStatus = EspNativeGameplaySelect_resolve(intent, &select);
    outResult->sequence = intent->sequence;
    outResult->frontTile = select.frontTile;
    outResult->eventIndex = select.eventIndex;

    if (selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_OUT_OF_BOUNDS ||
        selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_NO_TILE_EVENT) {
        return ESP_NATIVE_GAMEPLAY_ACTION_NO_EVENT;
    }
    if (selectStatus == ESP_NATIVE_GAMEPLAY_SELECT_NOT_READY) {
        return ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY;
    }
    if (selectStatus != ESP_NATIVE_GAMEPLAY_SELECT_TILE_EVENT ||
        select.eventFound != 1U) {
        return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    }

    memset(&eventRef, 0, sizeof(eventRef));
    memset(&descriptor, 0, sizeof(descriptor));
    if (!EspMapEvents_findByTile(select.frontTile, &eventRef) ||
        eventRef.index != select.eventIndex ||
        !EspMapEvents_describe(&eventRef, &descriptor) ||
        !descriptorMatchesSelect(&descriptor, &select) ||
        !EspMapEventFilter_prepare(&descriptor,
                                   select.currentState,
                                   0U,
                                   ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS,
                                   player->keys,
                                   &plan)) {
        return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    }

    /*
     * Match Game_runEvent's command walk. Dialog/password still stop at their
     * pause boundary. A line-only event may contain up to eight eligible door
     * commands, matching the legacy/native openDoors[8] visual batch.
     */
    for (offset = 0U; offset < descriptor.commandCount; ++offset) {
        uint32_t global = (uint32_t)descriptor.firstCommandIndex + offset;
        uint8_t removed;

        if (global > UINT16_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(&descriptor, &plan, offset,
                                        removed, &filtered)) {
            return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
        }
        if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;

        if (eligibleCount != UINT8_MAX) ++eligibleCount;
        outResult->eligibleCount = eligibleCount;

        if (selectedOffset == UINT32_MAX) {
            if (isNoteOpcode(filtered.codeId)) {
                EspMapUiIntent noteIntent;
                if (notePrefixEligible != 0U || family != SELECT_FAMILY_NONE) {
                    outResult->unsupportedCodeId = filtered.codeId;
                    return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
                }
                memset(&noteIntent, 0, sizeof(noteIntent));
                if (EspMapUiIntent_build(&descriptor, offset, &noteIntent) !=
                        ESP_MAP_UI_INTENT_OK ||
                    noteIntent.kind != ESP_MAP_UI_INTENT_APPEND_NOTE ||
                    noteIntent.codeId != ESP_MAP_OPCODE_NOTE) {
                    outResult->unsupportedCodeId = filtered.codeId;
                    return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
                }
                notePrefixEligible = 1U;
                continue;
            }

            selectedOffset = offset;
            selectedGlobal = filtered.globalCommandIndex;
            selectedRemoved = removed;
            selectedCodeId = filtered.codeId;
            if (isDoorOpcode(filtered.codeId)) {
                if (notePrefixEligible != 0U) {
                    outResult->unsupportedCodeId = filtered.codeId;
                    return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
                }
                family = SELECT_FAMILY_DOOR;
            }
            else if (isDialogOpcode(filtered.codeId)) {
                if ((filtered.arg2 & SELECT_REMOVE_FLAG) != 0U) {
                    outResult->unsupportedCodeId = filtered.codeId;
                    return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
                }
                family = SELECT_FAMILY_DIALOG;
                break;
            }
            else if (isPasswordOpcode(filtered.codeId)) {
                if (notePrefixEligible != 0U) {
                    outResult->unsupportedCodeId = filtered.codeId;
                    return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
                }
                family = SELECT_FAMILY_PASSWORD;
                break;
            }
            else if (filtered.codeId == ESP_MAP_OPCODE_CHECK_KEY) {
                EspMapByteCode keyCommand;
                uint8_t keyId;
                uint8_t keyMask;
                if (notePrefixEligible != 0U ||
                    (filtered.arg2 & SELECT_REMOVE_FLAG) != 0U ||
                    !EspMapEvents_getCommand(&descriptor, offset, &keyCommand) ||
                    keyCommand.id != ESP_MAP_OPCODE_CHECK_KEY ||
                    !decodeKeyCheck(keyCommand.arg1, &keyId, &keyMask)) {
                    outResult->unsupportedCodeId = filtered.codeId;
                    return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
                }
                outResult->keyCheckPresent = 1U;
                outResult->requiredKeyId = keyId;
                outResult->requiredKeyMask = keyMask;
                if ((player->keys & keyMask) == 0U) {
                    outResult->globalCommandIndex = selectedGlobal;
                    outResult->commandOffset = (uint8_t)selectedOffset;
                    outResult->codeId = selectedCodeId;
                    outResult->removedBefore = selectedRemoved;
                    outResult->removedAfter = selectedRemoved;
                    outResult->handled = 1U;
                    return ESP_NATIVE_GAMEPLAY_ACTION_KEY_REQUIRED;
                }
                family = SELECT_FAMILY_KEY_DOOR;
                continue;
            }
            else if (isSynchronousChainEntry(filtered.codeId)) {
                if (notePrefixEligible != 0U || selectedOffset > UINT8_MAX) {
                    outResult->unsupportedCodeId = filtered.codeId;
                    return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
                }
                family = SELECT_FAMILY_CHAIN;
                break;
            }
            else {
                outResult->unsupportedCodeId = filtered.codeId;
                return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
            }
            continue;
        }

        if (family == SELECT_FAMILY_DOOR ||
            family == SELECT_FAMILY_KEY_DOOR) {
            uint8_t doorEligible =
                (uint8_t)(eligibleCount -
                          (family == SELECT_FAMILY_KEY_DOOR ? 1U : 0U));
            if (!isDoorOpcode(filtered.codeId)) {
                outResult->unsupportedCodeId = filtered.codeId;
                return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
            }
            if (doorEligible > ESP_NATIVE_GAMEPLAY_ACTION_MAX_DOOR_COMMANDS) {
                return ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT;
            }
            continue;
        }
        return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    }

    if (selectedOffset == UINT32_MAX || eligibleCount == 0U) {
        if (notePrefixEligible != 0U) {
            outResult->unsupportedCodeId = ESP_MAP_OPCODE_NOTE;
            return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
        }
        return ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE;
    }

    outResult->globalCommandIndex = selectedGlobal;
    outResult->commandOffset = (uint8_t)selectedOffset;
    outResult->codeId = selectedCodeId;
    outResult->removedBefore = selectedRemoved;
    outResult->removedAfter = selectedRemoved;

    if (family == SELECT_FAMILY_DIALOG) {
        if (selectedOffset > UINT8_MAX || !isDialogOpcode(selectedCodeId)) {
            return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
        }
        return ESP_NATIVE_GAMEPLAY_ACTION_DIALOG_READY;
    }

    if (family == SELECT_FAMILY_PASSWORD) {
        if (selectedOffset > UINT8_MAX || !isPasswordOpcode(selectedCodeId)) {
            return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
        }
        return ESP_NATIVE_GAMEPLAY_ACTION_PASSWORD_READY;
    }

    if (family == SELECT_FAMILY_CHAIN) {
        EspNativeGameplayEventChainPreflightStatus chainStatus;
        if (selectedOffset > UINT8_MAX ||
            !isSynchronousChainEntry(selectedCodeId)) {
            return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
        }
        chainStatus = EspNativeGameplayEventChain_preflight(
            descriptor.eventIndex, (uint8_t)selectedOffset,
            ESP_NATIVE_GAMEPLAY_SELECT_RUN_FLAGS);
        if (chainStatus == ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK) {
            return ESP_NATIVE_GAMEPLAY_ACTION_CHAIN_READY;
        }
        outResult->unsupportedCodeId = selectedCodeId;
        return chainStatus == ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_NOT_READY
                   ? ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY
                   : ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
    }

    if ((family != SELECT_FAMILY_DOOR &&
         family != SELECT_FAMILY_KEY_DOOR) ||
        eligibleCount > (uint8_t)(ESP_NATIVE_GAMEPLAY_ACTION_MAX_DOOR_COMMANDS +
                                  (family == SELECT_FAMILY_KEY_DOOR ? 1U : 0U))) {
        return ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT;
    }
    if (family == SELECT_FAMILY_KEY_DOOR && eligibleCount == 1U) {
        outResult->unsupportedCodeId = ESP_MAP_OPCODE_CHECK_KEY;
        return ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT;
    }

    /*
     * Pass 2: prove the complete line batch against the same immutable event
     * and current overlay before the first world mutation. Duplicate target
     * lines are intentionally rejected because sequential same-line semantics
     * would require an intermediate-state planner rather than an atomic batch.
     */
    outResult->doorCount = 0U;
    for (offset = 0U; offset < descriptor.commandCount; ++offset) {
        uint32_t global = (uint32_t)descriptor.firstCommandIndex + offset;
        EspMapLineDoorResult preview;
        EspMapLineDoorStatus previewStatus;
        EspNativeGameplayActionDoorStep* step;
        uint8_t removed;
        uint8_t i;

        if (global > UINT16_MAX ||
            !EspMapScriptState_isCommandRemoved(global, &removed) ||
            !EspMapEventFilter_evaluate(&descriptor, &plan, offset,
                                        removed, &filtered)) {
            return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
        }
        if (filtered.decision != ESP_MAP_EVENT_COMMAND_ELIGIBLE) continue;
        if (family == SELECT_FAMILY_KEY_DOOR && offset == selectedOffset) {
            if (filtered.codeId != ESP_MAP_OPCODE_CHECK_KEY) {
                return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
            }
            continue;
        }
        if (!isDoorOpcode(filtered.codeId) ||
            outResult->doorCount >= ESP_NATIVE_GAMEPLAY_ACTION_MAX_DOOR_COMMANDS) {
            return ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT;
        }

        memset(&preview, 0, sizeof(preview));
        previewStatus =
            EspMapLineState_previewDoorCommand(&descriptor, offset, &preview);

        step = &outResult->doors[outResult->doorCount];
        memset(step, 0, sizeof(*step));
        step->globalCommandIndex = preview.globalCommandIndex;
        step->lineIndex = preview.lineIndex;
        step->soundId = preview.soundId;
        step->commandOffset = preview.sourceCommandOffset;
        step->codeId = preview.codeId;
        step->openBefore = preview.openBefore;
        step->openAfter = preview.openAfter;
        step->locked = preview.locked;
        step->effectFlags = preview.effectFlags;
        step->removedBefore = removed;
        step->removedAfter = removed;
        step->removeIfHandled = preview.removeCommandIfHandled;

        for (i = 0U; i < outResult->doorCount; ++i) {
            if (outResult->doors[i].lineIndex == step->lineIndex) {
                return ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT;
            }
        }

        copyDoorSummary(outResult, step);
        if (previewStatus == ESP_MAP_LINE_DOOR_LOCKED) {
            return ESP_NATIVE_GAMEPLAY_ACTION_DOOR_LOCKED;
        }
        if (previewStatus == ESP_MAP_LINE_DOOR_ALREADY_TARGET) {
            return ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE;
        }
        if (previewStatus == ESP_MAP_LINE_DOOR_NOT_READY) {
            return ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY;
        }
        if (previewStatus != ESP_MAP_LINE_DOOR_OK) {
            return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
        }
        ++outResult->doorCount;
    }

    if (outResult->doorCount == 0U ||
        outResult->doorCount !=
            (uint8_t)(eligibleCount -
                      (family == SELECT_FAMILY_KEY_DOOR ? 1U : 0U))) {
        return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
    }

    /*
     * Pass 3: commit in event order. The wrapped line applier may arm multiple
     * animation slots in one batch. Any failure restores every earlier line and
     * removed-bit mutation before returning.
     */
    for (offset = 0U; offset < outResult->doorCount; ++offset) {
        EspNativeGameplayActionDoorStep* step = &outResult->doors[offset];
        EspMapLineDoorResult applied;
        EspMapLineDoorStatus applyStatus;

        memset(&applied, 0, sizeof(applied));
        applyStatus = EspMapLineState_applyDoorCommand(
            &descriptor, step->commandOffset, &applied);
        if (applyStatus != ESP_MAP_LINE_DOOR_OK ||
            applied.mutated != 1U ||
            applied.globalCommandIndex != step->globalCommandIndex ||
            applied.lineIndex != step->lineIndex ||
            applied.codeId != step->codeId ||
            applied.openBefore != step->openBefore ||
            applied.openAfter != step->openAfter) {
            rollbackDoorPrefix(outResult, (uint8_t)offset);
            return applyStatus == ESP_MAP_LINE_DOOR_NOT_READY
                       ? ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY
                       : ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
        }

        if (step->removeIfHandled != 0U) {
            if (!EspMapScriptState_setCommandRemoved(
                    step->globalCommandIndex, 1U)) {
                rollbackDoorPrefix(outResult, (uint8_t)(offset + 1U));
                return ESP_NATIVE_GAMEPLAY_ACTION_INVALID;
            }
            step->removedAfter = 1U;
        }
    }

    copyDoorSummary(outResult, &outResult->doors[0]);
    outResult->handled = 1U;
    outResult->mutated = 1U;
    outResult->rollbackAvailable = 1U;
    return ESP_NATIVE_GAMEPLAY_ACTION_DOOR_OK;
}

int EspNativeGameplayAction_rollbackSelect(
    const EspNativeGameplayActionResult* result) {
    uint8_t i;

    if (result == NULL || result->handled != 1U || result->mutated != 1U ||
        result->rollbackAvailable != 1U || result->doorCount == 0U ||
        result->doorCount > ESP_NATIVE_GAMEPLAY_ACTION_MAX_DOOR_COMMANDS) {
        return 0;
    }

    for (i = 0U; i < result->doorCount; ++i) {
        const EspNativeGameplayActionDoorStep* step = &result->doors[i];
        uint8_t openNow;
        uint8_t removedNow;
        if (!isDoorOpcode(step->codeId) ||
            !EspMapLineState_getOpen(step->lineIndex, &openNow) ||
            !EspMapScriptState_isCommandRemoved(step->globalCommandIndex,
                                                 &removedNow) ||
            openNow != step->openAfter ||
            removedNow != step->removedAfter) {
            return 0;
        }
    }

    for (i = result->doorCount; i != 0U; --i) {
        const EspNativeGameplayActionDoorStep* step = &result->doors[i - 1U];
        if (step->removedAfter != step->removedBefore &&
            !EspMapScriptState_setCommandRemoved(step->globalCommandIndex,
                                                  step->removedBefore)) {
            return 0;
        }
        if (!EspMapLineState_setOpen(step->lineIndex, step->openBefore)) {
            return 0;
        }
    }

    (void)EspNativeDoorAnimator_validateLineState();

    for (i = 0U; i < result->doorCount; ++i) {
        const EspNativeGameplayActionDoorStep* step = &result->doors[i];
        uint8_t openNow;
        uint8_t removedNow;
        if (!EspMapLineState_getOpen(step->lineIndex, &openNow) ||
            !EspMapScriptState_isCommandRemoved(step->globalCommandIndex,
                                                 &removedNow) ||
            openNow != step->openBefore ||
            removedNow != step->removedBefore) {
            return 0;
        }
    }
    return 1;
}

const char* EspNativeGameplayAction_statusName(
    EspNativeGameplayActionStatus status) {
    switch (status) {
    case ESP_NATIVE_GAMEPLAY_ACTION_INVALID: return "INVALID";
    case ESP_NATIVE_GAMEPLAY_ACTION_NOT_READY: return "NOT_READY";
    case ESP_NATIVE_GAMEPLAY_ACTION_NO_EVENT: return "NO_EVENT";
    case ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE: return "NO_ELIGIBLE";
    case ESP_NATIVE_GAMEPLAY_ACTION_UNSUPPORTED_EVENT: return "UNSUPPORTED_EVENT";
    case ESP_NATIVE_GAMEPLAY_ACTION_COMPLEX_EVENT: return "COMPLEX_EVENT";
    case ESP_NATIVE_GAMEPLAY_ACTION_DOOR_LOCKED: return "DOOR_LOCKED";
    case ESP_NATIVE_GAMEPLAY_ACTION_DOOR_ALREADY_TARGET: return "DOOR_ALREADY_TARGET";
    case ESP_NATIVE_GAMEPLAY_ACTION_DOOR_OK: return "DOOR_OK";
    case ESP_NATIVE_GAMEPLAY_ACTION_DIALOG_READY: return "DIALOG_READY";
    case ESP_NATIVE_GAMEPLAY_ACTION_PASSWORD_READY: return "PASSWORD_READY";
    case ESP_NATIVE_GAMEPLAY_ACTION_CHAIN_READY: return "CHAIN_READY";
    case ESP_NATIVE_GAMEPLAY_ACTION_KEY_REQUIRED: return "KEY_REQUIRED";
    default: return "UNKNOWN";
    }
}
