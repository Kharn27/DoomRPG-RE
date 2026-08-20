#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_bsp_reader.h"

#define BSP_NODE_RECORD_BYTES 10U
#define BSP_LINE_RECORD_BYTES 10U
#define BSP_SPRITE_RECORD_BYTES 5U
#define BSP_EVENT_RECORD_BYTES 4U
#define BSP_BYTECODE_RECORD_BYTES 9U
#define BSP_BLOCKMAP_BYTES 256U
#define BSP_PLANE_TEXTURE_BYTES (2U * 1024U)

typedef struct EspBspCursor_s {
    EspAssetPackEntry entry;
    uint8_t buffer[ESP_BSP_READER_BUFFER_BYTES];
    uint32_t bufferPos;
    uint32_t bufferLength;
    uint32_t position;
    uint32_t readCalls;
    uint32_t fnv1a32;
    uint32_t crc32;
} EspBspCursor;

static uint32_t minU32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static uint32_t crc32Update(uint32_t crc, const uint8_t* data, uint32_t length) {
    uint32_t i;
    uint32_t bit;

    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8U; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return crc;
}

static uint32_t fnv1aUpdate(uint32_t hash, const uint8_t* data, uint32_t length) {
    uint32_t i;

    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

static int cursorRefill(EspBspCursor* cursor) {
    uint32_t remaining;
    uint32_t length;

    if (cursor == NULL || cursor->position > cursor->entry.size) {
        return 0;
    }

    if (cursor->position == cursor->entry.size) {
        cursor->bufferPos = 0;
        cursor->bufferLength = 0;
        return 1;
    }

    remaining = cursor->entry.size - cursor->position;
    length = minU32(remaining, ESP_BSP_READER_BUFFER_BYTES);

    if (!EspAssetPack_readRange(&cursor->entry,
                                cursor->position,
                                cursor->buffer,
                                length)) {
        return 0;
    }

    cursor->bufferPos = 0;
    cursor->bufferLength = length;
    ++cursor->readCalls;
    return 1;
}

static int cursorConsume(EspBspCursor* cursor,
                         void* destination,
                         uint32_t length) {
    uint8_t* out = (uint8_t*)destination;
    uint32_t available;
    uint32_t chunk;

    if (cursor == NULL ||
        length > cursor->entry.size - cursor->position) {
        return 0;
    }

    while (length > 0U) {
        if (cursor->bufferPos >= cursor->bufferLength) {
            if (!cursorRefill(cursor) || cursor->bufferLength == 0U) {
                return 0;
            }
        }

        available = cursor->bufferLength - cursor->bufferPos;
        chunk = minU32(available, length);

        cursor->fnv1a32 = fnv1aUpdate(cursor->fnv1a32,
                                     cursor->buffer + cursor->bufferPos,
                                     chunk);
        cursor->crc32 = crc32Update(cursor->crc32,
                                    cursor->buffer + cursor->bufferPos,
                                    chunk);

        if (out != NULL) {
            memcpy(out, cursor->buffer + cursor->bufferPos, chunk);
            out += chunk;
        }

        cursor->bufferPos += chunk;
        cursor->position += chunk;
        length -= chunk;
    }

    return 1;
}

static int cursorReadU8(EspBspCursor* cursor, uint8_t* value) {
    return value != NULL && cursorConsume(cursor, value, 1U);
}

static int cursorReadU16(EspBspCursor* cursor, uint16_t* value) {
    uint8_t bytes[2];

    if (value == NULL || !cursorConsume(cursor, bytes, sizeof(bytes))) {
        return 0;
    }

    *value = (uint16_t)((uint16_t)bytes[0] |
                        ((uint16_t)bytes[1] << 8));
    return 1;
}

static int cursorSkipRecords(EspBspCursor* cursor,
                             uint32_t count,
                             uint32_t recordBytes) {
    uint64_t bytes = (uint64_t)count * (uint64_t)recordBytes;

    if (bytes > UINT32_MAX) {
        return 0;
    }
    return cursorConsume(cursor, NULL, (uint32_t)bytes);
}

static int parseHeader(EspBspCursor* cursor, EspBspInventory* inventory) {
    uint8_t rawName[16];

    if (cursor == NULL || inventory == NULL ||
        !cursorConsume(cursor, rawName, sizeof(rawName))) {
        return 0;
    }

    memcpy(inventory->mapName, rawName, sizeof(rawName));
    inventory->mapName[16] = '\0';

    if (!cursorConsume(cursor, inventory->floorRgb, 3U) ||
        !cursorConsume(cursor, inventory->ceilingRgb, 3U) ||
        !cursorReadU8(cursor, &inventory->floorTexture) ||
        !cursorReadU8(cursor, &inventory->ceilingTexture) ||
        !cursorConsume(cursor, inventory->introRgb, 3U) ||
        !cursorReadU8(cursor, &inventory->loadMapId) ||
        !cursorReadU16(cursor, &inventory->spawnIndex) ||
        !cursorReadU8(cursor, &inventory->spawnDirection) ||
        !cursorReadU16(cursor, &inventory->cameraSpawnIndex)) {
        return 0;
    }

    return cursor->position == ESP_BSP_HEADER_BYTES;
}

static int parseInventory(EspBspCursor* cursor, EspBspInventory* inventory) {
    uint16_t count;
    uint16_t stringLength;
    uint32_t i;

    if (!parseHeader(cursor, inventory)) {
        return 0;
    }

    if (!cursorReadU16(cursor, &count)) return 0;
    inventory->nodes = count;
    if (!cursorSkipRecords(cursor, inventory->nodes, BSP_NODE_RECORD_BYTES)) return 0;

    if (!cursorReadU16(cursor, &count)) return 0;
    inventory->lines = count;
    if (!cursorSkipRecords(cursor, inventory->lines, BSP_LINE_RECORD_BYTES)) return 0;

    if (!cursorReadU16(cursor, &count)) return 0;
    inventory->mapSprites = count;
    if (!cursorSkipRecords(cursor, inventory->mapSprites, BSP_SPRITE_RECORD_BYTES)) return 0;

    if (!cursorReadU16(cursor, &count)) return 0;
    inventory->events = count;
    if (!cursorSkipRecords(cursor, inventory->events, BSP_EVENT_RECORD_BYTES)) return 0;

    if (!cursorReadU16(cursor, &count)) return 0;
    inventory->byteCodes = count;
    if (!cursorSkipRecords(cursor, inventory->byteCodes, BSP_BYTECODE_RECORD_BYTES)) return 0;

    if (!cursorReadU16(cursor, &count)) return 0;
    inventory->strings = count;

    for (i = 0; i < inventory->strings; ++i) {
        if (!cursorReadU16(cursor, &stringLength)) {
            return 0;
        }

        inventory->stringDataBytes += stringLength;
        inventory->legacyStringAllocationBytes += (uint32_t)stringLength + 1U;
        if ((uint32_t)stringLength > inventory->maxStringBytes) {
            inventory->maxStringBytes = stringLength;
        }

        if (!cursorConsume(cursor, NULL, stringLength)) {
            return 0;
        }
    }

    if (!cursorConsume(cursor, NULL, BSP_BLOCKMAP_BYTES) ||
        !cursorConsume(cursor, NULL, BSP_PLANE_TEXTURE_BYTES)) {
        return 0;
    }

    inventory->structuralEndOffset = cursor->position;
    inventory->trailingBytes = cursor->entry.size - cursor->position;

    if (inventory->trailingBytes > 0U &&
        !cursorConsume(cursor, NULL, inventory->trailingBytes)) {
        return 0;
    }

    inventory->consumedBytes = cursor->position;
    return cursor->position == cursor->entry.size;
}

int EspBspReader_inventoryPackEntry(const char* resourceName,
                                    EspBspInventory* outInventory) {
    EspAssetPackEntry entry;
    EspBspCursor cursor;
    int openedHere = 0;
    int ok = 0;

    if (resourceName == NULL || resourceName[0] == '\0' ||
        outInventory == NULL) {
        printf("[BSPREAD] FAILED invalid inventory request\n");
        return 0;
    }

    memset(outInventory, 0, sizeof(*outInventory));
    memset(&cursor, 0, sizeof(cursor));

    if (!EspAssetPack_isOpen()) {
        if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
            printf("[BSPREAD] FAILED open %s\n", ESP_ASSET_PACK_DEFAULT_PATH);
            return 0;
        }
        openedHere = 1;
    }

    if (!EspAssetPack_findEntry(resourceName, &entry) ||
        entry.size < ESP_BSP_HEADER_BYTES ||
        (entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U) {
        printf("[BSPREAD] FAILED missing/invalid entry %s\n", resourceName);
        goto done;
    }

    cursor.entry = entry;
    cursor.fnv1a32 = 2166136261U;
    cursor.crc32 = 0xFFFFFFFFU;

    printf("[BSPREAD] ENTRY %s offset=%u size=%u crc32=%08x window=%uB\n",
           resourceName,
           (unsigned int)entry.offset,
           (unsigned int)entry.size,
           (unsigned int)entry.crc32,
           (unsigned int)ESP_BSP_READER_BUFFER_BYTES);

    if (!parseInventory(&cursor, outInventory)) {
        printf("[BSPREAD] FAILED parse position=%u/%u readCalls=%u\n",
               (unsigned int)cursor.position,
               (unsigned int)entry.size,
               (unsigned int)cursor.readCalls);
        goto done;
    }

    outInventory->sourceBytes = entry.size;
    outInventory->readCalls = cursor.readCalls;
    outInventory->fnv1a32 = cursor.fnv1a32;
    outInventory->crc32 = cursor.crc32 ^ 0xFFFFFFFFU;
    outInventory->expectedCrc32 = entry.crc32;

    if (outInventory->crc32 != entry.crc32) {
        printf("[BSPREAD] FAILED CRC32 actual=%08x expected=%08x\n",
               (unsigned int)outInventory->crc32,
               (unsigned int)entry.crc32);
        goto done;
    }

    printf("[BSPREAD] HEADER name='%.16s' loadMapId=%u spawn=%u dir=%u camera=%u floorTex=%u ceilingTex=%u\n",
           outInventory->mapName,
           (unsigned int)outInventory->loadMapId,
           (unsigned int)outInventory->spawnIndex,
           (unsigned int)outInventory->spawnDirection,
           (unsigned int)outInventory->cameraSpawnIndex,
           (unsigned int)outInventory->floorTexture,
           (unsigned int)outInventory->ceilingTexture);
    printf("[BSPREAD] INVENTORY nodes=%u lines=%u mapSprites=%u events=%u byteCodes=%u strings=%u stringData=%u legacyStringAlloc=%u maxString=%u structuralEnd=%u trailing=%u\n",
           (unsigned int)outInventory->nodes,
           (unsigned int)outInventory->lines,
           (unsigned int)outInventory->mapSprites,
           (unsigned int)outInventory->events,
           (unsigned int)outInventory->byteCodes,
           (unsigned int)outInventory->strings,
           (unsigned int)outInventory->stringDataBytes,
           (unsigned int)outInventory->legacyStringAllocationBytes,
           (unsigned int)outInventory->maxStringBytes,
           (unsigned int)outInventory->structuralEndOffset,
           (unsigned int)outInventory->trailingBytes);
    printf("[BSPREAD] STREAM bytes=%u/%u readCalls=%u window=%uB fnv1a=%08x crc32=%08x verified=yes\n",
           (unsigned int)outInventory->consumedBytes,
           (unsigned int)outInventory->sourceBytes,
           (unsigned int)outInventory->readCalls,
           (unsigned int)ESP_BSP_READER_BUFFER_BYTES,
           (unsigned int)outInventory->fnv1a32,
           (unsigned int)outInventory->crc32);

    ok = 1;

done:
    if (openedHere) {
        EspAssetPack_close();
    }
    return ok;
}
