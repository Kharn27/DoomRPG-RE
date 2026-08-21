#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_status_message.h"
#include "esp_map_strings.h"

void EspMapStatusMessage_reset(EspMapStatusMessageState* state) {
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

int EspMapStatusMessage_isActive(const EspMapStatusMessageState* state) {
    return state != NULL && state->active != 0U;
}

int EspMapStatusMessage_getRef(const EspMapStatusMessageState* state,
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

EspMapStatusMessageApplyStatus EspMapStatusMessage_apply(
    EspMapStatusMessageState* state,
    const EspAssetPackEntry* sourceEntry,
    const EspMapUiIntent* intent,
    char* scratch,
    size_t scratchCapacity,
    size_t* outReadLength) {
    EspMapStringReadStatus readStatus;
    size_t readLength = 0U;

    if (outReadLength != NULL) {
        *outReadLength = 0U;
    }

    if (state == NULL || sourceEntry == NULL || intent == NULL ||
        scratch == NULL || scratchCapacity == 0U) {
        return ESP_MAP_STATUS_MESSAGE_APPLY_INVALID;
    }
    if (intent->codeId != ESP_MAP_OPCODE_FORCE_MESSAGE) {
        return ESP_MAP_STATUS_MESSAGE_APPLY_UNSUPPORTED;
    }
    if (intent->status != ESP_MAP_UI_INTENT_OK ||
        intent->kind != ESP_MAP_UI_INTENT_FORCE_MESSAGE ||
        intent->flags != ESP_MAP_UI_INTENT_FLAG_CLEAR_IF_EMPTY ||
        intent->arg1 != (uint32_t)intent->text.index) {
        return ESP_MAP_STATUS_MESSAGE_APPLY_INVALID;
    }

    readStatus = EspMapStrings_read(sourceEntry,
                                    &intent->text,
                                    scratch,
                                    scratchCapacity,
                                    &readLength);
    if (readStatus == ESP_MAP_STRING_READ_BUFFER_TOO_SMALL) {
        return ESP_MAP_STATUS_MESSAGE_APPLY_BUFFER_TOO_SMALL;
    }
    if (readStatus == ESP_MAP_STRING_READ_IO_ERROR) {
        return ESP_MAP_STATUS_MESSAGE_APPLY_IO_ERROR;
    }
    if (readStatus != ESP_MAP_STRING_READ_OK ||
        readLength != (size_t)intent->text.length) {
        return ESP_MAP_STATUS_MESSAGE_APPLY_INVALID;
    }

    if (scratch[0] == '\0') {
        EspMapStatusMessage_reset(state);
    }
    else {
        state->text = intent->text;
        state->active = 1U;
        state->reserved = 0U;
    }

    if (outReadLength != NULL) {
        *outReadLength = readLength;
    }
    return ESP_MAP_STATUS_MESSAGE_APPLY_OK;
}
