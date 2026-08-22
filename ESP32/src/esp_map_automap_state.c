#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "esp_map_automap_state.h"
#include "esp_map_runtime.h"
#include "esp_map_state.h"

static uint8_t* automapStorage;
static EspMapAutomapStateView automapView;

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
    automapView.stateFNV1a =
        automapStorage != NULL && automapView.storageBytes != 0U
            ? fnv1a32(automapStorage, automapView.storageBytes)
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

void EspMapAutomapState_reset(void) {
    if (automapStorage != NULL) {
        heap_caps_free(automapStorage);
        automapStorage = NULL;
    }
    memset(&automapView, 0, sizeof(automapView));
}

int EspMapAutomapState_buildFromRuntime(void) {
    const EspMapRuntimeView* runtime = EspMapRuntime_view();
    EspMapLine line;
    EspMapSprite sprite;
    uint32_t lineBytes;
    uint32_t spriteBytes;
    uint32_t storageBytes;
    uint32_t i;
    uint8_t* lineBits;
    uint8_t* spriteBits;
    int ok = 0;

    if (runtime == NULL || runtime->arena == NULL ||
        runtime->lineCount == 0U || runtime->mapSpriteCount == 0U ||
        runtime->lineCount > 0xffffU || runtime->mapSpriteCount > 0xffffU) {
        printf("[MAPAUTOMAP] FAILED native runtime/count unavailable\n");
        return 0;
    }

    lineBytes = (runtime->lineCount + 7U) >> 3;
    spriteBytes = (runtime->mapSpriteCount + 7U) >> 3;
    if (lineBytes == 0U || spriteBytes == 0U ||
        lineBytes > UINT32_MAX - spriteBytes) {
        printf("[MAPAUTOMAP] FAILED bitset sizing\n");
        return 0;
    }
    storageBytes = lineBytes + spriteBytes;

    EspMapAutomapState_reset();
    automapStorage =
        (uint8_t*)heap_caps_malloc(storageBytes, MALLOC_CAP_8BIT);
    if (automapStorage == NULL) {
        printf("[MAPAUTOMAP] FAILED allocation bytes=%u\n",
               (unsigned int)storageBytes);
        return 0;
    }
    memset(automapStorage, 0, storageBytes);

    lineBits = automapStorage;
    spriteBits = automapStorage + lineBytes;
    automapView.lineRevealedBits = lineBits;
    automapView.spriteRevealedBits = spriteBits;
    automapView.lineCount = runtime->lineCount;
    automapView.spriteCount = runtime->mapSpriteCount;
    automapView.lineBitsetBytes = lineBytes;
    automapView.spriteBitsetBytes = spriteBytes;
    automapView.storageBytes = storageBytes;

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) {
            printf("[MAPAUTOMAP] FAILED line=%u\n", (unsigned int)i);
            goto done;
        }
        if ((line.flags & ESP_MAP_LINE_FLAG_AUTOMAP_REVEALED) != 0U) {
            bitSet(lineBits, i, 1U);
            ++automapView.lineRevealedCount;
        }
    }

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        if (!EspMapRuntime_getMapSprite(i, &sprite)) {
            printf("[MAPAUTOMAP] FAILED sprite=%u\n", (unsigned int)i);
            goto done;
        }
        if ((sprite.info & ESP_MAP_SPRITE_INFO_AUTOMAP_REVEALED) != 0U) {
            bitSet(spriteBits, i, 1U);
            ++automapView.spriteRevealedCount;
        }
    }

    refreshFNV();
    printf("[MAPAUTOMAP] READY lines=%u lineBytes=%u lineRevealed=%u sprites=%u spriteBytes=%u spriteRevealed=%u storageBytes=%u stateFNV=%08x\n",
           (unsigned int)automapView.lineCount,
           (unsigned int)automapView.lineBitsetBytes,
           (unsigned int)automapView.lineRevealedCount,
           (unsigned int)automapView.spriteCount,
           (unsigned int)automapView.spriteBitsetBytes,
           (unsigned int)automapView.spriteRevealedCount,
           (unsigned int)automapView.storageBytes,
           (unsigned int)automapView.stateFNV1a);
    ok = 1;

done:
    if (!ok) EspMapAutomapState_reset();
    return ok;
}

int EspMapAutomapState_isReady(void) {
    return automapStorage != NULL &&
           automapView.lineRevealedBits == automapStorage &&
           automapView.spriteRevealedBits ==
               automapStorage + automapView.lineBitsetBytes &&
           automapView.lineCount != 0U && automapView.spriteCount != 0U &&
           automapView.lineBitsetBytes != 0U &&
           automapView.spriteBitsetBytes != 0U &&
           automapView.storageBytes ==
               automapView.lineBitsetBytes + automapView.spriteBitsetBytes;
}

const EspMapAutomapStateView* EspMapAutomapState_view(void) {
    return EspMapAutomapState_isReady() ? &automapView : NULL;
}

int EspMapAutomapState_getLineRevealed(uint32_t lineIndex,
                                       uint8_t* outRevealed) {
    if (!EspMapAutomapState_isReady() || outRevealed == NULL ||
        lineIndex >= automapView.lineCount) return 0;
    *outRevealed = bitGet(automapView.lineRevealedBits, lineIndex);
    return 1;
}

int EspMapAutomapState_setLineRevealed(uint32_t lineIndex,
                                       uint8_t revealed) {
    uint8_t before;
    uint8_t* lineBits;

    if (!EspMapAutomapState_isReady() || revealed > 1U ||
        lineIndex >= automapView.lineCount) return 0;
    lineBits = automapStorage;
    before = bitGet(lineBits, lineIndex);
    if (before == revealed) return 1;
    bitSet(lineBits, lineIndex, revealed);
    if (revealed != 0U) ++automapView.lineRevealedCount;
    else --automapView.lineRevealedCount;
    refreshFNV();
    return 1;
}

int EspMapAutomapState_getSpriteRevealed(uint32_t spriteIndex,
                                         uint8_t* outRevealed) {
    if (!EspMapAutomapState_isReady() || outRevealed == NULL ||
        spriteIndex >= automapView.spriteCount) return 0;
    *outRevealed = bitGet(automapView.spriteRevealedBits, spriteIndex);
    return 1;
}

int EspMapAutomapState_setSpriteRevealed(uint32_t spriteIndex,
                                         uint8_t revealed) {
    uint8_t before;
    uint8_t* spriteBits;

    if (!EspMapAutomapState_isReady() || revealed > 1U ||
        spriteIndex >= automapView.spriteCount) return 0;
    spriteBits = automapStorage + automapView.lineBitsetBytes;
    before = bitGet(spriteBits, spriteIndex);
    if (before == revealed) return 1;
    bitSet(spriteBits, spriteIndex, revealed);
    if (revealed != 0U) ++automapView.spriteRevealedCount;
    else --automapView.spriteRevealedCount;
    refreshFNV();
    return 1;
}

EspMapGiveMapStatus EspMapAutomapState_applyGiveMapCommand(
    const EspMapEventDescriptor* descriptor,
    uint32_t commandOffset,
    EspMapGiveMapResult* outResult) {
    const EspMapRuntimeView* runtime;
    EspMapByteCode command;
    EspMapLine line;
    uint32_t globalCommandIndex;
    uint32_t i;
    uint32_t lineTargets = 0U;
    uint32_t spriteTargets;
    uint32_t entranceTargets = 0U;
    uint32_t linesMutated = 0U;
    uint32_t spritesMutated = 0U;
    uint32_t tilesMutated = 0U;
    uint8_t revealed;
    uint8_t tileFlags;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (descriptor == NULL || outResult == NULL) return ESP_MAP_GIVEMAP_INVALID;
    if (!EspMapAutomapState_isReady() || !EspMapState_isReady()) {
        return ESP_MAP_GIVEMAP_NOT_READY;
    }
    if (!descriptorIsCanonical(descriptor) ||
        commandOffset >= descriptor->commandCount || commandOffset > 0xffU ||
        !EspMapEvents_getCommand(descriptor, commandOffset, &command)) {
        return ESP_MAP_GIVEMAP_INVALID;
    }
    if (command.id != ESP_MAP_OPCODE_GIVEMAP) return ESP_MAP_GIVEMAP_UNSUPPORTED;

    runtime = EspMapRuntime_view();
    if (runtime == NULL || runtime->lineCount != automapView.lineCount ||
        runtime->mapSpriteCount != automapView.spriteCount) {
        return ESP_MAP_GIVEMAP_INVALID;
    }
    globalCommandIndex =
        (uint32_t)descriptor->firstCommandIndex + commandOffset;
    if (globalCommandIndex > 0xffffU) return ESP_MAP_GIVEMAP_INVALID;

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) return ESP_MAP_GIVEMAP_INVALID;
        if ((line.flags & ESP_MAP_LINE_FLAG_NO_AUTOMAP) == 0U) ++lineTargets;
    }
    spriteTargets = runtime->mapSpriteCount;
    for (i = 0U; i < ESP_MAP_STATE_TILE_COUNT; ++i) {
        if (!EspMapState_getTileFlags(i, &tileFlags)) return ESP_MAP_GIVEMAP_INVALID;
        if ((tileFlags & ESP_MAP_TILE_ENTRANCE) != 0U) ++entranceTargets;
    }
    if (lineTargets > 0xffffU || spriteTargets > 0xffffU ||
        entranceTargets > 0xffffU) return ESP_MAP_GIVEMAP_INVALID;

    outResult->sourceEventIndex = descriptor->eventIndex;
    outResult->globalCommandIndex = (uint16_t)globalCommandIndex;
    outResult->lineTargetCount = (uint16_t)lineTargets;
    outResult->spriteTargetCount = (uint16_t)spriteTargets;
    outResult->entranceTargetCount = (uint16_t)entranceTargets;
    outResult->sourceCommandOffset = (uint8_t)commandOffset;
    outResult->legacyReturnValue = 1U;
    outResult->removeCommandIfHandled =
        (uint8_t)((command.arg2 & ESP_MAP_GIVEMAP_COMMAND_FLAG_REMOVE) != 0U ? 1U : 0U);

    for (i = 0U; i < runtime->lineCount; ++i) {
        if (!EspMapRuntime_getLine(i, &line)) return ESP_MAP_GIVEMAP_INVALID;
        if ((line.flags & ESP_MAP_LINE_FLAG_NO_AUTOMAP) != 0U) continue;
        if (!EspMapAutomapState_getLineRevealed(i, &revealed)) {
            return ESP_MAP_GIVEMAP_INVALID;
        }
        if (revealed == 0U) {
            if (!EspMapAutomapState_setLineRevealed(i, 1U)) {
                return ESP_MAP_GIVEMAP_INVALID;
            }
            ++linesMutated;
        }
    }

    for (i = 0U; i < runtime->mapSpriteCount; ++i) {
        if (!EspMapAutomapState_getSpriteRevealed(i, &revealed)) {
            return ESP_MAP_GIVEMAP_INVALID;
        }
        if (revealed == 0U) {
            if (!EspMapAutomapState_setSpriteRevealed(i, 1U)) {
                return ESP_MAP_GIVEMAP_INVALID;
            }
            ++spritesMutated;
        }
    }

    for (i = 0U; i < ESP_MAP_STATE_TILE_COUNT; ++i) {
        if (!EspMapState_getTileFlags(i, &tileFlags)) return ESP_MAP_GIVEMAP_INVALID;
        if ((tileFlags & ESP_MAP_TILE_ENTRANCE) != 0U &&
            (tileFlags & ESP_MAP_TILE_VISITED) == 0U) {
            if (!EspMapState_setVisited(i, 1U)) return ESP_MAP_GIVEMAP_INVALID;
            ++tilesMutated;
        }
    }

    outResult->linesMutated = (uint16_t)linesMutated;
    outResult->spritesMutated = (uint16_t)spritesMutated;
    outResult->tilesMutated = (uint16_t)tilesMutated;
    outResult->mutated =
        (uint8_t)((linesMutated != 0U || spritesMutated != 0U ||
                   tilesMutated != 0U) ? 1U : 0U);
    return ESP_MAP_GIVEMAP_OK;
}
