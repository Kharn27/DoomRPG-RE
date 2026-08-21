#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_ui_intent.h"
#include "esp_map_runtime.h"

int EspMapUiIntent_supports(uint8_t codeId) {
    return codeId == ESP_MAP_OPCODE_DIALOG ||
           codeId == ESP_MAP_OPCODE_FORCE_MESSAGE ||
           codeId == ESP_MAP_OPCODE_DIALOG_NO_BACK ||
           codeId == ESP_MAP_OPCODE_NOTE;
}

EspMapUiIntentStatus EspMapUiIntent_build(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapUiIntent* outIntent) {
    EspMapByteCode command;
    EspMapStringRef text;
    uint32_t globalCommandIndex;

    if (outIntent != NULL) {
        memset(outIntent, 0, sizeof(*outIntent));
    }
    if (descriptor == NULL || outIntent == NULL ||
        commandOffset >= descriptor->commandCount ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return ESP_MAP_UI_INTENT_INVALID;
    }

    outIntent->arg1 = command.arg1;
    outIntent->arg2 = command.arg2;
    outIntent->sourceEventIndex = descriptor->eventIndex;
    outIntent->sourceCommandOffset = (uint8_t)commandOffset;
    outIntent->resumeCommandOffset = (uint8_t)(commandOffset + 1U);
    outIntent->codeId = command.id;

    globalCommandIndex = (uint32_t)descriptor->firstCommandIndex + commandOffset;
    if (globalCommandIndex > 0xffffU) {
        return ESP_MAP_UI_INTENT_INVALID;
    }
    outIntent->globalCommandIndex = (uint16_t)globalCommandIndex;

    if (!EspMapUiIntent_supports(command.id)) {
        outIntent->status = ESP_MAP_UI_INTENT_UNSUPPORTED;
        return ESP_MAP_UI_INTENT_UNSUPPORTED;
    }
    if (!EspMapStrings_getRef(command.arg1, &text)) {
        outIntent->status = ESP_MAP_UI_INTENT_STRING_NOT_FOUND;
        return ESP_MAP_UI_INTENT_STRING_NOT_FOUND;
    }
    outIntent->text = text;

    if (command.id == ESP_MAP_OPCODE_DIALOG ||
        command.id == ESP_MAP_OPCODE_DIALOG_NO_BACK) {
        outIntent->kind = ESP_MAP_UI_INTENT_DIALOG;
        outIntent->flags = ESP_MAP_UI_INTENT_FLAG_PAUSE_SCRIPT |
                           ESP_MAP_UI_INTENT_FLAG_SKIP_ADVANCE_TURN;
        if (command.id == ESP_MAP_OPCODE_DIALOG) {
            outIntent->flags |= ESP_MAP_UI_INTENT_FLAG_DIALOG_BACK;
        }
    }
    else if (command.id == ESP_MAP_OPCODE_FORCE_MESSAGE) {
        outIntent->kind = ESP_MAP_UI_INTENT_FORCE_MESSAGE;
        outIntent->flags = ESP_MAP_UI_INTENT_FLAG_CLEAR_IF_EMPTY;
    }
    else {
        outIntent->kind = ESP_MAP_UI_INTENT_APPEND_NOTE;
        outIntent->flags = ESP_MAP_UI_INTENT_FLAG_APPEND_NOTE_SEPARATOR;
    }

    outIntent->status = ESP_MAP_UI_INTENT_OK;
    return ESP_MAP_UI_INTENT_OK;
}
