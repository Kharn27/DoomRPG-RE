#ifndef DOOMRPG_ESP32_ASSET_PACK_H
#define DOOMRPG_ESP32_ASSET_PACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ASSET_PACK_DEFAULT_PATH "/DoomRPG-ESP32.pak"
#define ESP_ASSET_PACK_FORMAT_VERSION 2U
#define ESP_ASSET_PACK_MAX_ENTRY_COUNT 4096U
#define ESP_ASSET_PACK_FLAG_DIRECTORY 0x00000001U

typedef struct EspAssetPackEntry_s {
    uint32_t nameHash;
    uint32_t offset;
    uint32_t size;
    uint32_t crc32;
    uint32_t flags;
} EspAssetPackEntry;

/* Opens and validates the ESP32-native asset pack on the SD card.
 * Pack v2 keeps its complete fixed-size index on SD. Only the Arduino File
 * object and a handful of scalar fields remain resident in RAM.
 */
int EspAssetPack_open(const char* path);
void EspAssetPack_close(void);
int EspAssetPack_isOpen(void);
uint32_t EspAssetPack_fileSize(void);
int EspAssetPack_entryCount(void);
uint32_t EspAssetPack_dataOffset(void);

/* Returns one fixed-size index record by ordinal. The index itself remains on
 * SD and is never materialized as a resident array.
 */
int EspAssetPack_getEntryByIndex(int index, EspAssetPackEntry* outEntry);

/* Case-insensitive, slash-normalized lookup. The pack index is hash-sorted, so
 * this performs a bounded binary search directly on SD.
 *
 * The offline builder rejects all name-hash collisions, making the 32-bit hash
 * an unambiguous key for a valid pack.
 */
uint32_t EspAssetPack_nameHash(const char* name);
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
