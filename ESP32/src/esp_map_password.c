#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_password.h"
#include "esp_map_runtime.h"

static int sameRef(const EspMapStringRef* a, const EspMapStringRef* b) {
    return a != NULL && b != NULL &&
           a->index == b->index &&
           a->sourceOffset == b->sourceOffset &&
           a->length == b->length;
}

static int sameDescriptor(const EspMapEventDescriptor* a,
                          const EspMapEventDescriptor* b) {
    return a != NULL && b != NULL &&
           a->value == b->value &&
           a->eventIndex == b->eventIndex &&
           a->tileIndex == b->tileIndex &&
           a->firstCommandIndex == b->firstCommandIndex &&
           a->commandEndIndex == b->commandEndIndex &&
           a->commandCount == b->commandCount &&
           a->initialState == b->initialState &&
           a->flags == b->flags;
}

static int descriptorIsCanonical(const EspMapEventDescriptor* descriptor) {
    EspMapEventRef ref;
    EspMapEventDescriptor canonical;
    uint32_t value;

    if (descriptor == NULL ||
        !EspMapRuntime_getEvent(descriptor->eventIndex, &value)) {
        return 0;
    }

    ref.index = descriptor->eventIndex;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    return EspMapEvents_describe(&ref, &canonical) &&
           sameDescriptor(descriptor, &canonical);
}

static size_t boundedCStringLength(const char* text, size_t capacity) {
    size_t i;

    if (text == NULL) return 0U;
    for (i = 0U; i < capacity; ++i) {
        if (text[i] == '\0') return i;
    }
    return capacity;
}

static int ownerIsCanonical(const EspMapPasswordOwnerState* state) {
    EspMapEventRef ref;
    EspMapEventDescriptor descriptor;
    EspMapByteCode command;
    EspMapStringRef expectedCode;
    EspMapStringRef prompt;
    uint32_t eventValue;
    uint32_t globalCommandIndex;
    uint32_t codeIndex;
    uint32_t promptIndex;

    if (state == NULL || state->active != 1U ||
        state->flags != ESP_MAP_PASSWORD_EXPECTED_FLAGS ||
        !EspMapRuntime_getEvent(state->sourceEventIndex, &eventValue)) {
        return 0;
    }

    ref.index = state->sourceEventIndex;
    ref.tileIndex = (uint16_t)(eventValue & ESP_MAP_EVENT_TILE_MASK);
    ref.value = eventValue;
    if (!EspMapEvents_describe(&ref, &descriptor) ||
        state->sourceCommandOffset >= descriptor.commandCount ||
        !EspMapEvents_getCommand(&descriptor,
                                 state->sourceCommandOffset,
                                 &command) ||
        command.id != ESP_MAP_OPCODE_PASSWORD) {
        return 0;
    }

    globalCommandIndex =
        (uint32_t)descriptor.firstCommandIndex + state->sourceCommandOffset;
    if (globalCommandIndex > 0xffffU ||
        state->globalCommandIndex != (uint16_t)globalCommandIndex ||
        state->resumeCommandOffset !=
            (uint8_t)(state->sourceCommandOffset + 1U)) {
        return 0;
    }

    codeIndex = command.arg1 & 0xffU;
    promptIndex = (command.arg1 >> 8) & 0xffU;
    if (!EspMapStrings_getRef(codeIndex, &expectedCode) ||
        !EspMapStrings_getRef(promptIndex, &prompt) ||
        !sameRef(&expectedCode, &state->expectedCode) ||
        !sameRef(&prompt, &state->prompt)) {
        return 0;
    }

    return 1;
}

void EspMapPasswordOwner_reset(EspMapPasswordOwnerState* state) {
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

int EspMapPasswordOwner_isActive(const EspMapPasswordOwnerState* state) {
    return ownerIsCanonical(state);
}

EspMapPasswordOwnerStatus EspMapPasswordOwner_apply(
    EspMapPasswordOwnerState* state,
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset) {
    EspMapPasswordOwnerState next;
    EspMapByteCode command;
    EspMapStringRef expectedCode;
    EspMapStringRef prompt;
    uint32_t globalCommandIndex;
    uint32_t codeIndex;
    uint32_t promptIndex;

    if (state == NULL || descriptor == NULL ||
        !descriptorIsCanonical(descriptor) ||
        commandOffset >= descriptor->commandCount ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return ESP_MAP_PASSWORD_OWNER_INVALID;
    }
    if (command.id != ESP_MAP_OPCODE_PASSWORD) {
        return ESP_MAP_PASSWORD_OWNER_UNSUPPORTED;
    }

    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    if (globalCommandIndex > 0xffffU || commandOffset >= 0xffU) {
        return ESP_MAP_PASSWORD_OWNER_INVALID;
    }

    codeIndex = command.arg1 & 0xffU;
    promptIndex = (command.arg1 >> 8) & 0xffU;
    if (!EspMapStrings_getRef(codeIndex, &expectedCode) ||
        !EspMapStrings_getRef(promptIndex, &prompt)) {
        return ESP_MAP_PASSWORD_OWNER_INVALID;
    }

    memset(&next, 0, sizeof(next));
    next.expectedCode = expectedCode;
    next.prompt = prompt;
    next.sourceEventIndex = descriptor->eventIndex;
    next.globalCommandIndex = (uint16_t)globalCommandIndex;
    next.sourceCommandOffset = (uint8_t)commandOffset;
    next.resumeCommandOffset = (uint8_t)(commandOffset + 1U);
    next.flags = ESP_MAP_PASSWORD_EXPECTED_FLAGS;
    next.active = 1U;
    *state = next;
    return ESP_MAP_PASSWORD_OWNER_OK;
}

EspMapPasswordSubmitStatus EspMapPassword_evaluateSubmit(
    const EspAssetPackEntry* sourceEntry,
    const EspMapPasswordOwnerState* state,
    const char* submitted,
    size_t submittedLength,
    char* scratch,
    size_t scratchCapacity,
    size_t* outExpectedLength,
    EspMapPasswordSubmitResult* outResult) {
    EspMapStringReadStatus readStatus;
    size_t readLength = 0U;
    size_t expectedLength;
    int matches;

    if (outExpectedLength != NULL) {
        *outExpectedLength = 0U;
    }
    if (outResult != NULL) {
        memset(outResult, 0, sizeof(*outResult));
    }

    if (sourceEntry == NULL || state == NULL || scratch == NULL ||
        scratchCapacity == 0U || outResult == NULL ||
        !ownerIsCanonical(state) ||
        submittedLength >= ESP_MAP_PASSWORD_INPUT_CAPACITY ||
        (submittedLength != 0U && submitted == NULL) ||
        (submittedLength != 0U &&
         memchr(submitted, '\0', submittedLength) != NULL)) {
        return ESP_MAP_PASSWORD_SUBMIT_INVALID;
    }

    readStatus = EspMapStrings_read(sourceEntry,
                                    &state->expectedCode,
                                    scratch,
                                    scratchCapacity,
                                    &readLength);
    if (readStatus == ESP_MAP_STRING_READ_BUFFER_TOO_SMALL) {
        return ESP_MAP_PASSWORD_SUBMIT_BUFFER_TOO_SMALL;
    }
    if (readStatus == ESP_MAP_STRING_READ_IO_ERROR) {
        return ESP_MAP_PASSWORD_SUBMIT_IO_ERROR;
    }
    if (readStatus != ESP_MAP_STRING_READ_OK ||
        readLength != (size_t)state->expectedCode.length) {
        return ESP_MAP_PASSWORD_SUBMIT_INVALID;
    }

    expectedLength = boundedCStringLength(scratch, readLength);
    if (expectedLength >= ESP_MAP_PASSWORD_INPUT_CAPACITY) {
        return ESP_MAP_PASSWORD_SUBMIT_INVALID;
    }

    if (outExpectedLength != NULL) {
        *outExpectedLength = expectedLength;
    }

    matches = submittedLength == expectedLength &&
              (expectedLength == 0U ||
               memcmp(submitted, scratch, expectedLength) == 0);

    outResult->sourceEventIndex = state->sourceEventIndex;
    outResult->globalCommandIndex = state->globalCommandIndex;
    outResult->feedbackDelayMs =
        submittedLength == expectedLength
            ? (uint16_t)ESP_MAP_PASSWORD_MATCH_DELAY_MS
            : 0U;
    outResult->sourceCommandOffset = state->sourceCommandOffset;
    outResult->resumeCommandOffset = state->resumeCommandOffset;
    outResult->closeDialog = 1U;

    if (matches) {
        outResult->kind = ESP_MAP_PASSWORD_OUTCOME_CORRECT;
        outResult->resumeEvent = 1U;
        outResult->forceStatusMessage = 1U;
    }
    else if (submittedLength != 0U) {
        outResult->kind = ESP_MAP_PASSWORD_OUTCOME_INCORRECT;
        outResult->forceStatusMessage = 1U;
    }
    else {
        outResult->kind = ESP_MAP_PASSWORD_OUTCOME_EMPTY;
    }

    return ESP_MAP_PASSWORD_SUBMIT_OK;
}

const char* EspMapPassword_resultMessage(
    const EspMapPasswordSubmitResult* result) {
    if (result == NULL || result->closeDialog == 0U) {
        return NULL;
    }
    if (result->kind == ESP_MAP_PASSWORD_OUTCOME_CORRECT &&
        result->resumeEvent != 0U && result->forceStatusMessage != 0U) {
        return "Correct code!";
    }
    if (result->kind == ESP_MAP_PASSWORD_OUTCOME_INCORRECT &&
        result->resumeEvent == 0U && result->forceStatusMessage != 0U) {
        return "Invalid code!";
    }
    return NULL;
}
