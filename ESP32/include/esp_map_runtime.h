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

/* Returns the BSP-entry-relative offset of string payload `index`. */
int EspMapRuntime_getStringSourceOffset(uint32_t index,
                                        uint16_t* outOffset);

#ifdef __cplusplus
}
#endif

#endif
