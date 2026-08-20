#include <stdint.h>

#include "esp_map_runtime.h"

static uint16_t readLe16(const uint8_t* bytes) {
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

static uint32_t readLe32(const uint8_t* bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static int bitIsSet(const uint8_t* bits, uint32_t resourceId) {
    uint32_t word;
    uint32_t mask;

    if (bits == NULL || resourceId >= ESP_BSP_RESOURCE_ID_LIMIT) {
        return 0;
    }

    word = readLe32(bits + ((resourceId >> 5) * 4U));
    mask = 1U << (resourceId & 31U);
    return (word & mask) != 0U;
}

int EspMapRuntime_getNode(uint32_t index, EspMapNode* outNode) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    const uint8_t* raw;

    if (view == NULL || outNode == NULL || index >= view->nodeCount ||
        view->nodeBytes != view->nodeCount * ESP_MAP_NODE_RECORD_BYTES) {
        return 0;
    }

    raw = view->nodes + (index * ESP_MAP_NODE_RECORD_BYTES);
    outNode->x1 = (uint16_t)raw[0] << 3;
    outNode->y1 = (uint16_t)raw[1] << 3;
    outNode->x2 = (uint16_t)raw[2] << 3;
    outNode->y2 = (uint16_t)raw[3] << 3;
    outNode->args1 = ((uint32_t)raw[4] << 16) |
                     ((uint32_t)raw[5] << 3);
    outNode->args2 = (uint32_t)readLe16(raw + 6U) |
                     ((uint32_t)readLe16(raw + 8U) << 16);
    return 1;
}

int EspMapRuntime_getLine(uint32_t index, EspMapLine* outLine) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    const uint8_t* raw;

    if (view == NULL || outLine == NULL || index >= view->lineCount ||
        view->lineBytes != view->lineCount * ESP_MAP_LINE_RECORD_BYTES) {
        return 0;
    }

    raw = view->lines + (index * ESP_MAP_LINE_RECORD_BYTES);
    outLine->x1 = (uint16_t)raw[0] << 3;
    outLine->y1 = (uint16_t)raw[1] << 3;
    outLine->x2 = (uint16_t)raw[2] << 3;
    outLine->y2 = (uint16_t)raw[3] << 3;
    outLine->texture = readLe16(raw + 4U);
    outLine->flags = readLe32(raw + 6U);
    return 1;
}

int EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* outSprite) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    const uint8_t* raw;

    if (view == NULL || outSprite == NULL || index >= view->mapSpriteCount ||
        view->mapSpriteBytes !=
            view->mapSpriteCount * ESP_MAP_SPRITE_RECORD_BYTES) {
        return 0;
    }

    raw = view->mapSprites + (index * ESP_MAP_SPRITE_RECORD_BYTES);
    outSprite->x = (uint16_t)raw[0] << 3;
    outSprite->y = (uint16_t)raw[1] << 3;
    outSprite->info = (uint32_t)raw[2] |
                      ((uint32_t)readLe16(raw + 3U) << 16);
    return 1;
}

int EspMapRuntime_getEvent(uint32_t index, uint32_t* outEvent) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    const uint8_t* raw;

    if (view == NULL || outEvent == NULL || index >= view->eventCount ||
        view->eventBytes != view->eventCount * ESP_MAP_EVENT_RECORD_BYTES) {
        return 0;
    }

    raw = view->events + (index * ESP_MAP_EVENT_RECORD_BYTES);
    *outEvent = readLe32(raw);
    return 1;
}

int EspMapRuntime_getByteCode(uint32_t index, EspMapByteCode* outByteCode) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    const uint8_t* raw;

    if (view == NULL || outByteCode == NULL || index >= view->byteCodeCount ||
        view->byteCodeBytes !=
            view->byteCodeCount * ESP_MAP_BYTECODE_RECORD_BYTES) {
        return 0;
    }

    raw = view->byteCodes + (index * ESP_MAP_BYTECODE_RECORD_BYTES);
    outByteCode->id = raw[0];
    outByteCode->arg1 = readLe32(raw + 1U);
    outByteCode->arg2 = readLe32(raw + 5U);
    return 1;
}

int EspMapRuntime_getBlockCell(uint32_t cellIndex, uint8_t* outFlags) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    uint8_t packed;
    uint32_t shift;

    if (view == NULL || outFlags == NULL ||
        cellIndex >= ESP_MAP_BLOCK_CELL_COUNT || view->blockMapBytes != 256U) {
        return 0;
    }

    packed = view->blockMap[cellIndex >> 2];
    shift = (cellIndex & 3U) * 2U;
    *outFlags = (uint8_t)((packed >> shift) & 3U);
    return 1;
}

int EspMapRuntime_getPlaneTexture(uint32_t planeIndex,
                                  uint32_t cellIndex,
                                  uint8_t* outTextureId) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    uint32_t offset;

    if (view == NULL || outTextureId == NULL ||
        planeIndex >= ESP_MAP_PLANE_COUNT ||
        cellIndex >= ESP_MAP_PLANE_CELL_COUNT ||
        view->planeMapBytes !=
            ESP_MAP_PLANE_COUNT * ESP_MAP_PLANE_CELL_COUNT) {
        return 0;
    }

    offset = (planeIndex * ESP_MAP_PLANE_CELL_COUNT) + cellIndex;
    *outTextureId = view->planeMap[offset];
    return 1;
}

int EspMapRuntime_textureRequired(uint32_t resourceId) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    return view != NULL && bitIsSet(view->textureResourceBits, resourceId);
}

int EspMapRuntime_spriteRequired(uint32_t resourceId) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    return view != NULL && bitIsSet(view->spriteResourceBits, resourceId);
}

int EspMapRuntime_planeTextureUsed(uint32_t resourceId) {
    const EspMapRuntimeView* view = EspMapRuntime_view();
    return view != NULL && bitIsSet(view->planeTextureBits, resourceId);
}
