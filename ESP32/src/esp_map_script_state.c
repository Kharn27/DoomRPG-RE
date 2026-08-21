#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "esp_map_events.h"
#include "esp_map_runtime.h"
#include "esp_map_script_state.h"

static uint8_t* scriptStorage;
static EspMapScriptStateView scriptView;

static uint32_t eventStateBytesForCount(uint32_t count) {
    return (count + 1U) >> 1;
}

static uint32_t removedBytesForCount(uint32_t count) {
    return (count + 7U) >> 3;
}

static int setPackedEventState(uint32_t eventIndex, uint8_t state) {
    uint32_t byteIndex;
    uint8_t value;

    if (scriptStorage == NULL || eventIndex >= scriptView.eventCount ||
        state > 15U) {
        return 0;
    }

    byteIndex = eventIndex >> 1;
    value = scriptStorage[byteIndex];
    if ((eventIndex & 1U) == 0U) {
        value = (uint8_t)((value & 0xf0U) | state);
    }
    else {
        value = (uint8_t)((value & 0x0fU) | (uint8_t)(state << 4));
    }
    scriptStorage[byteIndex] = value;
    return 1;
}

void EspMapScriptState_reset(void) {
    if (scriptStorage != NULL) {
        heap_caps_free(scriptStorage);
    }
    scriptStorage = NULL;
    memset(&scriptView, 0, sizeof(scriptView));
}

int EspMapScriptState_buildFromRuntime(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapEventRef ref;
    EspMapEventDescriptor descriptor;
    uint32_t eventBytes;
    uint32_t removedBytes;
    uint32_t storageBytes;
    uint32_t raw;
    uint32_t i;

    if (scriptStorage != NULL || runtime == NULL || runtime->events == NULL ||
        runtime->byteCodes == NULL || runtime->eventCount == 0U ||
        runtime->eventCount > 0xffffU || runtime->byteCodeCount == 0U) {
        return 0;
    }

    eventBytes = eventStateBytesForCount(runtime->eventCount);
    removedBytes = removedBytesForCount(runtime->byteCodeCount);
    storageBytes = eventBytes + removedBytes;
    if (storageBytes == 0U) {
        return 0;
    }

    scriptStorage = (uint8_t*)heap_caps_malloc(storageBytes, MALLOC_CAP_8BIT);
    if (scriptStorage == NULL) {
        return 0;
    }
    memset(scriptStorage, 0, storageBytes);

    scriptView.storage = scriptStorage;
    scriptView.storageBytes = storageBytes;
    scriptView.eventStatesPacked = scriptStorage;
    scriptView.eventCount = runtime->eventCount;
    scriptView.eventStateBytes = eventBytes;
    scriptView.removedCommandBits = scriptStorage + eventBytes;
    scriptView.byteCodeCount = runtime->byteCodeCount;
    scriptView.removedCommandBytes = removedBytes;

    for (i = 0U; i < runtime->eventCount; ++i) {
        if (!EspMapRuntime_getEvent(i, &raw)) {
            EspMapScriptState_reset();
            return 0;
        }
        ref.index = (uint16_t)i;
        ref.tileIndex = (uint16_t)(raw & ESP_MAP_EVENT_TILE_MASK);
        ref.value = raw;
        if (!EspMapEvents_describe(&ref, &descriptor) ||
            !setPackedEventState(i, descriptor.initialState)) {
            EspMapScriptState_reset();
            return 0;
        }
    }

    return 1;
}

int EspMapScriptState_isReady(void) {
    return scriptStorage != NULL;
}

const EspMapScriptStateView* EspMapScriptState_view(void) {
    return scriptStorage != NULL ? &scriptView : NULL;
}

int EspMapScriptState_getEventState(uint32_t eventIndex, uint8_t* outState) {
    uint8_t packed;

    if (scriptStorage == NULL || outState == NULL ||
        eventIndex >= scriptView.eventCount) {
        return 0;
    }

    packed = scriptStorage[eventIndex >> 1];
    *outState = (uint8_t)(((eventIndex & 1U) == 0U) ?
                          (packed & 0x0fU) : ((packed >> 4) & 0x0fU));
    return 1;
}

int EspMapScriptState_setEventState(uint32_t eventIndex, uint8_t state) {
    return setPackedEventState(eventIndex, state);
}

int EspMapScriptState_isCommandRemoved(uint32_t commandIndex,
                                       uint8_t* outRemoved) {
    const uint8_t* bits;

    if (scriptStorage == NULL || outRemoved == NULL ||
        commandIndex >= scriptView.byteCodeCount) {
        return 0;
    }

    bits = scriptStorage + scriptView.eventStateBytes;
    *outRemoved = (uint8_t)((bits[commandIndex >> 3] >>
                             (commandIndex & 7U)) & 1U);
    return 1;
}

int EspMapScriptState_setCommandRemoved(uint32_t commandIndex,
                                        uint8_t removed) {
    uint8_t* bits;
    uint8_t mask;

    if (scriptStorage == NULL || commandIndex >= scriptView.byteCodeCount ||
        removed > 1U) {
        return 0;
    }

    bits = scriptStorage + scriptView.eventStateBytes;
    mask = (uint8_t)(1U << (commandIndex & 7U));
    if (removed != 0U) {
        bits[commandIndex >> 3] |= mask;
    }
    else {
        bits[commandIndex >> 3] &= (uint8_t)~mask;
    }
    return 1;
}
