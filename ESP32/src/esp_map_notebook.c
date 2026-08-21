#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_events.h"
#include "esp_map_notebook.h"
#include "esp_map_runtime.h"
#include "esp_map_strings.h"
#include "esp_map_ui_intent.h"

static int sameRef(const EspMapStringRef* a, const EspMapStringRef* b) {
    return a != NULL && b != NULL &&
           a->index == b->index &&
           a->sourceOffset == b->sourceOffset &&
           a->length == b->length;
}

static size_t boundedCStringLength(const char* text, size_t capacity) {
    size_t i;

    if (text == NULL) return 0U;
    for (i = 0U; i < capacity; ++i) {
        if (text[i] == '\0') return i;
    }
    return capacity;
}

static int stateIsCanonical(const EspMapNotebookState* state) {
    if (state == NULL || state->length >= ESP_MAP_NOTEBOOK_CAPACITY ||
        state->text[state->length] != '\0') {
        return 0;
    }
    return boundedCStringLength(state->text, state->length) ==
           (size_t)state->length;
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

void EspMapNotebook_reset(EspMapNotebookState* state) {
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

uint16_t EspMapNotebook_length(const EspMapNotebookState* state) {
    return stateIsCanonical(state) ? state->length : 0U;
}

const char* EspMapNotebook_text(const EspMapNotebookState* state) {
    return stateIsCanonical(state) ? state->text : NULL;
}

EspMapNotebookApplyStatus EspMapNotebook_apply(
    EspMapNotebookState* state,
    const EspAssetPackEntry* sourceEntry,
    const EspMapUiIntent* intent,
    char* scratch,
    size_t scratchCapacity,
    size_t* outReadLength) {
    EspMapStringRef canonical;
    EspMapStringReadStatus readStatus;
    size_t readLength = 0U;
    size_t sourceTextLength;
    size_t available;
    size_t copyBytes;
    uint16_t length;

    if (outReadLength != NULL) {
        *outReadLength = 0U;
    }

    if (state == NULL || sourceEntry == NULL || intent == NULL ||
        scratch == NULL || scratchCapacity == 0U) {
        return ESP_MAP_NOTEBOOK_APPLY_INVALID;
    }
    if (intent->codeId != ESP_MAP_OPCODE_NOTE) {
        return ESP_MAP_NOTEBOOK_APPLY_UNSUPPORTED;
    }
    if (!stateIsCanonical(state) ||
        intent->status != ESP_MAP_UI_INTENT_OK ||
        intent->kind != ESP_MAP_UI_INTENT_APPEND_NOTE ||
        intent->flags != ESP_MAP_UI_INTENT_FLAG_APPEND_NOTE_SEPARATOR ||
        intent->arg1 != (uint32_t)intent->text.index ||
        !intentProvenanceIsCanonical(intent) ||
        !EspMapStrings_getRef(intent->text.index, &canonical) ||
        !sameRef(&canonical, &intent->text)) {
        return ESP_MAP_NOTEBOOK_APPLY_INVALID;
    }

    readStatus = EspMapStrings_read(sourceEntry,
                                    &intent->text,
                                    scratch,
                                    scratchCapacity,
                                    &readLength);
    if (readStatus == ESP_MAP_STRING_READ_BUFFER_TOO_SMALL) {
        return ESP_MAP_NOTEBOOK_APPLY_BUFFER_TOO_SMALL;
    }
    if (readStatus == ESP_MAP_STRING_READ_IO_ERROR) {
        return ESP_MAP_NOTEBOOK_APPLY_IO_ERROR;
    }
    if (readStatus != ESP_MAP_STRING_READ_OK ||
        readLength != (size_t)intent->text.length) {
        return ESP_MAP_NOTEBOOK_APPLY_INVALID;
    }

    sourceTextLength = boundedCStringLength(scratch, readLength);
    length = state->length;
    available = (ESP_MAP_NOTEBOOK_CAPACITY - 1U) - (size_t)length;
    copyBytes = sourceTextLength < available ? sourceTextLength : available;

    if (copyBytes != 0U) {
        memcpy(&state->text[length], scratch, copyBytes);
        length = (uint16_t)(length + copyBytes);
    }
    if ((size_t)length < ESP_MAP_NOTEBOOK_CAPACITY - 1U) {
        state->text[length++] = '|';
    }
    if ((size_t)length < ESP_MAP_NOTEBOOK_CAPACITY - 1U) {
        state->text[length++] = '|';
    }
    state->text[length] = '\0';
    state->length = length;

    if (outReadLength != NULL) {
        *outReadLength = readLength;
    }
    return ESP_MAP_NOTEBOOK_APPLY_OK;
}
