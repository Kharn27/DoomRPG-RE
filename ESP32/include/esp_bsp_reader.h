#ifndef DOOMRPG_ESP32_BSP_READER_H
#define DOOMRPG_ESP32_BSP_READER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_BSP_READER_BUFFER_BYTES 256U
#define ESP_BSP_HEADER_BYTES 33U

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
 * payload CRC32 and records only scalar inventory/header values.
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
