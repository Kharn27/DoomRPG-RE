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

typedef struct EspAssetPackResidentStats_s {
    uint32_t logicalOpens;
    uint32_t physicalOpens;
    uint32_t validationPasses;
    uint32_t residentReuses;
    uint32_t physicalReads;
    uint32_t physicalBytes;
    uint32_t entryCacheHits;
    uint32_t entryCacheMisses;
    uint32_t rangeCacheHits;
    uint32_t rangeCacheMisses;
    uint32_t rangeCacheStores;
    uint32_t rangeCacheBypasses;
    uint32_t rangeCacheBytesUsed;
    uint32_t rangeCacheCapacityBytes;
    uint32_t rangeCacheEntries;
    uint32_t rangeCacheEntryCapacity;
    uint32_t ownerBytes;
    uint8_t enabled;
    uint8_t ready;
    uint8_t largeRangeEnabled;
    uint8_t largeRangeEntries;
} EspAssetPackResidentStats;

typedef struct EspAssetPackMapFlashStats_s {
    uint32_t partitionBytes;
    uint32_t sourcePackBytes;
    uint32_t sourceDataBytes;
    uint32_t indexBytes;
    uint32_t metadataBytes;
    uint32_t stagedBytes;
    uint32_t excludedBytes;
    uint32_t buildMicros;
    uint32_t flashReads;
    uint32_t flashBytes;
    uint32_t strictMisses;
    uint16_t entryCount;
    uint8_t currentMapId;
    uint8_t excludedMaps;
    uint8_t active;
    uint8_t verified;
} EspAssetPackMapFlashStats;

/* Opens and validates the native asset pack. Before map-flash staging this is
 * the authoritative SD PAK. While a staged gameplay slot is active the same
 * logical API is served from raw internal flash; source offsets and entry
 * metadata remain the original PAK coordinates.
 */
int EspAssetPack_open(const char* path);
void EspAssetPack_close(void);
int EspAssetPack_isOpen(void);
uint32_t EspAssetPack_fileSize(void);
int EspAssetPack_entryCount(void);
uint32_t EspAssetPack_dataOffset(void);

/* Stage one complete gameplay working set into the raw data partition currently
 * labelled "spiffs". The filesystem is never mounted: the partition is used
 * as a single transactional cache slot addressed with esp_partition_*.
 *
 * The staged set is deliberately conservative and dependency-complete: the
 * original PAK index plus every data byte except the other known BSP maps.
 * The current BSP remains staged because dialog/string consumers read it by
 * original entry-relative offsets during gameplay. The header is committed
 * only after copy + flash readback verification. Once active, an attempt to
 * read an excluded range fails closed; it never falls back to SD.
 *
 * Stage requires no logical PAK lease and no resident RAM cache. It may erase
 * and rewrite the raw slot and is therefore a map-loading operation only.
 */
int EspAssetPack_mapFlashStage(uint8_t currentMapId);
void EspAssetPack_mapFlashDeactivate(void);
int EspAssetPack_isMapFlashActive(void);
void EspAssetPack_mapFlashGetStats(EspAssetPackMapFlashStats* outStats);

/* Opt-in backing-store residency for the native gameplay renderer. The normal
 * open/close contract remains unchanged until begin succeeds. In resident mode
 * close releases only the logical lease on the default PAK; the already
 * validated backing stays owned for the next phase/turn. Small exact immutable
 * ranges are cached in one bounded RAM owner. End is fail-closed while a
 * logical lease is open and also releases the current raw-flash gameplay slot
 * back to SD ownership for the next map load.
 */
int EspAssetPack_residentBegin(void);
int EspAssetPack_residentEnd(void);
int EspAssetPack_isResident(void);
void EspAssetPack_residentResetStats(void);
void EspAssetPack_residentGetStats(EspAssetPackResidentStats* outStats);

/* Optional second-stage cache for exact immutable 2048-byte ranges. It borrows
 * only unused tail bytes from the configured bounded resident payload: no
 * second heap owner is allocated. Small-range entries retain priority and may
 * evict large tail entries when they need payload space. Begin/end require the
 * resident default PAK to be logically closed.
 */
int EspAssetPack_residentLargeRangeBegin(void);
int EspAssetPack_residentLargeRangeEnd(void);
int EspAssetPack_isResidentLargeRangeEnabled(void);

/* Returns one fixed-size source-index record by ordinal. The active backing may
 * be SD or the verified raw-flash copy, but callers always see original PAK
 * offsets/size/CRC/flags.
 */
int EspAssetPack_getEntryByIndex(int index, EspAssetPackEntry* outEntry);

/* Case-insensitive, slash-normalized lookup. The pack index is hash-sorted, so
 * this performs a bounded binary search against the active backing.
 *
 * The offline builder rejects all name-hash collisions, making the 32-bit hash
 * an unambiguous key for a valid pack.
 */
uint32_t EspAssetPack_nameHash(const char* name);
int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* outEntry);

/* Read exactly `length` bytes from an entry-relative offset. No allocation and
 * no decompression occurs in this function. In staged gameplay mode an
 * excluded/non-staged range is a strict failure, never an SD fallback.
 */
int EspAssetPack_readRange(const EspAssetPackEntry* entry,
                           uint32_t relativeOffset,
                           void* destination,
                           size_t length);

#ifdef __cplusplus
}
#endif

#endif
