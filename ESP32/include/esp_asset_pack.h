#ifndef DOOMRPG_ESP32_ASSET_PACK_H
#define DOOMRPG_ESP32_ASSET_PACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ASSET_PACK_DEFAULT_PATH "/DoomRPG-ESP32.pak"
#define ESP_ASSET_PACK_MAX_ENTRIES 8

typedef struct EspAssetPackEntry_s {
    char name[17];
    uint32_t offset;
    uint32_t size;
    uint32_t crc32;
    uint32_t flags;
} EspAssetPackEntry;

/* Opens and validates the fixed-index ESP32-native asset pack on the SD card.
 * The reader keeps only a tiny index in static memory; payload data stays on SD.
 */
int EspAssetPack_open(const char* path);
void EspAssetPack_close(void);
int EspAssetPack_isOpen(void);
uint32_t EspAssetPack_fileSize(void);
int EspAssetPack_entryCount(void);
int EspAssetPack_getEntryByIndex(int index, EspAssetPackEntry* outEntry);
int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* outEntry);

/* Read exactly `length` bytes from an entry-relative offset. No allocation and
 * no decompression occurs in this function.
 */
int EspAssetPack_readRange(const EspAssetPackEntry* entry,
                           uint32_t relativeOffset,
                           void* destination,
                           size_t length);

#ifdef __cplusplus
}
#endif

#endif
