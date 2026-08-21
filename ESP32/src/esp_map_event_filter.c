#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_map_event_filter.h"
#include "esp_map_events.h"

static uint32_t stateArgMask(uint8_t currentState) {
    if (currentState == 0U) {
        return 0U;
    }
    return 0x00010000UL << (currentState - 1U);
}

static uint32_t injectKeyFlag(uint32_t inputFlags, uint32_t playerKeys) {
    uint32_t flags = inputFlags;

    if ((playerKeys & ESP_MAP_PLAYER_KEY_RED) != 0U) {
        flags |= ESP_MAP_RUN_KEY_RED;
    }
    else if ((playerKeys & ESP_MAP_PLAYER_KEY_BLUE) != 0U) {
        flags |= ESP_MAP_RUN_KEY_BLUE;
    }
    else if ((playerKeys & ESP_MAP_PLAYER_KEY_GREEN) != 0U) {
        flags |= ESP_MAP_RUN_KEY_GREEN;
    }
    else if ((playerKeys & ESP_MAP_PLAYER_KEY_YELLOW) != 0U) {
        flags |= ESP_MAP_RUN_KEY_YELLOW;
    }
    return flags;
}

int EspMapEventFilter_prepare(const EspMapEventDescriptor* descriptor,
                              uint8_t currentState,
                              uint32_t startCommandOffset,
                              uint32_t inputFlags,
                              uint32_t playerKeys,
                              EspMapEventFilterPlan* outPlan) {
    if (descriptor == NULL || outPlan == NULL || currentState > 15U ||
        startCommandOffset > descriptor->commandCount) {
        return 0;
    }

    memset(outPlan, 0, sizeof(*outPlan));
    outPlan->inputFlags = inputFlags;
    outPlan->effectiveFlags = injectKeyFlag(inputFlags, playerKeys);
    outPlan->stateArgMask = stateArgMask(currentState);
    outPlan->eventIndex = descriptor->eventIndex;
    outPlan->startCommandOffset = (uint16_t)startCommandOffset;
    outPlan->currentState = currentState;
    outPlan->eventBlocked =
        (uint8_t)(((descriptor->flags & ESP_MAP_EVENT_FLAG_BLOCK_INPUT) != 0U &&
                   (inputFlags & ESP_MAP_EVENT_BLOCK_INPUT_RUN_FLAG) != 0U) ? 1U : 0U);
    return 1;
}

int EspMapEventFilter_evaluate(const EspMapEventDescriptor* descriptor,
                               const EspMapEventFilterPlan* plan,
                               uint32_t commandOffset,
                               uint8_t removed,
                               EspMapEventCommandFilterResult* outResult) {
    EspMapByteCode command;
    uint32_t keyBits;
    uint32_t effectiveKeyBits;

    if (descriptor == NULL || plan == NULL || outResult == NULL ||
        plan->eventIndex != descriptor->eventIndex || removed > 1U ||
        commandOffset >= descriptor->commandCount || commandOffset > 0xffU ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return 0;
    }

    memset(outResult, 0, sizeof(*outResult));
    outResult->arg2 = command.arg2;
    outResult->globalCommandIndex =
        (uint16_t)((uint32_t)descriptor->firstCommandIndex + commandOffset);
    outResult->commandOffset = (uint8_t)commandOffset;
    outResult->codeId = command.id;

    if (plan->eventBlocked != 0U) {
        outResult->decision = ESP_MAP_EVENT_COMMAND_EVENT_BLOCKED;
        return 1;
    }

    if (commandOffset < plan->startCommandOffset) {
        outResult->decision = ESP_MAP_EVENT_COMMAND_BEFORE_START;
        return 1;
    }

    if (removed != 0U) {
        outResult->decision = ESP_MAP_EVENT_COMMAND_REMOVED;
        return 1;
    }

    if (plan->stateArgMask != 0U) {
        if ((command.arg2 & plan->stateArgMask) == 0U) {
            outResult->decision = ESP_MAP_EVENT_COMMAND_STATE_MISMATCH;
            return 1;
        }
    }
    else if ((command.arg2 & ESP_MAP_EVENT_STATE_ARG_MASK) != 0U) {
        outResult->decision = ESP_MAP_EVENT_COMMAND_STATE_MISMATCH;
        return 1;
    }

    keyBits = command.arg2 & ESP_MAP_EVENT_KEY_ARG_MASK;
    effectiveKeyBits = plan->effectiveFlags & ESP_MAP_EVENT_KEY_ARG_MASK;
    if (keyBits != 0U && keyBits != effectiveKeyBits) {
        outResult->decision = ESP_MAP_EVENT_COMMAND_KEY_MISMATCH;
        return 1;
    }

    if ((plan->effectiveFlags & command.arg2) == 0U) {
        outResult->decision = ESP_MAP_EVENT_COMMAND_FLAGS_MISMATCH;
        return 1;
    }

    outResult->decision = ESP_MAP_EVENT_COMMAND_ELIGIBLE;
    return 1;
}
