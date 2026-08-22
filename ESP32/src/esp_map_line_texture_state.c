#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "esp_map_line_texture_state.h"
#include "esp_map_runtime.h"

static uint8_t* textureStateStorage;
static EspMapLineTextureStateView textureStateView;

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

    if (value != 0U) bits[index >> 3] = (uint8_t)(bits[index >> 3] | mask);
    else bits[index >> 3] = (uint8_t)(bits[index >> 3] & (uint8_t)~mask);
}

static void refreshFNV(void) {
    textureStateView.stateFNV1a =
        textureStateStorage != NULL && textureStateView.storageBytes != 0U
            ? fnv1a32(textureStateStorage, textureStateView.storageBytes)
            : 0U;
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
        !EspMapRuntime_getEvent(descriptor->eventIndex, &value)) return 0;

    ref.index = descriptor->eventIndex;
    ref.tileIndex = (uint16_t)(value & ESP_MAP_EVENT_TILE_MASK);
    ref.value = value;
    return EspMapEvents_describe(&ref, &canonical) &&
           sameDescriptor(descriptor, &canonical);
}

void EspMapLineTextureState_reset(void) {
    if (textureStateStorage != NULL) {
        heap_caps_free(textureStateStorage);
        textureStateStorage = NULL;
    }
    memset(&textureStateView, 0, sizeof(textureStateView));
}

int EspMapLineTextureState_buildFromRuntime(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapLine line;
    uint32_t bitsetBytes;
    uint32_t i;
    int ok = 0;

    if (runtime == NULL || runtime->arena == NULL || runtime->lineCount == 0U ||
        runtime->lineCount > 0xffffU) {
        printf("[MAPLINETEX] FAILED native runtime/line count unavailable\n");
        return 0;
    }

    bitsetBytes = (runtime->lineCount + 7U) >> 3;
    if (bitsetBytes == 0U) {
        printf("[MAPLINETEX] FAILED bitset size lines=%u\n",
               (unsigned int)runtime->lineCount);
        return 0;
    }

    EspMapLineTextureState_reset();
    textureStateStorage =
        (uint8_t*)heap_caps_malloc(bitsetBytes, MALLOC_CAP_8BIT);
    if (textureStateStorage == NULL) {
        printf("[MAPLINETEX] FAILED allocation bytes=%u\n",
               (unsigned int)bitsetBytes);
        return 0;
    }
    memset(textureStateStorage, 0, bitsetBytes);

    textureStateView.texture10Bits = textureStateStorage;
    textureStateView.lineCount = runtime->lineCount;
    textureStateView.bitsetBytes = bitsetBytes;
    textureStateView.storageBytes = bitsetBytes;

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) {
            printf("[MAPLINETEX] FAILED decode line=%u\n", (unsigned int)i);
            goto done;
        }
        if (line.texture == ESP_MAP_LINE_TEXTURE_LOCKED ||
            line.texture == ESP_MAP_LINE_TEXTURE_UNLOCKED) {
            ++textureStateView.variantCount;
            if (line.texture == ESP_MAP_LINE_TEXTURE_UNLOCKED) {
                bitSet(textureStateStorage, i, 1U);
                ++textureStateView.texture10Count;
            }
        }
    }

    refreshFNV();
    printf("[MAPLINETEX] READY lines=%u storageBytes=%u variants=%u texture10=%u stateFNV=%08x\n",
           (unsigned int)textureStateView.lineCount,
           (unsigned int)textureStateView.storageBytes,
           (unsigned int)textureStateView.variantCount,
           (unsigned int)textureStateView.texture10Count,
           (unsigned int)textureStateView.stateFNV1a);
    ok = 1;

done:
    if (!ok) EspMapLineTextureState_reset();
    return ok;
}

int EspMapLineTextureState_isReady(void) {
    return textureStateStorage != NULL &&
           textureStateView.texture10Bits == textureStateStorage &&
           textureStateView.lineCount != 0U &&
           textureStateView.bitsetBytes != 0U &&
           textureStateView.storageBytes == textureStateView.bitsetBytes;
}

const EspMapLineTextureStateView* EspMapLineTextureState_view(void) {
    return EspMapLineTextureState_isReady() ? &textureStateView : NULL;
}

int EspMapLineTextureState_getEffectiveTexture(uint32_t lineIndex,
                                               uint16_t* outTexture) {
    EspMapLine line;

    if (!EspMapLineTextureState_isReady() || outTexture == NULL ||
        lineIndex >= textureStateView.lineCount ||
        !EspMapRuntime_getLine(lineIndex, &line)) return 0;

    if (line.texture == ESP_MAP_LINE_TEXTURE_LOCKED ||
        line.texture == ESP_MAP_LINE_TEXTURE_UNLOCKED) {
        *outTexture = bitGet(textureStateStorage, lineIndex) != 0U
                          ? ESP_MAP_LINE_TEXTURE_UNLOCKED
                          : ESP_MAP_LINE_TEXTURE_LOCKED;
    }
    else {
        *outTexture = line.texture;
    }
    return 1;
}

int EspMapLineTextureState_setDoorTexture(uint32_t lineIndex,
                                          uint16_t texture) {
    EspMapLine line;
    uint8_t before10;
    uint8_t target10;

    if (!EspMapLineTextureState_isReady() ||
        lineIndex >= textureStateView.lineCount ||
        (texture != ESP_MAP_LINE_TEXTURE_LOCKED &&
         texture != ESP_MAP_LINE_TEXTURE_UNLOCKED) ||
        !EspMapRuntime_getLine(lineIndex, &line) ||
        (line.texture != ESP_MAP_LINE_TEXTURE_LOCKED &&
         line.texture != ESP_MAP_LINE_TEXTURE_UNLOCKED)) return 0;

    before10 = bitGet(textureStateStorage, lineIndex);
    target10 = (uint8_t)(texture == ESP_MAP_LINE_TEXTURE_UNLOCKED ? 1U : 0U);
    if (before10 == target10) return 1;

    bitSet(textureStateStorage, lineIndex, target10);
    if (target10 != 0U) ++textureStateView.texture10Count;
    else --textureStateView.texture10Count;
    refreshFNV();
    return 1;
}

EspMapLineUnlockStatus EspMapLineTextureState_applyUnlockCommand(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapLineUnlockResult* outResult) {
    EspMapByteCode command;
    uint32_t globalCommandIndex;
    uint32_t lineIndex;
    uint16_t textureBefore;
    uint16_t textureAfter;
    uint8_t lockedBefore;
    uint8_t lockMutated;
    uint8_t textureMutated;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (descriptor == NULL || outResult == NULL) {
        return ESP_MAP_LINE_UNLOCK_INVALID;
    }
    if (!EspMapLineState_isReady() || !EspMapLineTextureState_isReady()) {
        return ESP_MAP_LINE_UNLOCK_NOT_READY;
    }
    if (!descriptorIsCanonical(descriptor) ||
        commandOffset >= descriptor->commandCount || commandOffset > 0xffU ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return ESP_MAP_LINE_UNLOCK_INVALID;
    }
    if (command.id != ESP_MAP_OPCODE_UNLOCK) {
        return ESP_MAP_LINE_UNLOCK_UNSUPPORTED;
    }

    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    lineIndex = command.arg1;
    if (globalCommandIndex > 0xffffU || lineIndex > 0xffffU ||
        lineIndex >= textureStateView.lineCount) {
        return ESP_MAP_LINE_UNLOCK_LINE_OUT_OF_RANGE;
    }
    if (!EspMapLineState_getLocked(lineIndex, &lockedBefore) ||
        !EspMapLineTextureState_getEffectiveTexture(lineIndex, &textureBefore)) {
        return ESP_MAP_LINE_UNLOCK_INVALID;
    }

    textureAfter = textureBefore == ESP_MAP_LINE_TEXTURE_LOCKED
                       ? ESP_MAP_LINE_TEXTURE_UNLOCKED
                       : textureBefore;
    lockMutated = lockedBefore != 0U ? 1U : 0U;
    textureMutated = textureBefore == ESP_MAP_LINE_TEXTURE_LOCKED ? 1U : 0U;

    outResult->sourceEventIndex = descriptor->eventIndex;
    outResult->globalCommandIndex = (uint16_t)globalCommandIndex;
    outResult->lineIndex = (uint16_t)lineIndex;
    outResult->textureBefore = textureBefore;
    outResult->textureAfter = textureAfter;
    outResult->sourceCommandOffset = (uint8_t)commandOffset;
    outResult->lockedBefore = lockedBefore;
    outResult->lockedAfter = 0U;
    outResult->lockMutated = lockMutated;
    outResult->textureMutated = textureMutated;
    outResult->legacyReturnValue = 1U;
    outResult->removeCommandIfHandled =
        (uint8_t)((command.arg2 & ESP_MAP_COMMAND_FLAG_REMOVE) != 0U ? 1U : 0U);

    if (textureMutated != 0U &&
        !EspMapLineTextureState_setDoorTexture(
            lineIndex, ESP_MAP_LINE_TEXTURE_UNLOCKED)) {
        memset(outResult, 0, sizeof(*outResult));
        return ESP_MAP_LINE_UNLOCK_INVALID;
    }

    if (lockMutated != 0U && !EspMapLineState_setLocked(lineIndex, 0U)) {
        if (textureMutated != 0U) {
            (void)EspMapLineTextureState_setDoorTexture(
                lineIndex, ESP_MAP_LINE_TEXTURE_LOCKED);
        }
        memset(outResult, 0, sizeof(*outResult));
        return ESP_MAP_LINE_UNLOCK_INVALID;
    }

    if (textureMutated != 0U) {
        outResult->soundId = ESP_MAP_LINE_UNLOCK_SOUND;
        outResult->effectFlags = ESP_MAP_LINE_UNLOCK_EFFECT_ALL;
    }
    return ESP_MAP_LINE_UNLOCK_OK;
}
