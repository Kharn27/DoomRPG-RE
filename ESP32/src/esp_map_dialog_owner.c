#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_dialog_owner.h"
#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_strings.h"
#include "esp_map_ui_intent.h"

static int sameRef(const EspMapStringRef* a, const EspMapStringRef* b) {
    return a != NULL && b != NULL &&
           a->index == b->index &&
           a->sourceOffset == b->sourceOffset &&
           a->length == b->length;
}

static int intentProvenanceIsCanonical(const EspMapUiIntent* intent) {
    EspMapEventRef eventRef;
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;
    uint32_t eventValue;
    uint32_t globalCommandIndex;

    if (intent == NULL ||
        !EspMapRuntime_getEvent(intent->sourceEventIndex, &eventValue)) {
        return 0;
    }

    eventRef.index = intent->sourceEventIndex;
    eventRef.tileIndex = (uint16_t)(eventValue & ESP_MAP_EVENT_TILE_MASK);
    eventRef.value = eventValue;
    if (!EspMapEvents_describe(&eventRef, &descriptor) ||
        intent->sourceCommandOffset >= descriptor.commandCount ||
        !EspMapEvents_getCommand(&descriptor,
                                 intent->sourceCommandOffset,
                                 &command)) {
        return 0;
    }

    globalCommandIndex =
        (uint32_t)descriptor.firstCommandIndex + intent->sourceCommandOffset;
    return globalCommandIndex <= 0xffffU &&
           intent->globalCommandIndex == (uint16_t)globalCommandIndex &&
           command.id == intent->codeId &&
           command.arg1 == intent->arg1 &&
           command.arg2 == intent->arg2;
}

void EspMapDialogOwner_reset(EspMapDialogOwnerState* state) {
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

int EspMapDialogOwner_isActive(const EspMapDialogOwnerState* state) {
    return state != NULL && state->active != 0U;
}

int EspMapDialogOwner_getRef(const EspMapDialogOwnerState* state,
                             EspMapStringRef* outRef) {
    if (outRef != NULL) {
        memset(outRef, 0, sizeof(*outRef));
    }
    if (state == NULL || outRef == NULL || state->active == 0U) {
        return 0;
    }

    *outRef = state->text;
    return 1;
}

EspMapDialogOwnerApplyStatus EspMapDialogOwner_apply(
    EspMapDialogOwnerState* state,
    const EspMapUiIntent* intent) {
    EspMapStringRef canonical;
    uint8_t expectedFlags;

    if (state == NULL || intent == NULL) {
        return ESP_MAP_DIALOG_OWNER_APPLY_INVALID;
    }
    if (intent->codeId != ESP_MAP_OPCODE_DIALOG &&
        intent->codeId != ESP_MAP_OPCODE_DIALOG_NO_BACK) {
        return ESP_MAP_DIALOG_OWNER_APPLY_UNSUPPORTED;
    }

    expectedFlags = ESP_MAP_UI_INTENT_FLAG_PAUSE_SCRIPT |
                    ESP_MAP_UI_INTENT_FLAG_SKIP_ADVANCE_TURN;
    if (intent->codeId == ESP_MAP_OPCODE_DIALOG) {
        expectedFlags |= ESP_MAP_UI_INTENT_FLAG_DIALOG_BACK;
    }

    if (intent->status != ESP_MAP_UI_INTENT_OK ||
        intent->kind != ESP_MAP_UI_INTENT_DIALOG ||
        intent->flags != expectedFlags ||
        intent->arg1 != (uint32_t)intent->text.index ||
        (uint16_t)intent->sourceCommandOffset + 1U !=
            (uint16_t)intent->resumeCommandOffset ||
        !intentProvenanceIsCanonical(intent) ||
        !EspMapStrings_getRef(intent->text.index, &canonical) ||
        !sameRef(&canonical, &intent->text)) {
        return ESP_MAP_DIALOG_OWNER_APPLY_INVALID;
    }

    state->text = intent->text;
    state->sourceEventIndex = intent->sourceEventIndex;
    state->sourceCommandOffset = intent->sourceCommandOffset;
    state->resumeCommandOffset = intent->resumeCommandOffset;
    state->flags = intent->flags;
    state->active = 1U;
    return ESP_MAP_DIALOG_OWNER_APPLY_OK;
}
