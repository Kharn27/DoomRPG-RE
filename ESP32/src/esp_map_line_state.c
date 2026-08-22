#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "esp_map_line_state.h"
#include "esp_map_runtime.h"

static uint8_t* lineStateStorage;
static EspMapLineStateView lineStateView;

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (data == NULL && length != 0U) return 0U;
    for (i = 0U; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint8_t bitGet(const uint8_t* bits, uint32_t index) {
    return (uint8_t)((bits[index >> 3] >> (index & 7U)) & 1U);
}

static void bitSet(uint8_t* bits, uint32_t index, uint8_t value) {
    const uint8_t mask = (uint8_t)(1U << (index & 7U));

    if (value != 0U) {
        bits[index >> 3] = (uint8_t)(bits[index >> 3] | mask);
    }
    else {
        bits[index >> 3] = (uint8_t)(bits[index >> 3] & (uint8_t)~mask);
    }
}

static void refreshFNV(void) {
    if (lineStateStorage != NULL && lineStateView.storageBytes != 0U) {
        lineStateView.stateFNV1a =
            fnv1a32(lineStateStorage, lineStateView.storageBytes);
    }
    else {
        lineStateView.stateFNV1a = 0U;
    }
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

void EspMapLineState_reset(void) {
    if (lineStateStorage != NULL) {
        heap_caps_free(lineStateStorage);
        lineStateStorage = NULL;
    }
    memset(&lineStateView, 0, sizeof(lineStateView));
}

int EspMapLineState_buildFromRuntime(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapLine line;
    uint32_t bitsetBytes;
    uint32_t storageBytes;
    uint32_t i;
    uint8_t* openBits;
    uint8_t* lockedBits;
    int ok = 0;

    if (runtime == NULL || runtime->arena == NULL || runtime->lineCount == 0U ||
        runtime->lineCount > 0xffffU) {
        printf("[MAPLINESTATE] FAILED native runtime/line count unavailable\n");
        return 0;
    }

    bitsetBytes = (runtime->lineCount + 7U) >> 3;
    if (bitsetBytes == 0U || bitsetBytes > (UINT32_MAX / 2U)) {
        printf("[MAPLINESTATE] FAILED bitset size lines=%u\n",
               (unsigned int)runtime->lineCount);
        return 0;
    }
    storageBytes = bitsetBytes * 2U;

    EspMapLineState_reset();
    lineStateStorage =
        (uint8_t*)heap_caps_malloc(storageBytes, MALLOC_CAP_8BIT);
    if (lineStateStorage == NULL) {
        printf("[MAPLINESTATE] FAILED allocation bytes=%u\n",
               (unsigned int)storageBytes);
        return 0;
    }
    memset(lineStateStorage, 0, storageBytes);

    openBits = lineStateStorage;
    lockedBits = lineStateStorage + bitsetBytes;
    lineStateView.openBits = openBits;
    lineStateView.lockedBits = lockedBits;
    lineStateView.lineCount = runtime->lineCount;
    lineStateView.bitsetBytes = bitsetBytes;
    lineStateView.storageBytes = storageBytes;

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) {
            printf("[MAPLINESTATE] FAILED decode line=%u\n", (unsigned int)i);
            goto done;
        }
        if ((line.flags & ESP_MAP_LINE_FLAG_OPEN) != 0U) {
            bitSet(openBits, i, 1U);
            ++lineStateView.openCount;
        }
        if ((line.flags & ESP_MAP_LINE_FLAG_LOCKED) != 0U) {
            bitSet(lockedBits, i, 1U);
            ++lineStateView.lockedCount;
        }
    }

    refreshFNV();
    printf("[MAPLINESTATE] READY lines=%u bitsetBytes=%u storageBytes=%u open=%u locked=%u stateFNV=%08x\n",
           (unsigned int)lineStateView.lineCount,
           (unsigned int)lineStateView.bitsetBytes,
           (unsigned int)lineStateView.storageBytes,
           (unsigned int)lineStateView.openCount,
           (unsigned int)lineStateView.lockedCount,
           (unsigned int)lineStateView.stateFNV1a);
    ok = 1;

done:
    if (!ok) EspMapLineState_reset();
    return ok;
}

int EspMapLineState_isReady(void) {
    return lineStateStorage != NULL &&
           lineStateView.openBits == lineStateStorage &&
           lineStateView.lockedBits ==
               lineStateStorage + lineStateView.bitsetBytes &&
           lineStateView.lineCount != 0U &&
           lineStateView.bitsetBytes != 0U &&
           lineStateView.storageBytes == lineStateView.bitsetBytes * 2U;
}

const EspMapLineStateView* EspMapLineState_view(void) {
    return EspMapLineState_isReady() ? &lineStateView : NULL;
}

int EspMapLineState_getOpen(uint32_t lineIndex, uint8_t* outOpen) {
    if (!EspMapLineState_isReady() || outOpen == NULL ||
        lineIndex >= lineStateView.lineCount) {
        return 0;
    }
    *outOpen = bitGet(lineStateView.openBits, lineIndex);
    return 1;
}

int EspMapLineState_getLocked(uint32_t lineIndex, uint8_t* outLocked) {
    if (!EspMapLineState_isReady() || outLocked == NULL ||
        lineIndex >= lineStateView.lineCount) {
        return 0;
    }
    *outLocked = bitGet(lineStateView.lockedBits, lineIndex);
    return 1;
}

int EspMapLineState_setOpen(uint32_t lineIndex, uint8_t open) {
    uint8_t before;
    uint8_t* openBits;

    if (!EspMapLineState_isReady() || open > 1U ||
        lineIndex >= lineStateView.lineCount) {
        return 0;
    }

    openBits = lineStateStorage;
    before = bitGet(openBits, lineIndex);
    if (before == open) return 1;

    bitSet(openBits, lineIndex, open);
    if (open != 0U) ++lineStateView.openCount;
    else --lineStateView.openCount;
    refreshFNV();
    return 1;
}

int EspMapLineState_setLocked(uint32_t lineIndex, uint8_t locked) {
    uint8_t before;
    uint8_t* lockedBits;

    if (!EspMapLineState_isReady() || locked > 1U ||
        lineIndex >= lineStateView.lineCount) {
        return 0;
    }

    lockedBits = lineStateStorage + lineStateView.bitsetBytes;
    before = bitGet(lockedBits, lineIndex);
    if (before == locked) return 1;

    bitSet(lockedBits, lineIndex, locked);
    if (locked != 0U) ++lineStateView.lockedCount;
    else --lineStateView.lockedCount;
    refreshFNV();
    return 1;
}

EspMapLineDoorStatus EspMapLineState_applyDoorCommand(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapLineDoorResult* outResult) {
    EspMapByteCode command;
    uint32_t globalCommandIndex;
    uint32_t lineIndex;
    uint8_t openBefore;
    uint8_t locked;
    uint8_t targetOpen;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (descriptor == NULL || outResult == NULL) {
        return ESP_MAP_LINE_DOOR_INVALID;
    }
    if (!EspMapLineState_isReady()) {
        return ESP_MAP_LINE_DOOR_NOT_READY;
    }
    if (!descriptorIsCanonical(descriptor) ||
        commandOffset >= descriptor->commandCount || commandOffset > 0xffU ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return ESP_MAP_LINE_DOOR_INVALID;
    }
    if (command.id != ESP_MAP_OPCODE_OPENLINE &&
        command.id != ESP_MAP_OPCODE_CLOSELINE) {
        return ESP_MAP_LINE_DOOR_UNSUPPORTED;
    }

    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    lineIndex = command.arg1;
    if (globalCommandIndex > 0xffffU || lineIndex > 0xffffU ||
        lineIndex >= lineStateView.lineCount) {
        return ESP_MAP_LINE_DOOR_LINE_OUT_OF_RANGE;
    }
    if (!EspMapLineState_getOpen(lineIndex, &openBefore) ||
        !EspMapLineState_getLocked(lineIndex, &locked)) {
        return ESP_MAP_LINE_DOOR_INVALID;
    }

    outResult->sourceEventIndex = descriptor->eventIndex;
    outResult->globalCommandIndex = (uint16_t)globalCommandIndex;
    outResult->lineIndex = (uint16_t)lineIndex;
    outResult->sourceCommandOffset = (uint8_t)commandOffset;
    outResult->codeId = command.id;
    outResult->openBefore = openBefore;
    outResult->openAfter = openBefore;
    outResult->locked = locked;

    if (locked != 0U) {
        return ESP_MAP_LINE_DOOR_LOCKED;
    }

    targetOpen = command.id == ESP_MAP_OPCODE_OPENLINE ? 1U : 0U;
    if (openBefore == targetOpen) {
        return ESP_MAP_LINE_DOOR_ALREADY_TARGET;
    }

    if (!EspMapLineState_setOpen(lineIndex, targetOpen)) {
        memset(outResult, 0, sizeof(*outResult));
        return ESP_MAP_LINE_DOOR_INVALID;
    }

    outResult->openAfter = targetOpen;
    outResult->mutated = 1U;
    outResult->soundId =
        targetOpen != 0U ? ESP_MAP_LINE_SOUND_OPEN : ESP_MAP_LINE_SOUND_CLOSE;
    outResult->effectFlags = ESP_MAP_LINE_EFFECT_ALL;
    outResult->removeCommandIfHandled =
        (uint8_t)((command.arg2 & ESP_MAP_COMMAND_FLAG_REMOVE) != 0U ? 1U : 0U);
    return ESP_MAP_LINE_DOOR_OK;
}
