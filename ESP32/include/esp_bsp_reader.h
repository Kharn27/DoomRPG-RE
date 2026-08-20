#ifndef DOOMRPG_ESP32_BSP_READER_H
#define DOOMRPG_ESP32_BSP_READER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_BSP_READER_BUFFER_BYTES 256U
#define ESP_BSP_HEADER_BYTES 33U
#define ESP_BSP_RESOURCE_ID_LIMIT 256U
#define ESP_BSP_RESOURCE_BITSET_WORDS (ESP_BSP_RESOURCE_ID_LIMIT / 32U)

typedef struct EspBspSections_s {
    uint32_t nodesOffset;
    uint32_t linesOffset;
    uint32_t mapSpritesOffset;
    uint32_t eventsOffset;
    uint32_t byteCodesOffset;
    uint32_t stringsOffset;
    uint32_t blockMapOffset;
    uint32_t planeTexturesOffset;
    uint32_t endOffset;
} EspBspSections;

/*
 * First bounded persistent-memory plan for the native map runtime.
 *
 * This deliberately keeps original BSP records compact instead of expanding
 * them into the desktop pointer-heavy Node_t/Line_t/Sprite_t structures.
 * Strings stay on SD; only one uint16_t offset per string is budgeted here.
 * Mutable gameplay/entity state is intentionally NOT included yet.
 */
typedef struct EspMapPlan_s {
    uint32_t nodeRecordsBytes;
    uint32_t lineRecordsBytes;
    uint32_t mapSpriteRecordsBytes;
    uint32_t eventRecordsBytes;
    uint32_t byteCodeRecordsBytes;
    uint32_t stringOffsetsBytes;
    uint32_t blockMapBytes;
    uint32_t planeMapBytes;
    uint32_t resourceSetsBytes;
    uint32_t persistentBytes;
} EspMapPlan;

typedef struct EspBspInventory_s {
    char mapName[17];

    uint8_t floorRgb[3];
    uint8_t ceilingRgb[3];
    uint8_t floorTexture;
    uint8_t ceilingTexture;
    uint8_t introRgb[3];
    uint8_t loadMapId;
    uint16_t spawnIndex;
    uint8_t spawnDirection;
    uint16_t cameraSpawnIndex;

    uint32_t nodes;
    uint32_t lines;
    uint32_t mapSprites;
    uint32_t events;
    uint32_t byteCodes;
    uint32_t strings;
    uint32_t stringDataBytes;
    uint32_t legacyStringAllocationBytes;
    uint32_t maxStringBytes;

    EspBspSections sections;
    EspMapPlan plan;

    /* Direct logical IDs present in the BSP. */
    uint32_t uniqueLineTextureIds;
    uint32_t uniqueMapSpriteIds;

    /* Resource IDs required by recovered map semantics. */
    uint32_t uniqueTextureResourceIds;
    uint32_t uniqueSpriteResourceIds;
    uint32_t uniquePlaneTextureIds;
    uint32_t changeSpriteByteCodes;
    uint32_t spriteAsTextureRefs;

    /* If non-zero, the corresponding 256-ID bitset is incomplete. */
    uint32_t lineTextureIdsAbove255;
    uint32_t textureResourceIdsAbove255;
    uint32_t spriteResourceIdsAbove255;

    uint32_t lineTextureIdBits[ESP_BSP_RESOURCE_BITSET_WORDS];
    uint32_t mapSpriteIdBits[ESP_BSP_RESOURCE_BITSET_WORDS];
    uint32_t textureResourceIdBits[ESP_BSP_RESOURCE_BITSET_WORDS];
    uint32_t spriteResourceIdBits[ESP_BSP_RESOURCE_BITSET_WORDS];
    uint32_t planeTextureIdBits[ESP_BSP_RESOURCE_BITSET_WORDS];

    uint32_t structuralEndOffset;
    uint32_t trailingBytes;
    uint32_t sourceBytes;
    uint32_t consumedBytes;
    uint32_t readCalls;
    uint32_t fnv1a32;
    uint32_t crc32;
    uint32_t expectedCrc32;
} EspBspInventory;

/*
 * Inventory one original Doom RPG BSP directly from DoomRPG-ESP32.pak.
 *
 * The reader owns no map-sized allocation. It walks the source sequentially
 * through a fixed ESP_BSP_READER_BUFFER_BYTES window, validates the complete
 * payload CRC32 and records scalar inventory, exact section offsets, compact
 * persistent-memory planning and bounded logical resource-ID sets.
 *
 * The asset pack may already be open. If this function opens it, it closes it
 * before returning; otherwise the caller retains ownership of the existing
 * open pack session.
 */
int EspBspReader_inventoryPackEntry(const char* resourceName,
                                    EspBspInventory* outInventory);

#ifdef __cplusplus
}
#endif

#endif
