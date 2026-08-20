#ifndef DOOMRPG_ESP32_MAP_RUNTIME_H
#define DOOMRPG_ESP32_MAP_RUNTIME_H

#include <stdint.h>

#include "esp_bsp_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_MAP_RUNTIME_STRING_OFFSET_BYTES 2U
#define ESP_MAP_RUNTIME_RESOURCE_SET_BYTES \
    (ESP_BSP_RESOURCE_BITSET_WORDS * (uint32_t)sizeof(uint32_t))

#define ESP_MAP_NODE_RECORD_BYTES 10U
#define ESP_MAP_LINE_RECORD_BYTES 10U
#define ESP_MAP_SPRITE_RECORD_BYTES 5U
#define ESP_MAP_EVENT_RECORD_BYTES 4U
#define ESP_MAP_BYTECODE_RECORD_BYTES 9U
#define ESP_MAP_BLOCK_CELL_COUNT 1024U
#define ESP_MAP_PLANE_COUNT 2U
#define ESP_MAP_PLANE_CELL_COUNT 1024U

/*
 * Decoded views of the immutable compact BSP records.
 *
 * Coordinates are source coordinates expanded with the recovered byte << 3
 * rule. They deliberately do not apply later legacy runtime nudges/relinks.
 * Those mutations belong in explicit native overlays/consumers instead of in
 * the immutable source contract.
 */
typedef struct EspMapNode_s {
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    uint32_t args1;
    uint32_t args2;
} EspMapNode;

typedef struct EspMapLine_s {
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    uint16_t texture;
    uint32_t flags;
} EspMapLine;

typedef struct EspMapSprite_s {
    uint16_t x;
    uint16_t y;
    uint32_t info;
} EspMapSprite;

typedef struct EspMapByteCode_s {
    uint8_t id;
    uint32_t arg1;
    uint32_t arg2;
} EspMapByteCode;

typedef struct EspMapRuntimeView_s {
    const uint8_t* arena;
    uint32_t arenaBytes;
    uint32_t arenaFNV1a;
    uint32_t populateReadCalls;

    const uint8_t* nodes;
    uint32_t nodeCount;
    uint32_t nodeBytes;

    const uint8_t* lines;
    uint32_t lineCount;
    uint32_t lineBytes;

    const uint8_t* mapSprites;
    uint32_t mapSpriteCount;
    uint32_t mapSpriteBytes;

    const uint8_t* events;
    uint32_t eventCount;
    uint32_t eventBytes;

    const uint8_t* byteCodes;
    uint32_t byteCodeCount;
    uint32_t byteCodeBytes;

    const uint8_t* stringOffsetsLE;
    uint32_t stringCount;
    uint32_t stringOffsetsBytes;

    const uint8_t* blockMap;
    uint32_t blockMapBytes;

    const uint8_t* planeMap;
    uint32_t planeMapBytes;

    const uint8_t* textureResourceBits;
    const uint8_t* spriteResourceBits;
    const uint8_t* planeTextureBits;

    uint32_t sourceBytes;
    uint32_t sourceCrc32;
} EspMapRuntimeView;

/*
 * Own one compact immutable native map base.
 *
 * The arena contains the original compact BSP records, a packed uint16 string
 * source-offset table, block/plane maps and three bounded resource bitsets.
 * It deliberately contains no entities, mutable doors/scripts, linked lists,
 * custom/drop sprites, texel payloads or desktop-derived runtime objects.
 */
void EspMapRuntime_reset(void);
int EspMapRuntime_loadPackEntry(const char* resourceName,
                                const EspBspInventory* inventory);
int EspMapRuntime_isLoaded(void);
const EspMapRuntimeView* EspMapRuntime_view(void);

/* Bounds-checked, allocation-free decoders over the compact resident arena. */
int EspMapRuntime_getNode(uint32_t index, EspMapNode* outNode);
int EspMapRuntime_getLine(uint32_t index, EspMapLine* outLine);
int EspMapRuntime_getMapSprite(uint32_t index, EspMapSprite* outSprite);
int EspMapRuntime_getEvent(uint32_t index, uint32_t* outEvent);
int EspMapRuntime_getByteCode(uint32_t index, EspMapByteCode* outByteCode);

/* Returns the BSP-entry-relative offset of string payload `index`. */
int EspMapRuntime_getStringSourceOffset(uint32_t index,
                                        uint16_t* outOffset);

/* Packed spatial/resource accessors. */
int EspMapRuntime_getBlockCell(uint32_t cellIndex, uint8_t* outFlags);
int EspMapRuntime_getPlaneTexture(uint32_t planeIndex,
                                  uint32_t cellIndex,
                                  uint8_t* outTextureId);
int EspMapRuntime_textureRequired(uint32_t resourceId);
int EspMapRuntime_spriteRequired(uint32_t resourceId);
int EspMapRuntime_planeTextureUsed(uint32_t resourceId);

#ifdef __cplusplus
}
#endif

#endif
