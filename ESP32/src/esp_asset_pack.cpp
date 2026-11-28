#include <Arduino.h>
#include <SD.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <esp_partition.h>
#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_asset_pack_io_probe.h"
#include "doomrpg_log.h"
#include "esp_map_catalog.h"
#if defined(DOOMRPG_ESP32) && DOOMRPG_ESP32
#include "esp_native_font_sd.h"
#endif

namespace {

/* No SD font File may overlap a physical SD PAK/probe open. Flash-backed
 * logical PAK sessions do not call this and can read SD font ranges safely. */
static void suspendFontSdBeforePackOpen()
{
#if defined(DOOMRPG_ESP32) && DOOMRPG_ESP32
    EspNativeFontSd_suspend();
#endif
}


constexpr uint8_t kMagic[8] = {'D', 'R', 'P', 'G', 'E', 'S', 'P', '2'};
constexpr uint32_t kVersion = ESP_ASSET_PACK_FORMAT_VERSION;
constexpr size_t kHeaderBytes = 24;
constexpr size_t kEntryBytes = 20;
/* WALL-SD recovery: keep the 46 KiB post-ring audio reserve on the
 * no-PSRAM CYD. Slightly smaller L1 cache; flash-backed misses stay safe. */
constexpr uint32_t kResidentRangeCapacityBytes = 15U * 1024U;
constexpr uint32_t kResidentRangeEntryCapacity = 256U;
constexpr uint32_t kResidentEntryCacheSlots = 24U;
constexpr uint32_t kResidentMaxCachedRangeBytes = 1024U;
constexpr uint32_t kResidentLargeRangeBytes = 2048U;

constexpr uint32_t kMapFlashMagic = 0x32464d44U; /* DMF2 */
constexpr uint16_t kMapFlashVersion = 2U; /* complete PAK, incompatible with compact V1 */
constexpr uint32_t kMapFlashCommitted = 0xa55a3cc3U;
constexpr uint32_t kMapFlashSectorBytes = 4096U;
constexpr uint32_t kMapFlashIndexOffset = kMapFlashSectorBytes;
constexpr uint32_t kMapFlashCopyBufferBytes = 4096U;
constexpr uint32_t kMapFlashEraseChunkBytes = 64U * 1024U;
constexpr uint8_t kMapFlashMaxMissLogs = 8U;

/* The physical backing is immutable for the lifetime of this cache.
 * A validated source PAK absolute offset + length identifies the bytes
 * exactly, including across aliases. resetPhysicalState clears all records
 * before changing backing; the authoritative SD probe bypasses this cache. */
struct ResidentRangeRecord {
    uint32_t absoluteOffset;
    uint16_t length;
    uint16_t dataOffset;
};

static_assert(kResidentRangeCapacityBytes <= UINT16_MAX,
              "resident payload offsets must fit uint16_t");
static_assert(kResidentLargeRangeBytes <= UINT16_MAX,
              "resident range lengths must fit uint16_t");
static_assert(sizeof(ResidentRangeRecord) == 8U,
              "resident range metadata must stay compact");

struct ResidentEntryRecord {
    uint32_t nameHash;
    EspAssetPackEntry entry;
    uint8_t valid;
};

struct ResidentCache {
    /* Small records in insertion order, then large records in insertion
     * order. The latter are at most seven and match descending payloads. */
    ResidentRangeRecord ranges[kResidentRangeEntryCapacity];
    ResidentEntryRecord entries[kResidentEntryCacheSlots];
    uint8_t bytes[kResidentRangeCapacityBytes];
    uint32_t rangeCount;
    uint16_t bytesUsed;
    uint16_t largeCount; /* contiguous 2048B blocks from high end */
};

struct MapFlashHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t headerBytes;
    uint32_t committed;
    uint32_t sourcePackBytes;
    uint32_t sourceIndexOffset;
    uint32_t sourceDataOffset;
    uint32_t entryCount;
    uint32_t indexFlashOffset;
    uint32_t indexBytes;
    uint32_t payloadFlashOffset;
    uint32_t stagedBytes;
    uint32_t indexFNV1a;
    uint32_t payloadFNV1a;
    uint32_t currentMapHash;
    uint8_t currentMapId;
    uint8_t reserved[3];
};

static_assert(sizeof(MapFlashHeader) <= kMapFlashSectorBytes,
              "map-flash header must fit one erase sector");

struct MapFlashPlan {
    uint32_t sourcePackBytes;
    uint32_t sourceIndexOffset;
    uint32_t sourceDataOffset;
    uint32_t sourceDataBytes;
    uint32_t entryCount;
    uint32_t indexBytes;
    uint32_t payloadFlashOffset;
    uint32_t stagedBytes;
    uint32_t indexFNV1a;
    uint32_t currentMapHash;
    /* Catalog inventory for the capacity audit. */
    uint32_t bspSourceOffset[ESP_MAP_CATALOG_COUNT];
    uint32_t bspBytes[ESP_MAP_CATALOG_COUNT];
    uint32_t allBspBytes;
    uint32_t maxBspBytes;
    uint32_t commonBytes;
};

struct MapFlashState {
    const esp_partition_t* partition;
    MapFlashHeader header;
    EspAssetPackMapFlashStats stats;
    uint8_t active;
    uint8_t missLogs;
    uint8_t handoffEligible; /* one same-map prepare may reuse payload proof */
};

File packFile;
File sourceProbeFile;
EspAssetPackEntry sourceProbeEntry = {};
bool sourceProbeReady = false;
uint32_t sourceProbeFlashEpoch = 0U; /* nonzero only for a verified BSP probe */
uint8_t sourceProbeReadLogged = 0U;
uint32_t entryCount = 0;
uint32_t packSize = 0;
uint32_t indexOffset = 0;
uint32_t dataOffset = 0;
bool openReady = false;
bool physicalReady = false;
bool physicalIsDefault = false;
bool physicalUsesMapFlash = false;
bool residentEnabled = false;
bool residentLargeRangeEnabled = false;
ResidentCache* residentCache = nullptr;
EspAssetPackResidentStats residentStats = {};
/* PR236 B timing witness; uint32 subtraction is valid over one bounded frame. */
uint32_t mapFlashRuntimeReadMicros = 0U;
#if defined(DOOMRPG_RA2B_CAPTURE) && DOOMRPG_RA2B_CAPTURE
/* Probe-only cumulative timing for successful or failed resident SD range
 * transactions. Includes seek+read, excludes PAK index scans and sourceProbe.
 * No heap, additional I/O, or runtime behavior change; never conflate
 * this SD window with raw-partition map-flash timing. */
uint32_t ra2bSdRangeMicros = 0U;
#endif
MapFlashState mapFlash = {};
/* Never reuse a memo across deactivation, re-stage or verify failure.
 * Saturation disables the optimization instead of recycling a generation. */
uint32_t verifiedFlashEpoch = 1U;
EspAssetPackMapFlashProgressCallback mapFlashProgressCallback = nullptr;

void notifyMapFlashProgress(uint8_t phase, uint32_t completed, uint32_t total)
{
    if (mapFlashProgressCallback != nullptr) {
        mapFlashProgressCallback(phase, completed, total);
    }
}

uint32_t readLe32(const uint8_t* data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

uint32_t fnv1aUpdate(uint32_t hash, const uint8_t* data, size_t length)
{
    if (data == nullptr) return hash;
    for (size_t i = 0U; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

uint32_t alignSector(uint32_t value)
{
    const uint32_t mask = kMapFlashSectorBytes - 1U;
    if (value > UINT32_MAX - mask) return 0U;
    return (value + mask) & ~mask;
}

void clearResidentCache()
{
    if (residentCache == nullptr) {
        residentLargeRangeEnabled = false;
        return;
    }
    residentCache->rangeCount = 0U;
    residentCache->bytesUsed = 0U;
    residentCache->largeCount = 0U;
    memset(residentCache->entries, 0, sizeof(residentCache->entries));
    residentLargeRangeEnabled = false;
}

bool readSdExact(void* destination, size_t length)
{
    if (!packFile || destination == nullptr) {
        return false;
    }
    const size_t got = packFile.read(static_cast<uint8_t*>(destination), length);
    if (got != length) {
        return false;
    }
    if (residentEnabled) {
        ++residentStats.physicalReads;
        residentStats.physicalBytes += (uint32_t)length;
    }
    return true;
}

void clearSourceProbe()
{
    if (sourceProbeFile) {
        sourceProbeFile.close();
    }
    memset(&sourceProbeEntry, 0, sizeof(sourceProbeEntry));
    sourceProbeReady = false;
    sourceProbeFlashEpoch = 0U;
    sourceProbeReadLogged = 0U;
}

bool sourceProbeEntryExact(const EspAssetPackEntry& entry)
{
    return sourceProbeReady &&
           entry.nameHash == sourceProbeEntry.nameHash &&
           entry.offset == sourceProbeEntry.offset &&
           entry.size == sourceProbeEntry.size &&
           entry.crc32 == sourceProbeEntry.crc32 &&
           entry.flags == sourceProbeEntry.flags;
}

void clearMapFlashRuntime(bool clearStats)
{
    if (verifiedFlashEpoch != UINT32_MAX) ++verifiedFlashEpoch;
    mapFlash.partition = nullptr;
    memset(&mapFlash.header, 0, sizeof(mapFlash.header));
    mapFlash.active = 0U;
    mapFlash.missLogs = 0U;
    mapFlash.handoffEligible = 0U;
    if (clearStats) {
        memset(&mapFlash.stats, 0, sizeof(mapFlash.stats));
    }
    else {
        mapFlash.stats.active = 0U;
    }
}

void resetPhysicalState()
{
    if (packFile) {
        packFile.close();
    }
    entryCount = 0;
    packSize = 0;
    indexOffset = 0;
    dataOffset = 0;
    openReady = false;
    physicalReady = false;
    physicalIsDefault = false;
    physicalUsesMapFlash = false;
    clearResidentCache();
}

void decodeEntry(const uint8_t* raw, EspAssetPackEntry* entry)
{
    entry->nameHash = readLe32(raw);
    entry->offset = readLe32(raw + 4);
    entry->size = readLe32(raw + 8);
    entry->crc32 = readLe32(raw + 12);
    entry->flags = readLe32(raw + 16);
}

bool validateEntry(const EspAssetPackEntry& entry)
{
    if (entry.nameHash == 0U) {
        return false;
    }
    if (entry.offset < dataOffset || entry.offset > packSize) {
        return false;
    }
    if (entry.size > packSize - entry.offset) {
        return false;
    }
    return true;
}

bool mapFlashRead(uint32_t offset, void* destination, size_t length,
                  bool countRuntime)
{
    if (!mapFlash.active || mapFlash.partition == nullptr ||
        destination == nullptr || offset > mapFlash.partition->size ||
        length > (size_t)(mapFlash.partition->size - offset)) {
        return false;
    }
    /* Two timestamps per runtime raw-flash read; no extra I/O or log here. */
    const int64_t readStart = (countRuntime && residentEnabled)
                                  ? esp_timer_get_time() : 0LL;
    const esp_err_t readResult =
        esp_partition_read(mapFlash.partition, offset, destination, length);
    if (readStart != 0LL) {
        const int64_t elapsed = esp_timer_get_time() - readStart;
        if (elapsed > 0LL) {
            mapFlashRuntimeReadMicros += (uint32_t)elapsed;
        }
    }
    if (readResult != ESP_OK) return false;
    if (countRuntime) {
        ++mapFlash.stats.flashReads;
        mapFlash.stats.flashBytes += (uint32_t)length;
        if (residentEnabled) {
            ++residentStats.physicalReads;
            residentStats.physicalBytes += (uint32_t)length;
        }
    }
    return true;
}

bool mapFlashHeaderLooksValid(const MapFlashHeader& header,
                              const esp_partition_t* partition)
{
    if (partition == nullptr || header.magic != kMapFlashMagic ||
        header.version != kMapFlashVersion ||
        header.headerBytes != sizeof(MapFlashHeader) ||
        header.committed != kMapFlashCommitted ||
        header.sourcePackBytes == 0U ||
        header.entryCount == 0U ||
        header.entryCount > ESP_ASSET_PACK_MAX_ENTRY_COUNT ||
        header.indexFlashOffset != kMapFlashIndexOffset ||
        header.sourceIndexOffset < kHeaderBytes ||
        (uint64_t)header.sourceIndexOffset + header.indexBytes > header.sourceDataOffset ||
        header.sourceDataOffset > header.sourcePackBytes ||
        header.indexBytes != header.entryCount * (uint32_t)kEntryBytes ||
        header.payloadFlashOffset != alignSector(header.indexFlashOffset + header.indexBytes) ||
        header.stagedBytes != header.sourcePackBytes - header.sourceDataOffset ||
        header.payloadFlashOffset > partition->size ||
        header.stagedBytes > partition->size - header.payloadFlashOffset) {
        return false;
    }
    return true;
}

bool mapFlashBuildSourcePlan(uint8_t targetMapId,
                             const esp_partition_t* partition,
                             MapFlashPlan* outPlan,
                             const char** outReason)
{
    uint8_t header[kHeaderBytes];
    uint8_t raw[kEntryBytes];
    uint32_t catalogHashes[ESP_MAP_CATALOG_COUNT] = {};
    MapFlashPlan plan = {};
    File sourceFile;
    bool currentFound = false;
    uint32_t previousHash = 0U;
    bool havePreviousHash = false;

    if (outReason != nullptr) *outReason = "invalid-plan-args";
    if (outPlan == nullptr || partition == nullptr ||
        !EspMapCatalog_isValidId(targetMapId)) {
        return false;
    }

    const char* targetMapName = EspMapCatalog_nameForId(targetMapId);
    plan.currentMapHash = EspAssetPack_nameHash(targetMapName);
    if (targetMapName == nullptr || plan.currentMapHash == 0U) {
        if (outReason != nullptr) *outReason = "target-map-name";
        return false;
    }

    for (uint8_t mapId = ESP_MAP_CATALOG_FIRST_ID;
         mapId <= ESP_MAP_CATALOG_LAST_ID; ++mapId) {
        const char* mapName = EspMapCatalog_nameForId(mapId);
        const uint32_t slot = (uint32_t)(mapId - ESP_MAP_CATALOG_FIRST_ID);
        catalogHashes[slot] = EspAssetPack_nameHash(mapName);
        if (mapName == nullptr || catalogHashes[slot] == 0U) {
            if (outReason != nullptr) *outReason = "catalog-map-name";
            return false;
        }
    }

    suspendFontSdBeforePackOpen();
    sourceFile = SD.open(ESP_ASSET_PACK_DEFAULT_PATH, FILE_READ);
    if (!sourceFile) {
        if (outReason != nullptr) *outReason = "source-pack-open";
        return false;
    }

    plan.sourcePackBytes = (uint32_t)sourceFile.size();
    if (plan.sourcePackBytes < kHeaderBytes ||
        sourceFile.read(header, sizeof(header)) != sizeof(header) ||
        memcmp(header, kMagic, sizeof(kMagic)) != 0 ||
        readLe32(header + 8) != kVersion) {
        sourceFile.close();
        if (outReason != nullptr) *outReason = "source-pack-header";
        return false;
    }

    plan.entryCount = readLe32(header + 12);
    plan.sourceIndexOffset = readLe32(header + 16);
    plan.sourceDataOffset = readLe32(header + 20);
    if (plan.entryCount == 0U ||
        plan.entryCount > ESP_ASSET_PACK_MAX_ENTRY_COUNT ||
        plan.sourceIndexOffset < kHeaderBytes ||
        plan.sourceIndexOffset > plan.sourcePackBytes) {
        sourceFile.close();
        if (outReason != nullptr) *outReason = "source-pack-layout";
        return false;
    }

    const uint64_t indexEnd =
        (uint64_t)plan.sourceIndexOffset +
        ((uint64_t)plan.entryCount * (uint64_t)kEntryBytes);
    if (indexEnd > plan.sourcePackBytes ||
        plan.sourceDataOffset < indexEnd ||
        plan.sourceDataOffset > plan.sourcePackBytes ||
        !sourceFile.seek(plan.sourceIndexOffset)) {
        sourceFile.close();
        if (outReason != nullptr) *outReason = "source-index-layout";
        return false;
    }

    plan.indexBytes = plan.entryCount * (uint32_t)kEntryBytes;
    plan.indexFNV1a = 2166136261U;
    for (uint32_t i = 0U; i < plan.entryCount; ++i) {
        EspAssetPackEntry entry = {};
        if (sourceFile.read(raw, sizeof(raw)) != sizeof(raw)) {
            sourceFile.close();
            if (outReason != nullptr) *outReason = "source-index-read";
            return false;
        }
        plan.indexFNV1a = fnv1aUpdate(plan.indexFNV1a, raw, sizeof(raw));
        decodeEntry(raw, &entry);
        if (entry.nameHash == 0U ||
            entry.offset < plan.sourceDataOffset ||
            entry.offset > plan.sourcePackBytes ||
            entry.size > plan.sourcePackBytes - entry.offset ||
            (havePreviousHash && entry.nameHash <= previousHash)) {
            sourceFile.close();
            if (outReason != nullptr) *outReason = "source-index-entry";
            return false;
        }
        previousHash = entry.nameHash;
        havePreviousHash = true;

        if (entry.nameHash == plan.currentMapHash) {
            if ((entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
                entry.size == 0U) {
                sourceFile.close();
                if (outReason != nullptr) *outReason = "target-bsp-invalid";
                return false;
            }
            const uint32_t currentSlot =
                (uint32_t)(targetMapId - ESP_MAP_CATALOG_FIRST_ID);
            plan.bspSourceOffset[currentSlot] = entry.offset;
            plan.bspBytes[currentSlot] = entry.size;
            currentFound = true;
            continue;
        }

        for (uint8_t mapId = ESP_MAP_CATALOG_FIRST_ID;
             mapId <= ESP_MAP_CATALOG_LAST_ID; ++mapId) {
            if (mapId == targetMapId) continue;
            const uint32_t slot =
                (uint32_t)(mapId - ESP_MAP_CATALOG_FIRST_ID);
            if (entry.nameHash != catalogHashes[slot]) continue;
            if ((entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
                entry.size == 0U) {
                sourceFile.close();
                if (outReason != nullptr) *outReason = "catalog-bsp-invalid";
                return false;
            }
            plan.bspSourceOffset[slot] = entry.offset;
            plan.bspBytes[slot] = entry.size;
            break;
        }
    }
    sourceFile.close();

    if (!currentFound) {
        if (outReason != nullptr) *outReason = "target-bsp-missing";
        return false;
    }

    /* Inventory from the index alone: no extra SD payload reads. */
    for (uint32_t i = 0U; i < ESP_MAP_CATALOG_COUNT; ++i) {
        const uint32_t size = plan.bspBytes[i];
        if (size == 0U) continue; /* Catalog BSP absent from this PAK. */
        if (plan.allBspBytes > UINT32_MAX - size) {
            if (outReason != nullptr) *outReason = "all-bsp-overflow";
            return false;
        }
        plan.allBspBytes += size;
        if (size > plan.maxBspBytes) plan.maxBspBytes = size;
        const uint64_t begin = plan.bspSourceOffset[i];
        const uint64_t end = begin + size;
        for (uint32_t j = 0U; j < i; ++j) {
            if (plan.bspBytes[j] == 0U) continue;
            const uint64_t otherBegin = plan.bspSourceOffset[j];
            const uint64_t otherEnd = otherBegin + plan.bspBytes[j];
            if (begin < otherEnd && otherBegin < end) {
                if (outReason != nullptr) *outReason = "bsp-overlap";
                return false;
            }
        }
    }

    plan.sourceDataBytes = plan.sourcePackBytes - plan.sourceDataOffset;
    if (plan.allBspBytes > plan.sourceDataBytes) {
        if (outReason != nullptr) *outReason = "bsp-bytes-invalid";
        return false;
    }
    plan.commonBytes = plan.sourceDataBytes - plan.allBspBytes;

    plan.payloadFlashOffset =
        alignSector(kMapFlashIndexOffset + plan.indexBytes);
    plan.stagedBytes = plan.sourceDataBytes;
    if (plan.payloadFlashOffset == 0U ||
        plan.payloadFlashOffset > partition->size ||
        plan.stagedBytes > partition->size - plan.payloadFlashOffset) {
        if (outReason != nullptr) *outReason = "complete-pak-does-not-fit";
        return false;
    }

    *outPlan = plan;
    if (outReason != nullptr) *outReason = "ok";
    return true;
}


/* MAPFLASH-V2 P0: dry-run capacity, no writes, no extra index scan and no
 * mutation of V1 state. All-BSP calculation is an optimistic lower bound:
 * a persistent multi-BSP cache would need a separate manifest. */
void printMapFlashV2Audit(const MapFlashPlan& plan,
                          const esp_partition_t* partition)
{
    static uint32_t lastPackBytes = 0U;
    static uint32_t lastIndexFNV = 0U;
    if (partition == nullptr ||
        (lastPackBytes == plan.sourcePackBytes &&
         lastIndexFNV == plan.indexFNV1a)) return;
    const uint64_t sector = kMapFlashSectorBytes;
    const uint64_t indexOffset = 2ULL * sector; /* two independent headers */
    const uint64_t commonOffset =
        (indexOffset + plan.indexBytes + sector - 1U) & ~(sector - 1U);
    const uint64_t bspOffset =
        (commonOffset + plan.commonBytes + sector - 1U) & ~(sector - 1U);
    const uint64_t bspReserve =
        ((uint64_t)plan.maxBspBytes + sector - 1U) & ~(sector - 1U);
    const uint64_t singleEnd = bspOffset + bspReserve;
    const uint64_t allEnd = bspOffset + plan.allBspBytes;
    const uint32_t singleFits = singleEnd <= partition->size ? 1U : 0U;
    const uint32_t allLowerBoundFits = allEnd <= partition->size ? 1U : 0U;
    const uint32_t headroom = singleFits
        ? (uint32_t)(partition->size - singleEnd) : 0U;
    uint32_t present = 0U;
    for (uint32_t i = 0U; i < ESP_MAP_CATALOG_COUNT; ++i)
        if (plan.bspBytes[i] != 0U) ++present;

    printf("[MAPFLASHV2] AUDIT scope=plan-only indexFNV=%08x pack=%u index=%u common=%u allBsp=%u maxBsp=%u present=%u/%u partition=%u commonOffset=%llu bspOffset=%llu slotReserve=%llu headroom=%u singleFits=%u allFitsLowerBound=%u\n",
           (unsigned int)plan.indexFNV1a,
           (unsigned int)plan.sourcePackBytes,
           (unsigned int)plan.indexBytes,
           (unsigned int)plan.commonBytes,
           (unsigned int)plan.allBspBytes,
           (unsigned int)plan.maxBspBytes,
           (unsigned int)present, (unsigned int)ESP_MAP_CATALOG_COUNT,
           (unsigned int)partition->size,
           (unsigned long long)commonOffset,
           (unsigned long long)bspOffset,
           (unsigned long long)bspReserve,
           (unsigned int)headroom,
           (unsigned int)singleFits,
           (unsigned int)allLowerBoundFits);
    for (uint8_t mapId = ESP_MAP_CATALOG_FIRST_ID;
         mapId <= ESP_MAP_CATALOG_LAST_ID; ++mapId) {
        const uint32_t slot = mapId - ESP_MAP_CATALOG_FIRST_ID;
        printf("[MAPFLASHV2] BSP id=%u name=%s source=%u bytes=%u present=%u\n",
               (unsigned int)mapId,
               EspMapCatalog_nameForId(mapId),
               (unsigned int)plan.bspSourceOffset[slot],
               (unsigned int)plan.bspBytes[slot],
               plan.bspBytes[slot] != 0U ? 1U : 0U);
    }
    lastPackBytes = plan.sourcePackBytes;
    lastIndexFNV = plan.indexFNV1a;
}

bool mapFlashHeaderMatchesPlan(const MapFlashHeader& header,
                               const MapFlashPlan& plan,
                               const char** outReason)
{
    if (outReason != nullptr) *outReason = "header-invalid";
    if (header.sourcePackBytes != plan.sourcePackBytes ||
        header.sourceIndexOffset != plan.sourceIndexOffset ||
        header.sourceDataOffset != plan.sourceDataOffset ||
        header.entryCount != plan.entryCount ||
        header.indexBytes != plan.indexBytes ||
        header.indexFNV1a != plan.indexFNV1a) {
        if (outReason != nullptr) *outReason = "source-identity";
        return false;
    }
    if (header.payloadFlashOffset != plan.payloadFlashOffset ||
        header.stagedBytes != plan.stagedBytes) {
        if (outReason != nullptr) *outReason = "pack-layout";
        return false;
    }
    if (outReason != nullptr) *outReason = "match";
    return true;
}

bool readEntryAt(uint32_t index, EspAssetPackEntry* outEntry)
{
    uint8_t raw[kEntryBytes];

    if (outEntry == nullptr || index >= entryCount) {
        return false;
    }

    if (physicalUsesMapFlash) {
        const uint64_t flashOffset64 =
            (uint64_t)mapFlash.header.indexFlashOffset +
            ((uint64_t)index * (uint64_t)kEntryBytes);
        if (flashOffset64 > UINT32_MAX ||
            !mapFlashRead((uint32_t)flashOffset64, raw, sizeof(raw), true)) {
            return false;
        }
    }
    else {
        if (!packFile) return false;
        const uint64_t absoluteOffset =
            (uint64_t)indexOffset + ((uint64_t)index * (uint64_t)kEntryBytes);
        if (absoluteOffset > UINT32_MAX ||
            !packFile.seek((uint32_t)absoluteOffset) ||
            !readSdExact(raw, sizeof(raw))) {
            return false;
        }
    }

    decodeEntry(raw, outEntry);
    return validateEntry(*outEntry);
}

uint8_t normalizeNameByte(uint8_t value)
{
    if (value == '\\') {
        return '/';
    }
    if (value >= 'A' && value <= 'Z') {
        return (uint8_t)(value + ('a' - 'A'));
    }
    return value;
}

bool isDefaultPath(const char* path)
{
    return path != nullptr && strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) == 0;
}

bool isLargeResidentRange(const ResidentRangeRecord& record)
{
    return record.length == kResidentLargeRangeBytes;
}

uint32_t residentLargeRangeCount()
{
    return residentCache != nullptr ? residentCache->largeCount : 0U;
}

uint32_t residentPayloadBytesUsed()
{
    if (residentCache == nullptr) {
        return 0U;
    }
    return residentCache->bytesUsed +
           residentLargeRangeCount() * kResidentLargeRangeBytes;
}

void eraseResidentRange(uint32_t index)
{
    if (residentCache == nullptr || index >= residentCache->rangeCount) {
        return;
    }
    if (index + 1U < residentCache->rangeCount) {
        memmove(&residentCache->ranges[index],
                &residentCache->ranges[index + 1U],
                (residentCache->rangeCount - index - 1U) *
                    sizeof(residentCache->ranges[0]));
    }
    --residentCache->rangeCount;
}

uint32_t lowestLargeRangeOffset()
{
    /* Large blocks are appended downwards and evicted from the low end.
     * There are no holes; the count is owned by these same mutations. */
    return kResidentRangeCapacityBytes -
           residentLargeRangeCount() * kResidentLargeRangeBytes;
}

bool evictLowestLargeRange()
{
    if (residentCache == nullptr || residentCache->largeCount == 0U ||
        residentCache->largeCount > residentCache->rangeCount) {
        return false;
    }
    const uint32_t last = residentCache->rangeCount - 1U;
    const ResidentRangeRecord& record = residentCache->ranges[last];
    if (!isLargeResidentRange(record) ||
        record.dataOffset != lowestLargeRangeOffset()) {
        return false;
    }
    /* The lowest large payload is always the last descriptor. No shifting. */
    eraseResidentRange(last);
    --residentCache->largeCount;
    return true;
}

void clearLargeResidentRanges()
{
    if (residentCache == nullptr) {
        return;
    }
    /* The two descriptor cohorts never interleave. */
    residentCache->rangeCount -= residentCache->largeCount;
    residentCache->largeCount = 0U;
}

void recycleResidentRangeWorkingSet()
{
    if (residentCache == nullptr) {
        return;
    }
    residentCache->rangeCount = 0U;
    residentCache->bytesUsed = 0U;
    residentCache->largeCount = 0U;
}

/* P3f: small descriptors are sorted by (absolute offset, length).
 * Payload placement and the insertion-ordered large suffix are independent.
 * Lower bound returns the oldest equal key; upper bound inserts after equals.
 * At capacity 256 this takes at most nine binary-search iterations. */
uint32_t residentSmallRangeBound(uint32_t absoluteOffset,
                                 uint32_t length, bool afterEqual)
{
    uint32_t lo = 0U;
    uint32_t hi = residentCache->rangeCount - residentCache->largeCount;
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2U;
        const ResidentRangeRecord& record = residentCache->ranges[mid];
        if (record.absoluteOffset < absoluteOffset ||
            (record.absoluteOffset == absoluteOffset &&
             (record.length < length ||
              (afterEqual && record.length == length)))) {
            lo = mid + 1U;
        } else {
            hi = mid;
        }
    }
    return lo;
}

ResidentRangeRecord* findResidentRange(uint32_t absoluteOffset,
                                       uint32_t length)
{
    if (!residentEnabled || residentCache == nullptr) {
        return nullptr;
    }
    const uint32_t smallCount =
        residentCache->rangeCount - residentCache->largeCount;
    if (length != kResidentLargeRangeBytes) {
        const uint32_t i = residentSmallRangeBound(absoluteOffset, length, false);
        if (i < smallCount) {
            ResidentRangeRecord* record = &residentCache->ranges[i];
            if (record->absoluteOffset == absoluteOffset &&
                record->length == length) {
                return record;
            }
        }
        return nullptr;
    }
    for (uint32_t i = smallCount; i < residentCache->rangeCount; ++i) {
        ResidentRangeRecord* record = &residentCache->ranges[i];
        if (record->absoluteOffset == absoluteOffset &&
            record->length == length) {
            return record;
        }
    }
    return nullptr;
}

void storeResidentSmallRange(uint32_t absoluteOffset,
                             const void* source,
                             uint32_t length)
{
    bool recycled = false;

    if (!residentEnabled || residentCache == nullptr || source == nullptr ||
        length == 0U || length > kResidentMaxCachedRangeBytes) {
        ++residentStats.rangeCacheBypasses;
        return;
    }

    for (;;) {
        const uint32_t largeBoundary = lowestLargeRangeOffset();
        const bool recordFull =
            residentCache->rangeCount >= kResidentRangeEntryCapacity;
        const bool payloadFull =
            largeBoundary < residentCache->bytesUsed ||
            length > largeBoundary - residentCache->bytesUsed;
        if (!recordFull && !payloadFull) {
            break;
        }
        if (evictLowestLargeRange()) {
            continue;
        }
        if (!recycled) {
            recycleResidentRangeWorkingSet();
            recycled = true;
            continue;
        }
        ++residentStats.rangeCacheBypasses;
        return;
    }

    /* Insert into the sorted small prefix. At most 255 descriptors move
     * on a miss, never their payloads. The large suffix keeps its order;
     * admission, eviction, recycle and sequential payload offsets are unchanged. */
    const uint32_t index = residentSmallRangeBound(absoluteOffset, length, true);
    if (index < residentCache->rangeCount) {
        memmove(&residentCache->ranges[index + 1U],
                &residentCache->ranges[index],
                (residentCache->rangeCount - index) * sizeof(residentCache->ranges[0]));
    }
    ++residentCache->rangeCount;
    ResidentRangeRecord* record = &residentCache->ranges[index];
    record->absoluteOffset = absoluteOffset;
    record->length = (uint16_t)length;
    record->dataOffset = (uint16_t)residentCache->bytesUsed;
    memcpy(residentCache->bytes + record->dataOffset, source, length);
    residentCache->bytesUsed += length;
    ++residentStats.rangeCacheStores;
}

void storeResidentLargeRange(uint32_t absoluteOffset,
                             const void* source,
                             uint32_t length)
{
    uint32_t boundary;
    uint32_t targetOffset;

    if (!residentEnabled || !residentLargeRangeEnabled ||
        residentCache == nullptr || source == nullptr ||
        length != kResidentLargeRangeBytes ||
        residentCache->rangeCount >= kResidentRangeEntryCapacity) {
        ++residentStats.rangeCacheBypasses;
        return;
    }

    boundary = lowestLargeRangeOffset();
    if (boundary < kResidentLargeRangeBytes) {
        ++residentStats.rangeCacheBypasses;
        return;
    }
    targetOffset = boundary - kResidentLargeRangeBytes;
    if (targetOffset < residentCache->bytesUsed) {
        ++residentStats.rangeCacheBypasses;
        return;
    }

    ResidentRangeRecord* record =
        &residentCache->ranges[residentCache->rangeCount++];
    record->absoluteOffset = absoluteOffset;
    record->length = (uint16_t)length;
    record->dataOffset = (uint16_t)targetOffset;
    memcpy(residentCache->bytes + targetOffset, source, length);
    ++residentCache->largeCount;
    ++residentStats.rangeCacheStores;
}

bool copySdRangeToFlash(uint32_t sourceOffset,
                        uint32_t length,
                        uint32_t flashOffset,
                        uint8_t* buffer,
                        uint32_t* ioFNV,
                        uint32_t* progressDone,
                        uint32_t progressTotal)
{
    if (!packFile || mapFlash.partition == nullptr || buffer == nullptr ||
        flashOffset > mapFlash.partition->size ||
        length > mapFlash.partition->size - flashOffset) {
        return false;
    }

    uint32_t remaining = length;
    uint32_t sourceCursor = sourceOffset;
    uint32_t destinationOffset = flashOffset;
    while (remaining > 0U) {
        const uint32_t chunk =
            remaining > kMapFlashCopyBufferBytes
                ? kMapFlashCopyBufferBytes
                : remaining;

        /*
         * Progress presentation intentionally reads c.bmp through the same
         * already-open SD PAK between copy chunks. That read moves packFile's
         * cursor. Never rely on sequential File.read() position across a
         * progress callback: re-anchor every chunk to its authoritative source
         * offset before reading it.
         */
        if (!packFile.seek(sourceCursor)) {
            return false;
        }
        const size_t got = packFile.read(buffer, chunk);
        if (got != chunk ||
            esp_partition_write(mapFlash.partition,
                                destinationOffset,
                                buffer,
                                chunk) != ESP_OK) {
            return false;
        }
        if (ioFNV != nullptr) {
            *ioFNV = fnv1aUpdate(*ioFNV, buffer, chunk);
        }
        sourceCursor += chunk;
        destinationOffset += chunk;
        remaining -= chunk;
        if (progressDone != nullptr) {
            *progressDone += chunk;
            notifyMapFlashProgress(ESP_ASSET_PACK_MAP_FLASH_PROGRESS_COPY,
                                   *progressDone, progressTotal);
        }
    }
    return true;
}

bool fnvFlashRange(uint32_t flashOffset,
                   uint32_t length,
                   uint8_t* buffer,
                   uint32_t* outFNV)
{
    if (mapFlash.partition == nullptr || buffer == nullptr || outFNV == nullptr ||
        flashOffset > mapFlash.partition->size ||
        length > mapFlash.partition->size - flashOffset) {
        return false;
    }

    uint32_t hash = 2166136261U;
    uint32_t remaining = length;
    uint32_t offset = flashOffset;
    while (remaining > 0U) {
        const uint32_t chunk =
            remaining > kMapFlashCopyBufferBytes
                ? kMapFlashCopyBufferBytes
                : remaining;
        if (esp_partition_read(mapFlash.partition, offset, buffer, chunk) != ESP_OK) {
            return false;
        }
        hash = fnv1aUpdate(hash, buffer, chunk);
        offset += chunk;
        remaining -= chunk;
    }
    *outFNV = hash;
    return true;
}

bool fnvFlashRangeProgress(uint32_t flashOffset,
                           uint32_t length,
                           uint8_t* buffer,
                           uint32_t* outFNV,
                           uint32_t* progressDone,
                           uint32_t progressTotal)
{
    if (mapFlash.partition == nullptr || buffer == nullptr || outFNV == nullptr ||
        flashOffset > mapFlash.partition->size ||
        length > mapFlash.partition->size - flashOffset) {
        return false;
    }

    uint32_t hash = 2166136261U;
    uint32_t remaining = length;
    uint32_t offset = flashOffset;
    while (remaining > 0U) {
        const uint32_t chunk =
            remaining > kMapFlashCopyBufferBytes
                ? kMapFlashCopyBufferBytes
                : remaining;
        if (esp_partition_read(mapFlash.partition, offset, buffer, chunk) != ESP_OK) {
            return false;
        }
        hash = fnv1aUpdate(hash, buffer, chunk);
        offset += chunk;
        remaining -= chunk;
        if (progressDone != nullptr) {
            *progressDone += chunk;
            notifyMapFlashProgress(ESP_ASSET_PACK_MAP_FLASH_PROGRESS_VERIFY,
                                   *progressDone, progressTotal);
        }
    }
    *outFNV = hash;
    return true;
}

// P4a: ZIP/PAK CRC32 (reflected IEEE), bounded by the existing copy buffer.
uint32_t mapFlashCrc32Update(uint32_t crc, const uint8_t* bytes, size_t length)
{
    static constexpr uint32_t nibble[16] = {
        0x00000000U,0x1db71064U,0x3b6e20c8U,0x26d930acU,
        0x76dc4190U,0x6b6b51f4U,0x4db26158U,0x5005713cU,
        0xedb88320U,0xf00f9344U,0xd6d6a3e8U,0xcb61b38cU,
        0x9b64c2b0U,0x86d3d2d4U,0xa00ae278U,0xbdbdf21cU
    };
    while (length-- != 0U) {
        crc ^= *bytes++;
        crc = (crc >> 4U) ^ nibble[crc & 15U];
        crc = (crc >> 4U) ^ nibble[crc & 15U];
    }
    return crc;
}

template <typename Read>
bool mapFlashValidateEntryCrcs(const MapFlashPlan& plan, uint8_t* buffer,
                               Read read, const char* backing)
{
    uint32_t indexFNV = 2166136261U, previousHash = 0U;
    uint32_t files = 0U, dirs = 0U;
    uint64_t bytes = 0U;
    const int64_t start = esp_timer_get_time();
    if (buffer == nullptr || plan.entryCount == 0U ||
        plan.entryCount > ESP_ASSET_PACK_MAX_ENTRY_COUNT ||
        plan.sourceDataOffset > plan.sourcePackBytes ||
        (uint64_t)plan.sourceIndexOffset +
            (uint64_t)plan.entryCount * kEntryBytes > plan.sourceDataOffset)
        return false;
    for (uint32_t i = 0U; i < plan.entryCount; ++i) {
        uint8_t raw[kEntryBytes];
        EspAssetPackEntry entry = {};
        if (!read(plan.sourceIndexOffset + i * (uint32_t)kEntryBytes,
                  raw, sizeof(raw))) return false;
        indexFNV = fnv1aUpdate(indexFNV, raw, sizeof(raw));
        decodeEntry(raw, &entry);
        if (entry.nameHash == 0U || (i != 0U && entry.nameHash <= previousHash) ||
            entry.offset < plan.sourceDataOffset || entry.offset > plan.sourcePackBytes ||
            entry.size > plan.sourcePackBytes - entry.offset) return false;
        previousHash = entry.nameHash;
        if ((entry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U) {
            if (entry.size != 0U || entry.crc32 != 0U) return false;
            ++dirs;
            continue;
        }
        uint32_t crc = UINT32_MAX;
        for (uint32_t done = 0U; done < entry.size;) {
            const uint32_t left = entry.size - done;
            const uint32_t chunk = left > kMapFlashCopyBufferBytes
                                     ? kMapFlashCopyBufferBytes : left;
            if (!read(entry.offset + done, buffer, chunk)) return false;
            crc = mapFlashCrc32Update(crc, buffer, chunk);
            done += chunk;
        }
        crc ^= UINT32_MAX;
        if (crc != entry.crc32) {
            printf("[MAPFLASHV4] CRC-REJECT backing=%s hash=%08x expected=%08x actual=%08x\n",
                   backing, (unsigned int)entry.nameHash,
                   (unsigned int)entry.crc32, (unsigned int)crc);
            return false;
        }
        ++files;
        bytes += entry.size;
    }
    if (indexFNV != plan.indexFNV1a) return false;
    printf("[MAPFLASHV4] CRC-PASS backing=%s entries=%u files=%u dirs=%u bytes=%llu us=%u scratch=%u\n",
           backing, (unsigned int)plan.entryCount, (unsigned int)files,
           (unsigned int)dirs, (unsigned long long)bytes,
           (unsigned int)(esp_timer_get_time() - start),
           (unsigned int)kMapFlashCopyBufferBytes);
    return true;
}

// CRC proof is boot-local and only reusable after the existing full FNV check.
// No on-flash format change or extra allocation. Never survives a stage writer.
struct MapFlashCrcProof {
    const esp_partition_t* partition;
    uint32_t packBytes, indexOffset, dataOffset, indexFNV, payloadFNV;
};
MapFlashCrcProof mapFlashCrcProof = {};

bool mapFlashEnsureEntryCrcs(const MapFlashPlan& plan, uint32_t payloadFNV,
                             uint8_t* buffer)
{
    if (mapFlash.partition != nullptr &&
        mapFlashCrcProof.partition == mapFlash.partition &&
        mapFlashCrcProof.packBytes == plan.sourcePackBytes &&
        mapFlashCrcProof.indexOffset == plan.sourceIndexOffset &&
        mapFlashCrcProof.dataOffset == plan.sourceDataOffset &&
        mapFlashCrcProof.indexFNV == plan.indexFNV1a &&
        mapFlashCrcProof.payloadFNV == payloadFNV) {
        printf("[MAPFLASHV4] CRC-REUSE scope=boot-identity writes=0\n");
        return true;
    }
    mapFlashCrcProof = {};
    if (mapFlash.partition == nullptr) return false;
    auto read = [&](uint32_t offset, void* dst, size_t length) -> bool {
        uint32_t flashOffset;
        if (offset >= plan.sourceDataOffset &&
            offset <= plan.sourcePackBytes && length <= plan.sourcePackBytes - offset) {
            flashOffset = plan.payloadFlashOffset + offset - plan.sourceDataOffset;
        } else if (offset >= plan.sourceIndexOffset &&
                   offset - plan.sourceIndexOffset <= plan.indexBytes &&
                   length <= plan.indexBytes - (offset - plan.sourceIndexOffset)) {
            flashOffset = kMapFlashIndexOffset + offset - plan.sourceIndexOffset;
        } else return false;
        return flashOffset <= mapFlash.partition->size &&
               length <= mapFlash.partition->size - flashOffset &&
               esp_partition_read(mapFlash.partition, flashOffset, dst, length) == ESP_OK;
    };
    if (!mapFlashValidateEntryCrcs(plan, buffer, read, "flash")) return false;
    mapFlashCrcProof = {mapFlash.partition, plan.sourcePackBytes,
                       plan.sourceIndexOffset, plan.sourceDataOffset,
                       plan.indexFNV1a, payloadFNV};
    return true;
}

bool mapFlashTranslate(uint32_t sourceOffset,
                       uint32_t length,
                       uint32_t* outFlashOffset)
{
    if (!mapFlash.active || outFlashOffset == nullptr ||
        sourceOffset < mapFlash.header.sourceDataOffset ||
        sourceOffset > mapFlash.header.sourcePackBytes ||
        length > mapFlash.header.sourcePackBytes - sourceOffset) {
        return false;
    }

    const uint32_t compactOffset = sourceOffset - mapFlash.header.sourceDataOffset;
    if (compactOffset > mapFlash.header.stagedBytes ||
        length > mapFlash.header.stagedBytes - compactOffset) {
        return false;
    }
    *outFlashOffset = mapFlash.header.payloadFlashOffset + compactOffset;
    return true;
}

bool readSourceProbePayload(uint32_t absoluteOffset, void* destination,
                            size_t length)
{
    if (sourceProbeFlashEpoch != 0U) {
        uint32_t flashOffset = 0U;
        if (sourceProbeFlashEpoch != verifiedFlashEpoch ||
            !mapFlash.stats.verified ||
            !mapFlashTranslate(absoluteOffset, (uint32_t)length, &flashOffset) ||
            !mapFlashRead(flashOffset, destination, length, false)) {
            printf("[MAPFLASHV3] BSP-PROBE-FAILED reason=lease-or-read fallbackSD=no\n");
            return false;
        }
        return true;
    }
    return sourceProbeFile.seek(absoluteOffset) &&
           sourceProbeFile.read(static_cast<uint8_t*>(destination), length) == length;
}

void logMapFlashStrictMiss(const EspAssetPackEntry* entry,
                           uint32_t relativeOffset,
                           size_t length)
{
    ++mapFlash.stats.strictMisses;
    if (mapFlash.missLogs < kMapFlashMaxMissLogs) {
        ++mapFlash.missLogs;
        printf("[MAPFLASH] MISS strict=1 hash=%08x source=%u relative=%u length=%u map=%u fallbackSD=no\n",
               entry != nullptr ? (unsigned int)entry->nameHash : 0U,
               entry != nullptr ? (unsigned int)entry->offset : 0U,
               (unsigned int)relativeOffset,
               (unsigned int)length,
               (unsigned int)mapFlash.header.currentMapId);
    }
}

} // namespace

int EspAssetPack_open(const char* path)
{
    uint8_t header[kHeaderBytes];
    uint32_t previousHash = 0U;
    bool havePreviousHash = false;

    if (path == nullptr || path[0] == '\0') {
        return 0;
    }

    if (residentEnabled) {
        ++residentStats.logicalOpens;
    }

    if (residentEnabled && physicalReady && physicalIsDefault &&
        isDefaultPath(path) && !openReady) {
        openReady = true;
        ++residentStats.residentReuses;
        return 1;
    }

    resetPhysicalState();

    if (isDefaultPath(path) && mapFlash.active) {
        if (mapFlash.partition == nullptr ||
            !mapFlashHeaderLooksValid(mapFlash.header, mapFlash.partition)) {
            return 0;
        }
        packSize = mapFlash.header.sourcePackBytes;
        entryCount = mapFlash.header.entryCount;
        indexOffset = mapFlash.header.sourceIndexOffset;
        dataOffset = mapFlash.header.sourceDataOffset;
        physicalUsesMapFlash = true;
        if (residentEnabled) {
            ++residentStats.physicalOpens;
        }

        for (uint32_t i = 0U; i < entryCount; ++i) {
            EspAssetPackEntry entry;
            if (!readEntryAt(i, &entry) ||
                (havePreviousHash && entry.nameHash <= previousHash)) {
                resetPhysicalState();
                return 0;
            }
            previousHash = entry.nameHash;
            havePreviousHash = true;
        }

        physicalReady = true;
        physicalIsDefault = true;
        openReady = true;
        if (residentEnabled) {
            ++residentStats.validationPasses;
        }
        return 1;
    }

    suspendFontSdBeforePackOpen();
    packFile = SD.open(path, FILE_READ);
    if (!packFile) {
        return 0;
    }
    if (residentEnabled) {
        ++residentStats.physicalOpens;
    }

    packSize = (uint32_t)packFile.size();
    if (packSize < kHeaderBytes || !readSdExact(header, sizeof(header))) {
        resetPhysicalState();
        return 0;
    }

    if (memcmp(header, kMagic, sizeof(kMagic)) != 0 ||
        readLe32(header + 8) != kVersion) {
        resetPhysicalState();
        return 0;
    }

    entryCount = readLe32(header + 12);
    indexOffset = readLe32(header + 16);
    dataOffset = readLe32(header + 20);

    if (entryCount == 0U || entryCount > ESP_ASSET_PACK_MAX_ENTRY_COUNT ||
        indexOffset < kHeaderBytes || indexOffset > packSize) {
        resetPhysicalState();
        return 0;
    }

    const uint64_t indexEnd =
        (uint64_t)indexOffset + ((uint64_t)entryCount * (uint64_t)kEntryBytes);
    if (indexEnd > packSize || dataOffset < indexEnd || dataOffset > packSize) {
        resetPhysicalState();
        return 0;
    }

    if (!packFile.seek(indexOffset)) {
        resetPhysicalState();
        return 0;
    }

    for (uint32_t i = 0U; i < entryCount; ++i) {
        uint8_t raw[kEntryBytes];
        EspAssetPackEntry entry;

        if (!readSdExact(raw, sizeof(raw))) {
            resetPhysicalState();
            return 0;
        }
        decodeEntry(raw, &entry);

        if (!validateEntry(entry) ||
            (havePreviousHash && entry.nameHash <= previousHash)) {
            resetPhysicalState();
            return 0;
        }

        previousHash = entry.nameHash;
        havePreviousHash = true;
    }

    physicalReady = true;
    physicalIsDefault = isDefaultPath(path);
    physicalUsesMapFlash = false;
    openReady = true;
    if (residentEnabled) {
        ++residentStats.validationPasses;
    }
    return 1;
}

void EspAssetPack_close(void)
{
    if (residentEnabled && physicalReady && physicalIsDefault) {
        openReady = false;
        return;
    }
    resetPhysicalState();
}

int EspAssetPack_isOpen(void)
{
    return openReady ? 1 : 0;
}

int EspAssetPack_musicSdSafe(void)
{
    return !openReady || (physicalUsesMapFlash && !sourceProbeReady);
}

uint32_t EspAssetPack_fileSize(void)
{
    return openReady ? packSize : 0U;
}

int EspAssetPack_entryCount(void)
{
    return openReady ? (int)entryCount : 0;
}

uint32_t EspAssetPack_dataOffset(void)
{
    return openReady ? dataOffset : 0U;
}

int EspAssetPack_sourceProbeBegin(const char* resourceName)
{
    uint8_t header[kHeaderBytes];
    uint8_t raw[kEntryBytes];
    EspAssetPackEntry found = {};
    uint32_t sourcePackBytes;
    uint32_t sourceEntryCount;
    uint32_t sourceIndexOffset;
    uint32_t sourceDataOffset;
    uint32_t sourceIndexFNV = 2166136261U;
    uint32_t foundIndex = 0U;
    uint32_t previousHash = 0U;
    const uint32_t wantedHash = EspAssetPack_nameHash(resourceName);
    bool havePreviousHash = false;
    bool foundEntry = false;

    if (resourceName == nullptr || resourceName[0] == '\0' ||
        wantedHash == 0U || openReady || sourceProbeReady) {
        return 0;
    }

    clearSourceProbe();
    suspendFontSdBeforePackOpen();
    sourceProbeFile = SD.open(ESP_ASSET_PACK_DEFAULT_PATH, FILE_READ);
    if (!sourceProbeFile) {
        return 0;
    }

    sourcePackBytes = (uint32_t)sourceProbeFile.size();
    if (sourcePackBytes < kHeaderBytes ||
        sourceProbeFile.read(header, sizeof(header)) != sizeof(header) ||
        memcmp(header, kMagic, sizeof(kMagic)) != 0 ||
        readLe32(header + 8) != kVersion) {
        clearSourceProbe();
        return 0;
    }

    sourceEntryCount = readLe32(header + 12);
    sourceIndexOffset = readLe32(header + 16);
    sourceDataOffset = readLe32(header + 20);
    const uint64_t indexEnd =
        (uint64_t)sourceIndexOffset +
        ((uint64_t)sourceEntryCount * (uint64_t)kEntryBytes);
    if (sourceEntryCount == 0U ||
        sourceEntryCount > ESP_ASSET_PACK_MAX_ENTRY_COUNT ||
        sourceIndexOffset < kHeaderBytes ||
        sourceIndexOffset > sourcePackBytes ||
        indexEnd > sourcePackBytes ||
        sourceDataOffset < indexEnd ||
        sourceDataOffset > sourcePackBytes ||
        !sourceProbeFile.seek(sourceIndexOffset)) {
        clearSourceProbe();
        return 0;
    }

    for (uint32_t i = 0U; i < sourceEntryCount; ++i) {
        EspAssetPackEntry entry = {};
        if (sourceProbeFile.read(raw, sizeof(raw)) != sizeof(raw)) {
            clearSourceProbe();
            return 0;
        }
        sourceIndexFNV = fnv1aUpdate(sourceIndexFNV, raw, sizeof(raw));
        decodeEntry(raw, &entry);
        if (entry.nameHash == 0U ||
            entry.offset < sourceDataOffset ||
            entry.offset > sourcePackBytes ||
            entry.size > sourcePackBytes - entry.offset ||
            (havePreviousHash && entry.nameHash <= previousHash)) {
            clearSourceProbe();
            return 0;
        }
        previousHash = entry.nameHash;
        havePreviousHash = true;
        if (entry.nameHash == wantedHash) {
            found = entry;
            foundIndex = i;
            foundEntry = true;
        }
    }

    if (!foundEntry) {
        clearSourceProbe();
        return 0;
    }

    /* P3: the SD index remains authoritative. Only catalog BSPs in the
     * already verified immutable V2 lease can read their payload in flash.
     * Keep the exact-entry probe and bypass L1; BSP stream CRC still runs.
     * An inactive cache or a changed SD identity uses the original explicit
     * SD preflight path, never a fallback after a flash read failure. */
    uint8_t bspMapId = 0U;
    if (mapFlash.active && mapFlash.stats.verified &&
        verifiedFlashEpoch != UINT32_MAX &&
        EspMapCatalog_idForName(resourceName, &bspMapId) &&
        mapFlashHeaderLooksValid(mapFlash.header, mapFlash.partition) &&
        mapFlash.header.sourcePackBytes == sourcePackBytes &&
        mapFlash.header.sourceIndexOffset == sourceIndexOffset &&
        mapFlash.header.sourceDataOffset == sourceDataOffset &&
        mapFlash.header.entryCount == sourceEntryCount &&
        mapFlash.header.indexFNV1a == sourceIndexFNV) {
        EspAssetPackEntry flashEntry = {};
        if (!mapFlashRead(mapFlash.header.indexFlashOffset +
                              foundIndex * (uint32_t)kEntryBytes,
                          raw, sizeof(raw), false)) {
            clearSourceProbe();
            return 0;
        }
        decodeEntry(raw, &flashEntry);
        if (flashEntry.nameHash != found.nameHash ||
            flashEntry.offset != found.offset || flashEntry.size != found.size ||
            flashEntry.crc32 != found.crc32 || flashEntry.flags != found.flags ||
            (found.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
            found.size == 0U) {
            clearSourceProbe();
            return 0;
        }
        sourceProbeFlashEpoch = verifiedFlashEpoch;
        printf("[MAPFLASHV3] BSP-PROBE map=%u bytes=%u indexFNV=%08x crc32=%08x backing=verified-flash sdPayloadBytes=0 scope=exact-entry\n",
               (unsigned int)bspMapId, (unsigned int)found.size,
               (unsigned int)sourceIndexFNV, (unsigned int)found.crc32);
    }
    sourceProbeEntry = found;
    sourceProbeReady = true;
    sourceProbeReadLogged = 0U;
    printf("[PAKSOURCE] PROBE-OPEN hash=%08x source=%u size=%u crc32=%08x mapFlash=%u resident=%u scope=exact-entry\n",
           (unsigned int)found.nameHash,
           (unsigned int)found.offset,
           (unsigned int)found.size,
           (unsigned int)found.crc32,
           (unsigned int)EspAssetPack_isMapFlashActive(),
           (unsigned int)EspAssetPack_isResident());
    return 1;
}

void EspAssetPack_sourceProbeEnd(void)
{
    if (!sourceProbeReady && !sourceProbeFile) {
        return;
    }
    clearSourceProbe();
}

int EspAssetPack_isSourceProbeActive(void)
{
    return sourceProbeReady && sourceProbeFile ? 1 : 0;
}

int EspAssetPack_mapFlashPrepare(uint8_t targetMapId)
{
    MapFlashPlan plan = {};
    MapFlashHeader cached = {};
    const esp_partition_t* partition = nullptr;
    const char* planReason = nullptr;
    const char* missReason = "header-invalid";
    uint8_t cachedValid = 0U;
    uint8_t cachedMapId = 0U;
    uint8_t* buffer = nullptr;
    uint32_t flashIndexFNV = 0U;
    uint32_t flashPayloadFNV = 0U;
    const int64_t prepareStart = esp_timer_get_time();

    if (openReady || residentEnabled || sourceProbeReady ||
        !EspMapCatalog_isValidId(targetMapId)) {
        mapFlash.handoffEligible = 0U;
        printf("[MAPFLASH] PREPARE FAILED reason=bad-boundary map=%u open=%u resident=%u\n",
               (unsigned int)targetMapId,
               (unsigned int)openReady,
               (unsigned int)residentEnabled);
        return 0;
    }

    /* P2: retain only the immediately preceding verified target's proof.
     * The active owner is the sole writer of this partition; every stage,
     * deactivation and failed prepare clears eligibility before any write.
     * Never carry a proof across a resident teardown or a boot. */
    const bool handoffCandidate =
        mapFlash.handoffEligible && mapFlash.active && mapFlash.stats.verified &&
        mapFlash.stats.currentMapId == targetMapId &&
        mapFlash.header.currentMapId == targetMapId &&
        !sourceProbeReady && verifiedFlashEpoch != UINT32_MAX;
    const esp_partition_t* priorPartition = mapFlash.partition;
    const MapFlashHeader priorHeader = mapFlash.header;
    resetPhysicalState();
    clearMapFlashRuntime(true);
    partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                         ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                                         "spiffs");
    if (partition == nullptr || partition->size <= kMapFlashIndexOffset) {
        printf("[MAPFLASH] PREPARE FAILED reason=raw-partition-missing map=%u\n",
               (unsigned int)targetMapId);
        return 0;
    }
    mapFlash.partition = partition;

    if (!mapFlashBuildSourcePlan(targetMapId,
                                 partition,
                                 &plan,
                                 &planReason)) {
        printf("[MAPFLASH] PREPARE FAILED reason=%s map=%u\n",
               planReason != nullptr ? planReason : "source-plan",
               (unsigned int)targetMapId);
        clearMapFlashRuntime(true);
        return 0;
    }

    printMapFlashV2Audit(plan, partition);

    if (esp_partition_read(partition, 0U, &cached, sizeof(cached)) == ESP_OK &&
        mapFlashHeaderLooksValid(cached, partition)) {
        cachedValid = 1U;
        cachedMapId = cached.currentMapId;
        if (!mapFlashHeaderMatchesPlan(cached,
                                       plan,
                                       &missReason)) {
            /* Exact reason is reported below before rebuilding. */
        }
        else {
            /* Fresh authoritative SD index plan + committed flash header.
             * The previous RAM header differs only in its map hint; the
             * immutable payload identity must agree exactly. Still hash the
             * flash index below, even when adopting the payload proof. */
            const bool adoptPayload =
                handoffCandidate && verifiedFlashEpoch != UINT32_MAX &&
                priorPartition == partition &&
                mapFlashHeaderLooksValid(priorHeader, partition) &&
                mapFlashHeaderMatchesPlan(priorHeader, plan, nullptr) &&
                priorHeader.currentMapHash == plan.currentMapHash &&
                priorHeader.payloadFNV1a == cached.payloadFNV1a;
            buffer = (uint8_t*)malloc(kMapFlashCopyBufferBytes);
            if (buffer == nullptr) {
                printf("[MAPFLASH] PREPARE FAILED reason=verify-buffer-allocation map=%u\n",
                       (unsigned int)targetMapId);
                clearMapFlashRuntime(true);
                return 0;
            }
            if (!fnvFlashRange(cached.indexFlashOffset,
                               cached.indexBytes,
                               buffer,
                               &flashIndexFNV)) {
                missReason = "flash-index-readback";
            }
            else if (flashIndexFNV != cached.indexFNV1a ||
                     flashIndexFNV != plan.indexFNV1a) {
                missReason = "flash-index-fnv";
            }
            else if (!adoptPayload && !fnvFlashRange(cached.payloadFlashOffset,
                                    cached.stagedBytes,
                                    buffer,
                                    &flashPayloadFNV)) {
                missReason = "flash-payload-readback";
            }
            else if (!adoptPayload && flashPayloadFNV != cached.payloadFNV1a) {
                missReason = "flash-payload-fnv";
            }
            else if (!adoptPayload && !mapFlashEnsureEntryCrcs(plan, flashPayloadFNV, buffer)) {
                missReason = "flash-entry-crc";
            }
            else {
                free(buffer);
                buffer = nullptr;
                mapFlash.partition = partition;
                mapFlash.header = cached;
                /* Runtime map identity is not persisted on a cache hit. */
                mapFlash.header.currentMapId = targetMapId;
                mapFlash.header.currentMapHash = plan.currentMapHash;
                mapFlash.active = 1U;
                mapFlash.missLogs = 0U;
                mapFlash.handoffEligible = adoptPayload ? 0U : 1U;
                if (adoptPayload) flashPayloadFNV = cached.payloadFNV1a;
                memset(&mapFlash.stats, 0, sizeof(mapFlash.stats));
                mapFlash.stats.partitionBytes = partition->size;
                mapFlash.stats.sourcePackBytes = plan.sourcePackBytes;
                mapFlash.stats.sourceDataBytes = plan.sourceDataBytes;
                mapFlash.stats.indexBytes = plan.indexBytes;
                mapFlash.stats.metadataBytes = plan.payloadFlashOffset;
                mapFlash.stats.stagedBytes = plan.stagedBytes;
                mapFlash.stats.entryCount =
                    plan.entryCount <= UINT16_MAX
                        ? (uint16_t)plan.entryCount
                        : UINT16_MAX;
                mapFlash.stats.currentMapId = targetMapId;
                mapFlash.stats.active = 1U;
                mapFlash.stats.verified = 1U;
                mapFlash.stats.reused = 1U;
                const uint32_t verifyUs =
                    (uint32_t)(esp_timer_get_time() - prepareStart);
                printf("[MAPFLASH] REUSE HIT requestedMap=%u current=%s cachedMap=%u sourceIndexFNV=%08x payloadFNV=%08x verifyUs=%u rebuild=no\n",
                       (unsigned int)targetMapId,
                       EspMapCatalog_nameForId(targetMapId),
                       (unsigned int)cached.currentMapId,
                       (unsigned int)plan.indexFNV1a,
                       (unsigned int)flashPayloadFNV,
                       (unsigned int)verifyUs);
                if (adoptPayload) {
                    printf("[MAPFLASHV2] HANDOFF map=%u eraseBytes=0 writeBytes=0 verifiedBytes=%u priorPayloadBytes=%u payloadFNV=%08x once=1 scope=same-target\n",
                           (unsigned int)targetMapId,
                           (unsigned int)plan.indexBytes,
                           (unsigned int)plan.stagedBytes,
                           (unsigned int)flashPayloadFNV);
                }
                else {
                    printf("[MAPFLASHV2] HIT map=%u eraseBytes=0 writeBytes=0 verifiedBytes=%u scope=all-maps\n",
                           (unsigned int)targetMapId,
                           (unsigned int)(plan.indexBytes + plan.stagedBytes));
                }
                return 1;
            }
            free(buffer);
            buffer = nullptr;
        }
    }

    printf("[MAPFLASH] REUSE MISS requestedMap=%u current=%s cachedValid=%u cachedMap=%u reason=%s rebuild=yes\n",
           (unsigned int)targetMapId,
           EspMapCatalog_nameForId(targetMapId),
           (unsigned int)cachedValid,
           (unsigned int)cachedMapId,
           missReason != nullptr ? missReason : "unknown");
    clearMapFlashRuntime(true);
    return EspAssetPack_mapFlashStage(targetMapId);
}

int EspAssetPack_mapFlashStage(uint8_t currentMapId)
{
    uint8_t* buffer = nullptr;
    MapFlashPlan plan = {};
    const char* planReason = nullptr;
    EspAssetPackEntry currentEntry = {};
    const esp_partition_t* partition = nullptr;
    const char* currentMapName = nullptr;
    uint32_t indexBytes = 0U;
    uint32_t payloadFlashOffset = 0U;
    uint32_t stagedBytes = 0U;
    uint32_t sourceDataBytes = 0U;
    uint32_t sourcePackBytes = 0U;
    uint32_t sourceIndexOffset = 0U;
    uint32_t sourceDataOffset = 0U;
    uint32_t sourceEntryCount = 0U;
    uint32_t currentMapHash = 0U;
    uint32_t indexFNV = 2166136261U;
    uint32_t payloadFNV = 2166136261U;
    uint32_t verifyIndexFNV = 0U;
    uint32_t verifyPayloadFNV = 0U;
    uint32_t copyProgress = 0U;
    uint32_t copyProgressTotal = 0U;
    uint32_t verifyProgress = 0U;
    uint32_t verifyProgressTotal = 0U;
    const int64_t buildStart = esp_timer_get_time();

    if (openReady || residentEnabled || sourceProbeReady ||
        !EspMapCatalog_isValidId(currentMapId)) {
        mapFlash.handoffEligible = 0U;
        printf("[MAPFLASH] FAILED reason=bad-stage-boundary map=%u open=%u resident=%u\n",
               (unsigned int)currentMapId,
               (unsigned int)openReady,
               (unsigned int)residentEnabled);
        return 0;
    }

    resetPhysicalState();
    clearMapFlashRuntime(true);

    partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                         ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                                         "spiffs");
    if (partition == nullptr || partition->size <= kMapFlashIndexOffset) {
        printf("[MAPFLASH] FAILED reason=raw-partition-missing map=%u\n",
               (unsigned int)currentMapId);
        return 0;
    }
    mapFlash.partition = partition;

    if (!mapFlashBuildSourcePlan(currentMapId, partition, &plan, &planReason)) {
        printf("[MAPFLASH] FAILED reason=%s map=%u\n",
               planReason != nullptr ? planReason : "source-plan",
               (unsigned int)currentMapId);
        clearMapFlashRuntime(true);
        return 0;
    }
    currentMapName = EspMapCatalog_nameForId(currentMapId);
    if (currentMapName == nullptr || !EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        printf("[MAPFLASH] FAILED reason=source-pack-open map=%u\n",
               (unsigned int)currentMapId);
        clearMapFlashRuntime(true);
        return 0;
    }

    auto failStage = [&](const char* reason) -> int {
        if (EspAssetPack_isOpen()) EspAssetPack_close();
        if (buffer != nullptr) free(buffer);
        printf("[MAPFLASH] FAILED reason=%s map=%u\n",
               reason != nullptr ? reason : "unknown",
               (unsigned int)currentMapId);
        clearMapFlashRuntime(true);
        return 0;
    };

    sourcePackBytes = packSize;
    sourceIndexOffset = indexOffset;
    sourceDataOffset = dataOffset;
    sourceEntryCount = entryCount;
    sourceDataBytes = sourcePackBytes - sourceDataOffset;
    if (sourceEntryCount > UINT16_MAX) {
        return failStage("entry-count-overflow");
    }

    currentMapHash = EspAssetPack_nameHash(currentMapName);
    if (currentMapHash == 0U ||
        !EspAssetPack_findEntry(currentMapName, &currentEntry) ||
        (currentEntry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
        currentEntry.size == 0U) {
        return failStage("current-bsp-missing");
    }

    if (sourcePackBytes != plan.sourcePackBytes ||
        sourceIndexOffset != plan.sourceIndexOffset ||
        sourceDataOffset != plan.sourceDataOffset ||
        sourceEntryCount != plan.entryCount) {
        return failStage("source-plan-changed");
    }

    indexBytes = sourceEntryCount * (uint32_t)kEntryBytes;
    payloadFlashOffset = alignSector(kMapFlashIndexOffset + indexBytes);
    stagedBytes = sourceDataBytes;
    if (payloadFlashOffset == 0U || payloadFlashOffset > partition->size ||
        stagedBytes > partition->size - payloadFlashOffset) {
        printf("[MAPFLASH] PLAN map=%u current=%s pack=%u entries=%u index=%u metadata=%u data=%u excludeMaps=%u excludeBytes=%u stage=%u partition=%u fits=no\n",
               (unsigned int)currentMapId,
               currentMapName,
               (unsigned int)sourcePackBytes,
               (unsigned int)sourceEntryCount,
               (unsigned int)indexBytes,
               (unsigned int)payloadFlashOffset,
               (unsigned int)sourceDataBytes,
               0U,
               0U,
               (unsigned int)stagedBytes,
               (unsigned int)partition->size);
        return failStage("complete-pak-does-not-fit");
    }

    printf("[MAPFLASH] PLAN map=%u current=%s pack=%u entries=%u index=%u metadata=%u data=%u excludeMaps=%u excludeBytes=%u stage=%u partition=%u headroom=%u fits=yes\n",
           (unsigned int)currentMapId,
           currentMapName,
           (unsigned int)sourcePackBytes,
           (unsigned int)sourceEntryCount,
           (unsigned int)indexBytes,
           (unsigned int)payloadFlashOffset,
           (unsigned int)sourceDataBytes,
           0U,
           0U,
           (unsigned int)stagedBytes,
           (unsigned int)partition->size,
           (unsigned int)(partition->size - payloadFlashOffset - stagedBytes));
    printf("[MAPFLASH] CONTRACT raw-partition V2 full PAK index+payload; all BSPs present when supplied; direct source offsets; 15KiB RAM cache stays L1; commit marker last after flash readback; SD fallback during active gameplay=no\n");

    buffer = (uint8_t*)malloc(kMapFlashCopyBufferBytes);
    if (buffer == nullptr) {
        return failStage("copy-buffer-allocation");
    }

    if ((partition->size % kMapFlashSectorBytes) != 0U) {
        return failStage("partition-erase-alignment");
    }
    const uint32_t eraseTotal = alignSector(payloadFlashOffset + stagedBytes);
    if (eraseTotal == 0U || eraseTotal > partition->size) {
        return failStage("erase-bounds");
    }
    // Reject damaged source resources before destroying an existing cache.
    auto readSource = [&](uint32_t offset, void* dst, size_t length) -> bool {
        return packFile.seek(offset) &&
               packFile.read(static_cast<uint8_t*>(dst), length) == length;
    };
    if (!mapFlashValidateEntryCrcs(plan, buffer, readSource, "sd-before-erase"))
        return failStage("source-entry-crc");
    mapFlashCrcProof = {};
    /* Invalidate the old commit before touching any index/payload sectors. */
    if (esp_partition_erase_range(partition, 0U, kMapFlashSectorBytes) != ESP_OK) {
        return failStage("header-invalidate");
    }
    notifyMapFlashProgress(ESP_ASSET_PACK_MAP_FLASH_PROGRESS_ERASE,
                           kMapFlashSectorBytes, eraseTotal);
    for (uint32_t eraseOffset = kMapFlashSectorBytes; eraseOffset < eraseTotal;) {
        uint32_t eraseBytes = eraseTotal - eraseOffset;
        if (eraseBytes > kMapFlashEraseChunkBytes) eraseBytes = kMapFlashEraseChunkBytes;
        if (esp_partition_erase_range(partition, eraseOffset, eraseBytes) != ESP_OK) {
            return failStage("partition-erase");
        }
        eraseOffset += eraseBytes;
        notifyMapFlashProgress(ESP_ASSET_PACK_MAP_FLASH_PROGRESS_ERASE,
                               eraseOffset, eraseTotal);
    }
    printf("[MAPFLASH] ERASE bytes=%u chunk=%u buffer=%u owner=transient progress=chunked\n",
           (unsigned int)eraseTotal, (unsigned int)kMapFlashEraseChunkBytes,
           (unsigned int)kMapFlashCopyBufferBytes);

    copyProgressTotal = indexBytes + stagedBytes;
    if (!copySdRangeToFlash(sourceIndexOffset,
                            indexBytes,
                            kMapFlashIndexOffset,
                            buffer,
                            &indexFNV,
                            &copyProgress,
                            copyProgressTotal)) {
        return failStage("index-copy");
    }

    if (indexFNV != plan.indexFNV1a) {
        return failStage("source-index-changed");
    }
    if (!copySdRangeToFlash(sourceDataOffset, stagedBytes, payloadFlashOffset,
                            buffer, &payloadFNV, &copyProgress, copyProgressTotal)) {
        return failStage("payload-copy");
    }

    verifyProgressTotal = indexBytes + stagedBytes;
    if (!fnvFlashRangeProgress(kMapFlashIndexOffset,
                               indexBytes,
                               buffer,
                               &verifyIndexFNV,
                               &verifyProgress,
                               verifyProgressTotal) ||
        !fnvFlashRangeProgress(payloadFlashOffset,
                               stagedBytes,
                               buffer,
                               &verifyPayloadFNV,
                               &verifyProgress,
                               verifyProgressTotal) ||
        verifyIndexFNV != indexFNV || verifyPayloadFNV != payloadFNV) {
        return failStage("flash-readback-fnv");
    }

    // Validate the actual bytes written, not merely the source preflight.
    if (!mapFlashEnsureEntryCrcs(plan, verifyPayloadFNV, buffer))
        return failStage("flash-entry-crc");
    MapFlashHeader committed = {};
    committed.magic = kMapFlashMagic;
    committed.version = kMapFlashVersion;
    committed.headerBytes = (uint16_t)sizeof(MapFlashHeader);
    committed.committed = UINT32_MAX; /* erased until the final marker write */
    committed.sourcePackBytes = sourcePackBytes;
    committed.sourceIndexOffset = sourceIndexOffset;
    committed.sourceDataOffset = sourceDataOffset;
    committed.entryCount = sourceEntryCount;
    committed.indexFlashOffset = kMapFlashIndexOffset;
    committed.indexBytes = indexBytes;
    committed.payloadFlashOffset = payloadFlashOffset;
    committed.stagedBytes = stagedBytes;
    committed.indexFNV1a = indexFNV;
    committed.payloadFNV1a = payloadFNV;
    committed.currentMapHash = currentMapHash;
    committed.currentMapId = currentMapId;
    if (esp_partition_write(partition, 0U, &committed, sizeof(committed)) != ESP_OK) {
        return failStage("header-commit");
    }
    committed.committed = kMapFlashCommitted;
    if (esp_partition_write(partition, offsetof(MapFlashHeader, committed),
                            &committed.committed, sizeof(committed.committed)) != ESP_OK) {
        return failStage("commit-marker");
    }
    MapFlashHeader readback = {};
    if (esp_partition_read(partition, 0U, &readback, sizeof(readback)) != ESP_OK ||
        memcmp(&readback, &committed, sizeof(committed)) != 0 ||
        !mapFlashHeaderLooksValid(readback, partition)) {
        return failStage("header-readback");
    }

    EspAssetPack_close();
    free(buffer);
    buffer = nullptr;

    mapFlash.partition = partition;
    mapFlash.header = readback;
    mapFlash.active = 1U;
    mapFlash.missLogs = 0U;
    mapFlash.handoffEligible = 1U;
    memset(&mapFlash.stats, 0, sizeof(mapFlash.stats));
    mapFlash.stats.partitionBytes = partition->size;
    mapFlash.stats.sourcePackBytes = sourcePackBytes;
    mapFlash.stats.sourceDataBytes = sourceDataBytes;
    mapFlash.stats.indexBytes = indexBytes;
    mapFlash.stats.metadataBytes = payloadFlashOffset;
    mapFlash.stats.stagedBytes = stagedBytes;
    mapFlash.stats.buildMicros =
        (uint32_t)(esp_timer_get_time() - buildStart);
    mapFlash.stats.entryCount = (uint16_t)sourceEntryCount;
    mapFlash.stats.currentMapId = currentMapId;
    mapFlash.stats.active = 1U;
    mapFlash.stats.verified = 1U;

    printf("[MAPFLASH] COPY indexFNV=%08x payloadFNV=%08x verified=yes\n",
           (unsigned int)indexFNV,
           (unsigned int)payloadFNV);
    printf("[MAPFLASH] READY map=%u staged=%u metadata=%u excluded=%u/%u buildUs=%u backing=raw-internal-flash SDGameplayReads=forbidden\n",
           (unsigned int)currentMapId,
           (unsigned int)stagedBytes,
           (unsigned int)payloadFlashOffset,
           0U,
           0U,
           (unsigned int)mapFlash.stats.buildMicros);
    printf("[MAPFLASHV2] BUILD map=%u eraseBytes=%u writeBytes=%u verifiedBytes=%u headroom=%u scope=all-maps\n",
           (unsigned int)currentMapId, (unsigned int)eraseTotal,
           (unsigned int)(indexBytes + stagedBytes + sizeof(MapFlashHeader) + sizeof(committed.committed)),
           (unsigned int)(indexBytes + stagedBytes),
           (unsigned int)(partition->size - eraseTotal));
    return 1;
}

void EspAssetPack_mapFlashSetProgressCallback(
    EspAssetPackMapFlashProgressCallback callback)
{
    mapFlashProgressCallback = callback;
}

void EspAssetPack_mapFlashDeactivate(void)
{
    if (openReady || residentEnabled) {
        return;
    }
    clearMapFlashRuntime(true);
}

int EspAssetPack_isMapFlashActive(void)
{
    return mapFlash.active && mapFlash.partition != nullptr ? 1 : 0;
}

void EspAssetPack_mapFlashGetStats(EspAssetPackMapFlashStats* outStats)
{
    if (outStats == nullptr) return;
    *outStats = mapFlash.stats;
    outStats->active = EspAssetPack_isMapFlashActive() ? 1U : 0U;
}

uint8_t EspAssetPack_mapFlashCachedMapId(void)
{
    const esp_partition_t* partition = nullptr;
    MapFlashHeader cached = {};
    const char* name = nullptr;
    /* An active lease is already structurally verified. Otherwise inspect
     * only the persisted committed header without activating the slot or
     * reading the payload. Staging still requires full verification. */
    if (EspAssetPack_isMapFlashActive()) {
        cached = mapFlash.header;
        partition = mapFlash.partition;
    }
    else {
        partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
            "spiffs");
        if (partition == nullptr ||
            esp_partition_read(partition, 0U, &cached, sizeof(cached)) !=
                ESP_OK) {
            return 0U;
        }
    }
    if (!mapFlashHeaderLooksValid(cached, partition) ||
        !EspMapCatalog_isValidId(cached.currentMapId)) {
        return 0U;
    }
    name = EspMapCatalog_nameForId(cached.currentMapId);
    if (name == nullptr || cached.currentMapHash !=
                           EspAssetPack_nameHash(name)) {
        return 0U;
    }
    return cached.currentMapId;
}

/* ARCH-RC18: raw resident-cache leaf; the public begin transaction lives in
 * esp_asset_pack_perf_probe.cpp and stages verified map flash first. */
extern "C" int EspAssetPack_residentBeginBase(void)
{
    if (openReady) {
        return 0;
    }
    if (residentEnabled && physicalReady && physicalIsDefault &&
        residentCache != nullptr) {
        return 1;
    }

    if (residentCache == nullptr) {
        residentCache = (ResidentCache*)calloc(1U, sizeof(*residentCache));
        if (residentCache == nullptr) {
            return 0;
        }
    }

    memset(&residentStats, 0, sizeof(residentStats));
    residentLargeRangeEnabled = false;
    residentEnabled = true;
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        residentEnabled = false;
        resetPhysicalState();
        free(residentCache);
        residentCache = nullptr;
        return 0;
    }
    EspAssetPack_close();
    return physicalReady && !openReady ? 1 : 0;
}

int EspAssetPack_residentEnd(void)
{
    /* ARCH-RC3 preserves the old --wrap preflight even when openReady rejects
     * residentEnd: report pending TRACE I/O and invalidate the baseline before
     * any owner-side close, cache release or flash deactivation. */
    EspAssetPackIoProbe_beforeResidentEnd();
    if (openReady) {
        return 0;
    }
    residentLargeRangeEnabled = false;
    residentEnabled = false;
    resetPhysicalState();
    if (residentCache != nullptr) {
        free(residentCache);
        residentCache = nullptr;
    }
    clearMapFlashRuntime(true);
    return 1;
}

int EspAssetPack_isResident(void)
{
    return residentEnabled && physicalReady && physicalIsDefault &&
                   residentCache != nullptr
               ? 1
               : 0;
}

void EspAssetPack_residentResetStats(void)
{
    /* Preserve the former --wrap order, now under explicit native ownership:
     * flush the TRACE I/O sample before clearing counters, then invalidate its
     * physical baseline only after the reset is fully committed. */
    EspAssetPackIoProbe_beforeResidentStatsReset();
    memset(&residentStats, 0, sizeof(residentStats));
    mapFlashRuntimeReadMicros = 0U;
#if defined(DOOMRPG_RA2B_CAPTURE) && DOOMRPG_RA2B_CAPTURE
    ra2bSdRangeMicros = 0U;
#endif
    EspAssetPackIoProbe_afterResidentStatsReset();
}

uint32_t EspAssetPack_verifiedFlashLeaseEpoch(void)
{
    return verifiedFlashEpoch != UINT32_MAX && openReady &&
           physicalUsesMapFlash && mapFlash.active && mapFlash.partition != nullptr &&
           mapFlash.stats.verified && !EspAssetPack_isSourceProbeActive()
               ? verifiedFlashEpoch : 0U;
}

uint32_t EspAssetPack_mapFlashRuntimeReadMicros(void)
{
    return mapFlashRuntimeReadMicros;
}

#if defined(DOOMRPG_RA2B_CAPTURE) && DOOMRPG_RA2B_CAPTURE
uint32_t EspAssetPack_ra2bSdRangeMicros(void)
{
    return ra2bSdRangeMicros;
}
#endif

void EspAssetPack_residentGetStats(EspAssetPackResidentStats* outStats)
{
    if (outStats == nullptr) {
        return;
    }
    *outStats = residentStats;
    outStats->rangeCacheCapacityBytes = kResidentRangeCapacityBytes;
    outStats->rangeCacheEntryCapacity = kResidentRangeEntryCapacity;
    outStats->ownerBytes = residentCache != nullptr
                               ? (uint32_t)sizeof(*residentCache)
                               : 0U;
    outStats->enabled = residentEnabled ? 1U : 0U;
    outStats->ready = EspAssetPack_isResident() ? 1U : 0U;
    outStats->largeRangeEnabled = residentLargeRangeEnabled ? 1U : 0U;
    if (residentCache != nullptr) {
        const uint32_t largeCount = residentLargeRangeCount();
        outStats->rangeCacheBytesUsed = residentPayloadBytesUsed();
        outStats->rangeCacheEntries = residentCache->rangeCount;
        outStats->largeRangeEntries =
            largeCount <= UINT8_MAX ? (uint8_t)largeCount : UINT8_MAX;
    }
}

int EspAssetPack_residentLargeRangeBegin(void)
{
    if (openReady || !EspAssetPack_isResident()) {
        return 0;
    }
    residentLargeRangeEnabled = true;
    return 1;
}

int EspAssetPack_residentLargeRangeEnd(void)
{
    if (openReady || !EspAssetPack_isResident()) {
        return 0;
    }
    clearLargeResidentRanges();
    residentLargeRangeEnabled = false;
    return 1;
}

int EspAssetPack_isResidentLargeRangeEnabled(void)
{
    return EspAssetPack_isResident() && residentLargeRangeEnabled ? 1 : 0;
}

int EspAssetPack_getEntryByIndex(int index, EspAssetPackEntry* outEntry)
{
    if (!openReady || index < 0) {
        return 0;
    }
    return readEntryAt((uint32_t)index, outEntry) ? 1 : 0;
}

uint32_t EspAssetPack_nameHash(const char* name)
{
    uint32_t hash = 2166136261U;
    bool sawByte = false;

    if (name == nullptr) {
        return 0U;
    }

    while (*name == '/' || *name == '\\') {
        ++name;
    }

    while (*name != '\0') {
        const uint8_t value = normalizeNameByte((uint8_t)*name++);
        hash ^= value;
        hash *= 16777619U;
        sawByte = true;
    }

    return sawByte ? hash : 0U;
}

int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* outEntry)
{
    uint32_t low = 0U;
    uint32_t high = entryCount;
    const uint32_t wantedHash = EspAssetPack_nameHash(name);

    if (!openReady || outEntry == nullptr || wantedHash == 0U) {
        return 0;
    }

    if (residentEnabled && residentCache != nullptr) {
        ResidentEntryRecord* slot =
            &residentCache->entries[wantedHash % kResidentEntryCacheSlots];
        if (slot->valid && slot->nameHash == wantedHash) {
            *outEntry = slot->entry;
            ++residentStats.entryCacheHits;
            return 1;
        }
        ++residentStats.entryCacheMisses;
    }

    while (low < high) {
        const uint32_t mid = low + ((high - low) / 2U);
        EspAssetPackEntry entry;

        if (!readEntryAt(mid, &entry)) {
            return 0;
        }

        if (entry.nameHash < wantedHash) {
            low = mid + 1U;
        }
        else if (entry.nameHash > wantedHash) {
            high = mid;
        }
        else {
            *outEntry = entry;
            if (residentEnabled && residentCache != nullptr) {
                ResidentEntryRecord* slot =
                    &residentCache->entries[wantedHash % kResidentEntryCacheSlots];
                slot->nameHash = wantedHash;
                slot->entry = entry;
                slot->valid = 1U;
            }
            return 1;
        }
    }

    return 0;
}

static int readRangeCore(const EspAssetPackEntry* entry,
                           uint32_t relativeOffset,
                           void* destination,
                           size_t length)
{
    bool cacheableSmall;
    bool cacheableLarge;

    if (!openReady || entry == nullptr || destination == nullptr) {
        return 0;
    }

    if (!validateEntry(*entry) || relativeOffset > entry->size ||
        length > (size_t)(entry->size - relativeOffset)) {
        return 0;
    }

    if (length == 0U) {
        return 1;
    }

    const bool sourceProbeHash =
        sourceProbeReady && entry->nameHash == sourceProbeEntry.nameHash;
    const bool sourceProbeExact =
        sourceProbeHash && sourceProbeEntryExact(*entry);

    if (sourceProbeHash && !sourceProbeExact) {
        printf("[PAKSOURCE] PROBE-MISMATCH hash=%08x activeOffset=%u sourceOffset=%u activeSize=%u sourceSize=%u activeCrc=%08x sourceCrc=%08x failClosed=yes\n",
               (unsigned int)entry->nameHash,
               (unsigned int)entry->offset,
               (unsigned int)sourceProbeEntry.offset,
               (unsigned int)entry->size,
               (unsigned int)sourceProbeEntry.size,
               (unsigned int)entry->crc32,
               (unsigned int)sourceProbeEntry.crc32);
        return 0;
    }

    cacheableSmall = !sourceProbeExact &&
                     residentEnabled && residentCache != nullptr &&
                     length <= kResidentMaxCachedRangeBytes;
    cacheableLarge = !sourceProbeExact &&
                     residentEnabled && residentLargeRangeEnabled &&
                     residentCache != nullptr &&
                     length == kResidentLargeRangeBytes;

    /* validateEntry and the relative bounds above prove no uint32 overflow. */
    const uint32_t absoluteOffset = entry->offset + relativeOffset;
    if (cacheableSmall || cacheableLarge) {
        ResidentRangeRecord* record =
            findResidentRange(absoluteOffset, (uint32_t)length);
        if (record != nullptr) {
            memcpy(destination,
                   residentCache->bytes + record->dataOffset,
                   length);
            ++residentStats.rangeCacheHits;
            return 1;
        }
        ++residentStats.rangeCacheMisses;
    }
    else if (residentEnabled) {
        ++residentStats.rangeCacheBypasses;
    }

    if (sourceProbeExact) {
        if (!readSourceProbePayload(absoluteOffset, destination, length)) return 0;
        if (sourceProbeReadLogged == 0U) {
            sourceProbeReadLogged = 1U;
            printf("[PAKSOURCE] PROBE-READ hash=%08x source=%u length=%u backing=%s activeMapFlashUntouched=yes cachePollution=no\n",
                   (unsigned int)entry->nameHash,
                   (unsigned int)absoluteOffset,
                   (unsigned int)length,
                   sourceProbeFlashEpoch != 0U ? "verified-flash" : "sd-authoritative");
        }
    }
    else if (physicalUsesMapFlash) {
        uint32_t flashOffset = 0U;
        if (!mapFlashTranslate(absoluteOffset, (uint32_t)length, &flashOffset) ||
            !mapFlashRead(flashOffset, destination, length, true)) {
            logMapFlashStrictMiss(entry, relativeOffset, length);
            return 0;
        }
    }
    else {
#if defined(DOOMRPG_RA2B_CAPTURE) && DOOMRPG_RA2B_CAPTURE
        /* RA2b only: profile actual SD seek + read, not flash or cache hits.
         * The logical lease is already open. Never start I/O for the probe. */
        const int64_t ra2bStart =
            residentEnabled ? esp_timer_get_time() : 0LL;
        const bool ra2bReadOk =
            packFile.seek(absoluteOffset) && readSdExact(destination, length);
        if (ra2bStart != 0LL) {
            const int64_t elapsed = esp_timer_get_time() - ra2bStart;
            if (elapsed > 0LL) {
                const uint32_t delta = (uint64_t)elapsed > UINT32_MAX
                                           ? UINT32_MAX : (uint32_t)elapsed;
                ra2bSdRangeMicros += delta;
            }
        }
        if (!ra2bReadOk) return 0;
#else
        if (!packFile.seek(absoluteOffset) || !readSdExact(destination, length)) {
            return 0;
        }
#endif
    }

    if (cacheableSmall) {
        storeResidentSmallRange(absoluteOffset,
                                destination,
                                (uint32_t)length);
    }
    else if (cacheableLarge) {
        storeResidentLargeRange(absoluteOffset,
                                destination,
                                (uint32_t)length);
    }
    return 1;
}

/* ARCH-RC4: observe the public logical read exactly once, including every
 * short-circuit failure and zero-length success. The unchanged core retains
 * exclusive PAK/flash/cache ownership. INFO builds have no timer calls. */
int EspAssetPack_readRange(const EspAssetPackEntry* entry,
                           uint32_t relativeOffset,
                           void* destination,
                           size_t length)
{
#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE
    const int64_t ioStart = EspAssetPackIoProbe_beforeReadRange();
#endif
    const int result = readRangeCore(entry, relativeOffset, destination, length);
#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE
    EspAssetPackIoProbe_afterReadRange(ioStart);
#endif
    return result;
}
