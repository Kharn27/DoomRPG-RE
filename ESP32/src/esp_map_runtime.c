#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "esp_asset_pack.h"
#include "esp_map_runtime.h"

#define STRING_INDEX_WINDOW_BYTES 256U
#define RESOURCE_SET_COUNT 3U

typedef struct StringIndexCursor_s {
    const EspAssetPackEntry* entry;
    uint32_t endOffset;
    uint32_t bufferOffset;
    uint32_t bufferLength;
    uint32_t readCalls;
    uint8_t buffer[STRING_INDEX_WINDOW_BYTES];
} StringIndexCursor;

static uint8_t* runtimeArena;
static EspMapRuntimeView runtimeView;

static uint32_t minU32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static uint32_t fnv1a32(const uint8_t* data, uint32_t length) {
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static int planIsSupported(const EspBspInventory* inventory) {
    uint32_t sum;

    if (inventory == NULL ||
        inventory->sourceBytes == 0U ||
        inventory->sourceBytes > UINT16_MAX ||
        inventory->sections.endOffset != inventory->sourceBytes ||
        inventory->trailingBytes != 0U ||
        inventory->plan.stringOffsetsBytes !=
            inventory->strings * ESP_MAP_RUNTIME_STRING_OFFSET_BYTES ||
        inventory->plan.resourceSetsBytes !=
            RESOURCE_SET_COUNT * ESP_MAP_RUNTIME_RESOURCE_SET_BYTES) {
        return 0;
    }

    sum = inventory->plan.nodeRecordsBytes +
          inventory->plan.lineRecordsBytes +
          inventory->plan.mapSpriteRecordsBytes +
          inventory->plan.eventRecordsBytes +
          inventory->plan.byteCodeRecordsBytes +
          inventory->plan.stringOffsetsBytes +
          inventory->plan.blockMapBytes +
          inventory->plan.planeMapBytes +
          inventory->plan.resourceSetsBytes;

    if (sum != inventory->plan.persistentBytes ||
        inventory->sections.nodesOffset + inventory->plan.nodeRecordsBytes + 2U !=
            inventory->sections.linesOffset ||
        inventory->sections.linesOffset + inventory->plan.lineRecordsBytes + 2U !=
            inventory->sections.mapSpritesOffset ||
        inventory->sections.mapSpritesOffset + inventory->plan.mapSpriteRecordsBytes + 2U !=
            inventory->sections.eventsOffset ||
        inventory->sections.eventsOffset + inventory->plan.eventRecordsBytes + 2U !=
            inventory->sections.byteCodesOffset ||
        inventory->sections.byteCodesOffset + inventory->plan.byteCodeRecordsBytes + 2U !=
            inventory->sections.stringsOffset ||
        inventory->sections.stringsOffset >= inventory->sections.blockMapOffset ||
        inventory->sections.blockMapOffset + inventory->plan.blockMapBytes !=
            inventory->sections.planeTexturesOffset ||
        inventory->sections.planeTexturesOffset + inventory->plan.planeMapBytes !=
            inventory->sections.endOffset) {
        return 0;
    }

    return 1;
}

static int readSection(const EspAssetPackEntry* entry,
                       uint32_t sourceOffset,
                       void* destination,
                       uint32_t length,
                       uint32_t* readCalls) {
    if (length == 0U) {
        return 1;
    }

    if (entry == NULL || destination == NULL || readCalls == NULL ||
        !EspAssetPack_readRange(entry, sourceOffset, destination, length)) {
        return 0;
    }

    ++(*readCalls);
    return 1;
}

static int stringCursorReadU16(StringIndexCursor* cursor,
                               uint32_t offset,
                               uint16_t* value) {
    uint32_t remaining;
    uint32_t length;
    uint32_t local;

    if (cursor == NULL || cursor->entry == NULL || value == NULL ||
        offset + 2U > cursor->endOffset) {
        return 0;
    }

    if (cursor->bufferLength == 0U ||
        offset < cursor->bufferOffset ||
        offset + 2U > cursor->bufferOffset + cursor->bufferLength) {
        remaining = cursor->endOffset - offset;
        length = minU32(remaining, STRING_INDEX_WINDOW_BYTES);
        if (length < 2U ||
            !EspAssetPack_readRange(cursor->entry,
                                    offset,
                                    cursor->buffer,
                                    length)) {
            return 0;
        }

        cursor->bufferOffset = offset;
        cursor->bufferLength = length;
        ++cursor->readCalls;
    }

    local = offset - cursor->bufferOffset;
    *value = (uint16_t)((uint16_t)cursor->buffer[local] |
                        ((uint16_t)cursor->buffer[local + 1U] << 8));
    return 1;
}

static int buildStringOffsetTable(const EspAssetPackEntry* entry,
                                  const EspBspInventory* inventory,
                                  uint8_t* destination,
                                  uint32_t* readCalls) {
    StringIndexCursor cursor;
    uint32_t position;
    uint32_t i;
    uint16_t stringLength;
    uint32_t payloadOffset;

    if (entry == NULL || inventory == NULL || destination == NULL ||
        readCalls == NULL) {
        return 0;
    }

    memset(&cursor, 0, sizeof(cursor));
    cursor.entry = entry;
    cursor.endOffset = inventory->sections.blockMapOffset;
    position = inventory->sections.stringsOffset;

    for (i = 0; i < inventory->strings; ++i) {
        if (!stringCursorReadU16(&cursor, position, &stringLength)) {
            return 0;
        }

        payloadOffset = position + 2U;
        if (payloadOffset > UINT16_MAX ||
            payloadOffset + (uint32_t)stringLength > cursor.endOffset) {
            return 0;
        }

        destination[(i * 2U) + 0U] = (uint8_t)(payloadOffset & 0xFFU);
        destination[(i * 2U) + 1U] = (uint8_t)((payloadOffset >> 8) & 0xFFU);
        position = payloadOffset + (uint32_t)stringLength;
    }

    if (position != inventory->sections.blockMapOffset) {
        return 0;
    }

    *readCalls += cursor.readCalls;
    return 1;
}

void EspMapRuntime_reset(void) {
    if (runtimeArena != NULL) {
        heap_caps_free(runtimeArena);
        runtimeArena = NULL;
    }
    memset(&runtimeView, 0, sizeof(runtimeView));
}

int EspMapRuntime_loadPackEntry(const char* resourceName,
                                const EspBspInventory* inventory) {
    EspAssetPackEntry entry;
    uint8_t* cursor;
    uint32_t readCalls = 0U;
    int openedHere = 0;
    int ok = 0;

    if (resourceName == NULL || resourceName[0] == '\0' ||
        !planIsSupported(inventory)) {
        printf("[MAPRT] FAILED unsupported plan/source\n");
        return 0;
    }

    EspMapRuntime_reset();

    if (!EspAssetPack_isOpen()) {
        if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
            printf("[MAPRT] FAILED open %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
            return 0;
        }
        openedHere = 1;
    }

    if (!EspAssetPack_findEntry(resourceName, &entry) ||
        entry.size != inventory->sourceBytes ||
        entry.crc32 != inventory->crc32 ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U) {
        printf("[MAPRT] FAILED source mismatch file=%s size=%u/%u crc=%08x/%08x\n",
               resourceName,
               (unsigned int)entry.size,
               (unsigned int)inventory->sourceBytes,
               (unsigned int)entry.crc32,
               (unsigned int)inventory->crc32);
        goto done;
    }

    runtimeArena = (uint8_t*)heap_caps_malloc(inventory->plan.persistentBytes,
                                               MALLOC_CAP_8BIT);
    if (runtimeArena == NULL) {
        printf("[MAPRT] FAILED arena allocation bytes=%u\n",
               (unsigned int)inventory->plan.persistentBytes);
        goto done;
    }

    memset(&runtimeView, 0, sizeof(runtimeView));
    runtimeView.arena = runtimeArena;
    runtimeView.arenaBytes = inventory->plan.persistentBytes;
    runtimeView.sourceBytes = inventory->sourceBytes;
    runtimeView.sourceCrc32 = inventory->crc32;

    cursor = runtimeArena;

    runtimeView.nodes = cursor;
    runtimeView.nodeCount = inventory->nodes;
    runtimeView.nodeBytes = inventory->plan.nodeRecordsBytes;
    cursor += runtimeView.nodeBytes;

    runtimeView.lines = cursor;
    runtimeView.lineCount = inventory->lines;
    runtimeView.lineBytes = inventory->plan.lineRecordsBytes;
    cursor += runtimeView.lineBytes;

    runtimeView.mapSprites = cursor;
    runtimeView.mapSpriteCount = inventory->mapSprites;
    runtimeView.mapSpriteBytes = inventory->plan.mapSpriteRecordsBytes;
    cursor += runtimeView.mapSpriteBytes;

    runtimeView.events = cursor;
    runtimeView.eventCount = inventory->events;
    runtimeView.eventBytes = inventory->plan.eventRecordsBytes;
    cursor += runtimeView.eventBytes;

    runtimeView.byteCodes = cursor;
    runtimeView.byteCodeCount = inventory->byteCodes;
    runtimeView.byteCodeBytes = inventory->plan.byteCodeRecordsBytes;
    cursor += runtimeView.byteCodeBytes;

    runtimeView.stringOffsetsLE = cursor;
    runtimeView.stringCount = inventory->strings;
    runtimeView.stringOffsetsBytes = inventory->plan.stringOffsetsBytes;
    cursor += runtimeView.stringOffsetsBytes;

    runtimeView.blockMap = cursor;
    runtimeView.blockMapBytes = inventory->plan.blockMapBytes;
    cursor += runtimeView.blockMapBytes;

    runtimeView.planeMap = cursor;
    runtimeView.planeMapBytes = inventory->plan.planeMapBytes;
    cursor += runtimeView.planeMapBytes;

    runtimeView.textureResourceBits = cursor;
    cursor += ESP_MAP_RUNTIME_RESOURCE_SET_BYTES;
    runtimeView.spriteResourceBits = cursor;
    cursor += ESP_MAP_RUNTIME_RESOURCE_SET_BYTES;
    runtimeView.planeTextureBits = cursor;
    cursor += ESP_MAP_RUNTIME_RESOURCE_SET_BYTES;

    if ((uint32_t)(cursor - runtimeArena) != runtimeView.arenaBytes) {
        printf("[MAPRT] FAILED arena partition used=%u planned=%u\n",
               (unsigned int)(cursor - runtimeArena),
               (unsigned int)runtimeView.arenaBytes);
        goto done;
    }

    printf("[MAPRT] ARENA bytes=%u nodes@0 lines@%u sprites@%u events@%u byteCodes@%u strings@%u blockMap@%u planes@%u resources@%u\n",
           (unsigned int)runtimeView.arenaBytes,
           (unsigned int)runtimeView.nodeBytes,
           (unsigned int)(runtimeView.nodeBytes + runtimeView.lineBytes),
           (unsigned int)(runtimeView.nodeBytes + runtimeView.lineBytes + runtimeView.mapSpriteBytes),
           (unsigned int)(runtimeView.nodeBytes + runtimeView.lineBytes + runtimeView.mapSpriteBytes + runtimeView.eventBytes),
           (unsigned int)(runtimeView.nodeBytes + runtimeView.lineBytes + runtimeView.mapSpriteBytes + runtimeView.eventBytes + runtimeView.byteCodeBytes),
           (unsigned int)(runtimeView.nodeBytes + runtimeView.lineBytes + runtimeView.mapSpriteBytes + runtimeView.eventBytes + runtimeView.byteCodeBytes + runtimeView.stringOffsetsBytes),
           (unsigned int)(runtimeView.nodeBytes + runtimeView.lineBytes + runtimeView.mapSpriteBytes + runtimeView.eventBytes + runtimeView.byteCodeBytes + runtimeView.stringOffsetsBytes + runtimeView.blockMapBytes),
           (unsigned int)(runtimeView.nodeBytes + runtimeView.lineBytes + runtimeView.mapSpriteBytes + runtimeView.eventBytes + runtimeView.byteCodeBytes + runtimeView.stringOffsetsBytes + runtimeView.blockMapBytes + runtimeView.planeMapBytes));

    if (!readSection(&entry, inventory->sections.nodesOffset,
                     (void*)runtimeView.nodes, runtimeView.nodeBytes, &readCalls) ||
        !readSection(&entry, inventory->sections.linesOffset,
                     (void*)runtimeView.lines, runtimeView.lineBytes, &readCalls) ||
        !readSection(&entry, inventory->sections.mapSpritesOffset,
                     (void*)runtimeView.mapSprites, runtimeView.mapSpriteBytes, &readCalls) ||
        !readSection(&entry, inventory->sections.eventsOffset,
                     (void*)runtimeView.events, runtimeView.eventBytes, &readCalls) ||
        !readSection(&entry, inventory->sections.byteCodesOffset,
                     (void*)runtimeView.byteCodes, runtimeView.byteCodeBytes, &readCalls) ||
        !buildStringOffsetTable(&entry, inventory,
                                (uint8_t*)runtimeView.stringOffsetsLE,
                                &readCalls) ||
        !readSection(&entry, inventory->sections.blockMapOffset,
                     (void*)runtimeView.blockMap, runtimeView.blockMapBytes, &readCalls) ||
        !readSection(&entry, inventory->sections.planeTexturesOffset,
                     (void*)runtimeView.planeMap, runtimeView.planeMapBytes, &readCalls)) {
        printf("[MAPRT] FAILED populate readCalls=%u\n", (unsigned int)readCalls);
        goto done;
    }

    memcpy((void*)runtimeView.textureResourceBits,
           inventory->textureResourceIdBits,
           ESP_MAP_RUNTIME_RESOURCE_SET_BYTES);
    memcpy((void*)runtimeView.spriteResourceBits,
           inventory->spriteResourceIdBits,
           ESP_MAP_RUNTIME_RESOURCE_SET_BYTES);
    memcpy((void*)runtimeView.planeTextureBits,
           inventory->planeTextureIdBits,
           ESP_MAP_RUNTIME_RESOURCE_SET_BYTES);

    runtimeView.populateReadCalls = readCalls;
    runtimeView.arenaFNV1a = fnv1a32(runtimeArena, runtimeView.arenaBytes);

    printf("[MAPRT] READY arenaBytes=%u populateReadCalls=%u arenaFNV=%08x strings=%u sourceCRC=%08x\n",
           (unsigned int)runtimeView.arenaBytes,
           (unsigned int)runtimeView.populateReadCalls,
           (unsigned int)runtimeView.arenaFNV1a,
           (unsigned int)runtimeView.stringCount,
           (unsigned int)runtimeView.sourceCrc32);

    ok = 1;

done:
    if (!ok) {
        EspMapRuntime_reset();
    }
    if (openedHere) {
        EspAssetPack_close();
    }
    return ok;
}

int EspMapRuntime_isLoaded(void) {
    return runtimeArena != NULL &&
           runtimeView.arena == runtimeArena &&
           runtimeView.arenaBytes > 0U;
}

const EspMapRuntimeView* EspMapRuntime_view(void) {
    return EspMapRuntime_isLoaded() ? &runtimeView : NULL;
}

int EspMapRuntime_getStringSourceOffset(uint32_t index,
                                        uint16_t* outOffset) {
    uint32_t pos;

    if (!EspMapRuntime_isLoaded() || outOffset == NULL ||
        index >= runtimeView.stringCount) {
        return 0;
    }

    pos = index * ESP_MAP_RUNTIME_STRING_OFFSET_BYTES;
    *outOffset = (uint16_t)((uint16_t)runtimeView.stringOffsetsLE[pos] |
                            ((uint16_t)runtimeView.stringOffsetsLE[pos + 1U] << 8));
    return 1;
}
