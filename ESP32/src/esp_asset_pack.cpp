#include <Arduino.h>
#include <SD.h>
#include <stdlib.h>
#include <string.h>

#include <esp_partition.h>
#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_map_catalog.h"

namespace {

constexpr uint8_t kMagic[8] = {'D', 'R', 'P', 'G', 'E', 'S', 'P', '2'};
constexpr uint32_t kVersion = ESP_ASSET_PACK_FORMAT_VERSION;
constexpr size_t kHeaderBytes = 24;
constexpr size_t kEntryBytes = 20;
constexpr uint32_t kResidentRangeCapacityBytes = 19U * 1024U;
constexpr uint32_t kResidentRangeEntryCapacity = 288U;
constexpr uint32_t kResidentEntryCacheSlots = 24U;
constexpr uint32_t kResidentMaxCachedRangeBytes = 1024U;
constexpr uint32_t kResidentLargeRangeBytes = 2048U;

constexpr uint32_t kMapFlashMagic = 0x32464d44U; /* DMF2 */
constexpr uint16_t kMapFlashVersion = 1U;
constexpr uint32_t kMapFlashCommitted = 0xa55a3cc3U;
constexpr uint32_t kMapFlashSectorBytes = 4096U;
constexpr uint32_t kMapFlashIndexOffset = kMapFlashSectorBytes;
constexpr uint32_t kMapFlashCopyBufferBytes = 4096U;
constexpr uint32_t kMapFlashMaxExcludedMaps = ESP_MAP_CATALOG_COUNT - 1U;
constexpr uint8_t kMapFlashMaxMissLogs = 8U;

struct ResidentRangeRecord {
    uint32_t nameHash;
    uint32_t relativeOffset;
    uint16_t length;
    uint16_t dataOffset;
};

static_assert(kResidentRangeCapacityBytes <= UINT16_MAX,
              "resident payload offsets must fit uint16_t");
static_assert(kResidentLargeRangeBytes <= UINT16_MAX,
              "resident range lengths must fit uint16_t");
static_assert(sizeof(ResidentRangeRecord) == 12U,
              "resident range metadata must stay compact");

struct ResidentEntryRecord {
    uint32_t nameHash;
    EspAssetPackEntry entry;
    uint8_t valid;
};

struct ResidentCache {
    ResidentRangeRecord ranges[kResidentRangeEntryCapacity];
    ResidentEntryRecord entries[kResidentEntryCacheSlots];
    uint8_t bytes[kResidentRangeCapacityBytes];
    uint32_t rangeCount;
    uint32_t bytesUsed;
};

struct MapFlashExcludedSpan {
    uint32_t nameHash;
    uint32_t sourceOffset;
    uint32_t size;
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
    uint32_t excludedBytes;
    uint32_t indexFNV1a;
    uint32_t payloadFNV1a;
    uint32_t currentMapHash;
    uint8_t currentMapId;
    uint8_t excludedCount;
    uint16_t reserved;
    MapFlashExcludedSpan excluded[kMapFlashMaxExcludedMaps];
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
    uint32_t excludedBytes;
    uint32_t indexFNV1a;
    uint32_t currentMapHash;
    uint32_t excludedCount;
    MapFlashExcludedSpan excluded[kMapFlashMaxExcludedMaps];
};

struct MapFlashState {
    const esp_partition_t* partition;
    MapFlashHeader header;
    EspAssetPackMapFlashStats stats;
    uint8_t active;
    uint8_t missLogs;
};

File packFile;
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
MapFlashState mapFlash = {};

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

void clearMapFlashRuntime(bool clearStats)
{
    mapFlash.partition = nullptr;
    memset(&mapFlash.header, 0, sizeof(mapFlash.header));
    mapFlash.active = 0U;
    mapFlash.missLogs = 0U;
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
    if (esp_partition_read(mapFlash.partition, offset, destination, length) != ESP_OK) {
        return false;
    }
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
        header.excludedCount > kMapFlashMaxExcludedMaps ||
        header.sourceDataOffset > header.sourcePackBytes ||
        header.indexBytes != header.entryCount * (uint32_t)kEntryBytes ||
        header.payloadFlashOffset < header.indexFlashOffset + header.indexBytes ||
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
                entry.size == 0U ||
                plan.excludedCount >= kMapFlashMaxExcludedMaps) {
                sourceFile.close();
                if (outReason != nullptr) *outReason = "excluded-bsp-invalid";
                return false;
            }
            MapFlashExcludedSpan& span = plan.excluded[plan.excludedCount++];
            span.nameHash = entry.nameHash;
            span.sourceOffset = entry.offset;
            span.size = entry.size;
            break;
        }
    }
    sourceFile.close();

    if (!currentFound) {
        if (outReason != nullptr) *outReason = "target-bsp-missing";
        return false;
    }

    for (uint32_t i = 0U; i < plan.excludedCount; ++i) {
        for (uint32_t j = i + 1U; j < plan.excludedCount; ++j) {
            if (plan.excluded[j].sourceOffset < plan.excluded[i].sourceOffset) {
                const MapFlashExcludedSpan temp = plan.excluded[i];
                plan.excluded[i] = plan.excluded[j];
                plan.excluded[j] = temp;
            }
        }
    }

    plan.sourceDataBytes = plan.sourcePackBytes - plan.sourceDataOffset;
    uint32_t previousEnd = plan.sourceDataOffset;
    for (uint32_t i = 0U; i < plan.excludedCount; ++i) {
        const MapFlashExcludedSpan& span = plan.excluded[i];
        if (span.sourceOffset < plan.sourceDataOffset ||
            span.sourceOffset < previousEnd ||
            span.sourceOffset > plan.sourcePackBytes ||
            span.size > plan.sourcePackBytes - span.sourceOffset ||
            plan.excludedBytes > UINT32_MAX - span.size) {
            if (outReason != nullptr) *outReason = "excluded-span-invalid";
            return false;
        }
        previousEnd = span.sourceOffset + span.size;
        plan.excludedBytes += span.size;
    }
    if (plan.excludedBytes > plan.sourceDataBytes) {
        if (outReason != nullptr) *outReason = "excluded-bytes-invalid";
        return false;
    }

    plan.payloadFlashOffset =
        alignSector(kMapFlashIndexOffset + plan.indexBytes);
    plan.stagedBytes = plan.sourceDataBytes - plan.excludedBytes;
    if (plan.payloadFlashOffset == 0U ||
        plan.payloadFlashOffset > partition->size ||
        plan.stagedBytes > partition->size - plan.payloadFlashOffset) {
        if (outReason != nullptr) *outReason = "working-set-does-not-fit";
        return false;
    }

    *outPlan = plan;
    if (outReason != nullptr) *outReason = "ok";
    return true;
}

bool mapFlashHeaderMatchesPlan(const MapFlashHeader& header,
                               uint8_t targetMapId,
                               const MapFlashPlan& plan,
                               const char** outReason)
{
    if (outReason != nullptr) *outReason = "header-invalid";
    if (header.currentMapId != targetMapId ||
        header.currentMapHash != plan.currentMapHash) {
        if (outReason != nullptr) *outReason = "world-identity";
        return false;
    }
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
        header.stagedBytes != plan.stagedBytes ||
        header.excludedBytes != plan.excludedBytes ||
        header.excludedCount != plan.excludedCount ||
        memcmp(header.excluded,
               plan.excluded,
               plan.excludedCount * sizeof(plan.excluded[0])) != 0) {
        if (outReason != nullptr) *outReason = "world-layout";
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
    uint32_t count = 0U;
    if (residentCache == nullptr) {
        return 0U;
    }
    for (uint32_t i = 0U; i < residentCache->rangeCount; ++i) {
        if (isLargeResidentRange(residentCache->ranges[i])) {
            ++count;
        }
    }
    return count;
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
    uint32_t lowest = kResidentRangeCapacityBytes;
    if (residentCache == nullptr) {
        return lowest;
    }
    for (uint32_t i = 0U; i < residentCache->rangeCount; ++i) {
        const ResidentRangeRecord& record = residentCache->ranges[i];
        if (isLargeResidentRange(record) && record.dataOffset < lowest) {
            lowest = record.dataOffset;
        }
    }
    return lowest;
}

bool evictLowestLargeRange()
{
    uint32_t victim = UINT32_MAX;
    uint32_t lowest = UINT32_MAX;
    if (residentCache == nullptr) {
        return false;
    }
    for (uint32_t i = 0U; i < residentCache->rangeCount; ++i) {
        const ResidentRangeRecord& record = residentCache->ranges[i];
        if (isLargeResidentRange(record) && record.dataOffset < lowest) {
            lowest = record.dataOffset;
            victim = i;
        }
    }
    if (victim == UINT32_MAX) {
        return false;
    }
    eraseResidentRange(victim);
    return true;
}

void clearLargeResidentRanges()
{
    if (residentCache == nullptr) {
        return;
    }
    uint32_t i = 0U;
    while (i < residentCache->rangeCount) {
        if (isLargeResidentRange(residentCache->ranges[i])) {
            eraseResidentRange(i);
        }
        else {
            ++i;
        }
    }
}

void recycleResidentRangeWorkingSet()
{
    if (residentCache == nullptr) {
        return;
    }
    residentCache->rangeCount = 0U;
    residentCache->bytesUsed = 0U;
}

ResidentRangeRecord* findResidentRange(uint32_t nameHash,
                                       uint32_t relativeOffset,
                                       uint32_t length)
{
    if (!residentEnabled || residentCache == nullptr) {
        return nullptr;
    }
    for (uint32_t i = 0U; i < residentCache->rangeCount; ++i) {
        ResidentRangeRecord* record = &residentCache->ranges[i];
        if (record->nameHash == nameHash &&
            record->relativeOffset == relativeOffset &&
            record->length == length) {
            return record;
        }
    }
    return nullptr;
}

void storeResidentSmallRange(uint32_t nameHash,
                             uint32_t relativeOffset,
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

    ResidentRangeRecord* record =
        &residentCache->ranges[residentCache->rangeCount++];
    record->nameHash = nameHash;
    record->relativeOffset = relativeOffset;
    record->length = (uint16_t)length;
    record->dataOffset = (uint16_t)residentCache->bytesUsed;
    memcpy(residentCache->bytes + record->dataOffset, source, length);
    residentCache->bytesUsed += length;
    ++residentStats.rangeCacheStores;
}

void storeResidentLargeRange(uint32_t nameHash,
                             uint32_t relativeOffset,
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
    record->nameHash = nameHash;
    record->relativeOffset = relativeOffset;
    record->length = (uint16_t)length;
    record->dataOffset = (uint16_t)targetOffset;
    memcpy(residentCache->bytes + targetOffset, source, length);
    ++residentStats.rangeCacheStores;
}

bool copySdRangeToFlash(uint32_t sourceOffset,
                        uint32_t length,
                        uint32_t flashOffset,
                        uint8_t* buffer,
                        uint32_t* ioFNV)
{
    if (!packFile || mapFlash.partition == nullptr || buffer == nullptr ||
        flashOffset > mapFlash.partition->size ||
        length > mapFlash.partition->size - flashOffset ||
        !packFile.seek(sourceOffset)) {
        return false;
    }

    uint32_t remaining = length;
    uint32_t destinationOffset = flashOffset;
    while (remaining > 0U) {
        const uint32_t chunk =
            remaining > kMapFlashCopyBufferBytes
                ? kMapFlashCopyBufferBytes
                : remaining;
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
        destinationOffset += chunk;
        remaining -= chunk;
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

    const uint64_t sourceEnd = (uint64_t)sourceOffset + (uint64_t)length;
    uint32_t removed = 0U;
    for (uint32_t i = 0U; i < mapFlash.header.excludedCount; ++i) {
        const MapFlashExcludedSpan& span = mapFlash.header.excluded[i];
        const uint64_t spanEnd =
            (uint64_t)span.sourceOffset + (uint64_t)span.size;
        if (sourceEnd <= span.sourceOffset) {
            break;
        }
        if (sourceOffset >= spanEnd) {
            removed += span.size;
            continue;
        }
        return false;
    }

    const uint32_t compactOffset =
        (sourceOffset - mapFlash.header.sourceDataOffset) - removed;
    if (compactOffset > mapFlash.header.stagedBytes ||
        length > mapFlash.header.stagedBytes - compactOffset) {
        return false;
    }
    *outFlashOffset = mapFlash.header.payloadFlashOffset + compactOffset;
    return true;
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

    if (openReady || residentEnabled || !EspMapCatalog_isValidId(targetMapId)) {
        printf("[MAPFLASH] PREPARE FAILED reason=bad-boundary map=%u open=%u resident=%u\n",
               (unsigned int)targetMapId,
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

    if (esp_partition_read(partition, 0U, &cached, sizeof(cached)) == ESP_OK &&
        mapFlashHeaderLooksValid(cached, partition)) {
        cachedValid = 1U;
        cachedMapId = cached.currentMapId;
        if (!mapFlashHeaderMatchesPlan(cached,
                                       targetMapId,
                                       plan,
                                       &missReason)) {
            /* Exact reason is reported below before rebuilding. */
        }
        else {
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
            else if (!fnvFlashRange(cached.payloadFlashOffset,
                                    cached.stagedBytes,
                                    buffer,
                                    &flashPayloadFNV)) {
                missReason = "flash-payload-readback";
            }
            else if (flashPayloadFNV != cached.payloadFNV1a) {
                missReason = "flash-payload-fnv";
            }
            else {
                free(buffer);
                buffer = nullptr;
                mapFlash.partition = partition;
                mapFlash.header = cached;
                mapFlash.active = 1U;
                mapFlash.missLogs = 0U;
                memset(&mapFlash.stats, 0, sizeof(mapFlash.stats));
                mapFlash.stats.partitionBytes = partition->size;
                mapFlash.stats.sourcePackBytes = plan.sourcePackBytes;
                mapFlash.stats.sourceDataBytes = plan.sourceDataBytes;
                mapFlash.stats.indexBytes = plan.indexBytes;
                mapFlash.stats.metadataBytes = plan.payloadFlashOffset;
                mapFlash.stats.stagedBytes = plan.stagedBytes;
                mapFlash.stats.excludedBytes = plan.excludedBytes;
                mapFlash.stats.entryCount =
                    plan.entryCount <= UINT16_MAX
                        ? (uint16_t)plan.entryCount
                        : UINT16_MAX;
                mapFlash.stats.currentMapId = targetMapId;
                mapFlash.stats.excludedMaps =
                    plan.excludedCount <= UINT8_MAX
                        ? (uint8_t)plan.excludedCount
                        : UINT8_MAX;
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
    MapFlashExcludedSpan spans[kMapFlashMaxExcludedMaps] = {};
    EspAssetPackEntry currentEntry = {};
    const esp_partition_t* partition = nullptr;
    const char* currentMapName = nullptr;
    uint32_t excludedCount = 0U;
    uint32_t excludedBytes = 0U;
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
    const int64_t buildStart = esp_timer_get_time();

    if (openReady || residentEnabled || !EspMapCatalog_isValidId(currentMapId)) {
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

    for (uint8_t mapId = ESP_MAP_CATALOG_FIRST_ID;
         mapId <= ESP_MAP_CATALOG_LAST_ID; ++mapId) {
        if (mapId == currentMapId) continue;
        const char* mapName = EspMapCatalog_nameForId(mapId);
        EspAssetPackEntry mapEntry = {};
        if (mapName == nullptr ||
            !EspAssetPack_findEntry(mapName, &mapEntry)) {
            continue;
        }
        if ((mapEntry.flags & ESP_ASSET_PACK_FLAG_DIRECTORY) != 0U ||
            mapEntry.size == 0U || excludedCount >= kMapFlashMaxExcludedMaps) {
            return failStage("invalid-excluded-bsp");
        }
        spans[excludedCount].nameHash = mapEntry.nameHash;
        spans[excludedCount].sourceOffset = mapEntry.offset;
        spans[excludedCount].size = mapEntry.size;
        ++excludedCount;
    }

    for (uint32_t i = 0U; i < excludedCount; ++i) {
        for (uint32_t j = i + 1U; j < excludedCount; ++j) {
            if (spans[j].sourceOffset < spans[i].sourceOffset) {
                const MapFlashExcludedSpan temp = spans[i];
                spans[i] = spans[j];
                spans[j] = temp;
            }
        }
    }

    uint32_t previousEnd = sourceDataOffset;
    for (uint32_t i = 0U; i < excludedCount; ++i) {
        const MapFlashExcludedSpan& span = spans[i];
        if (span.sourceOffset < sourceDataOffset ||
            span.sourceOffset < previousEnd ||
            span.sourceOffset > sourcePackBytes ||
            span.size > sourcePackBytes - span.sourceOffset ||
            excludedBytes > UINT32_MAX - span.size) {
            return failStage("excluded-span-invalid");
        }
        previousEnd = span.sourceOffset + span.size;
        excludedBytes += span.size;
    }
    if (excludedBytes > sourceDataBytes) {
        return failStage("excluded-bytes-invalid");
    }

    indexBytes = sourceEntryCount * (uint32_t)kEntryBytes;
    payloadFlashOffset = alignSector(kMapFlashIndexOffset + indexBytes);
    stagedBytes = sourceDataBytes - excludedBytes;
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
               (unsigned int)excludedCount,
               (unsigned int)excludedBytes,
               (unsigned int)stagedBytes,
               (unsigned int)partition->size);
        return failStage("working-set-does-not-fit");
    }

    printf("[MAPFLASH] PLAN map=%u current=%s pack=%u entries=%u index=%u metadata=%u data=%u excludeMaps=%u excludeBytes=%u stage=%u partition=%u headroom=%u fits=yes\n",
           (unsigned int)currentMapId,
           currentMapName,
           (unsigned int)sourcePackBytes,
           (unsigned int)sourceEntryCount,
           (unsigned int)indexBytes,
           (unsigned int)payloadFlashOffset,
           (unsigned int)sourceDataBytes,
           (unsigned int)excludedCount,
           (unsigned int)excludedBytes,
           (unsigned int)stagedBytes,
           (unsigned int)partition->size,
           (unsigned int)(partition->size - payloadFlashOffset - stagedBytes));
    printf("[MAPFLASH] CONTRACT raw-partition single-slot; copy complete current-map gameplay working set before resident gameplay; original index+offset semantics retained; 19KiB RAM cache stays L1; header committed last after flash readback; excluded BSP access fails closed; SD fallback during active gameplay=no\n");

    buffer = (uint8_t*)malloc(kMapFlashCopyBufferBytes);
    if (buffer == nullptr) {
        return failStage("copy-buffer-allocation");
    }

    if (esp_partition_erase_range(partition, 0U, partition->size) != ESP_OK) {
        return failStage("partition-erase");
    }
    printf("[MAPFLASH] ERASE bytes=%u buffer=%u owner=transient\n",
           (unsigned int)partition->size,
           (unsigned int)kMapFlashCopyBufferBytes);

    if (!copySdRangeToFlash(sourceIndexOffset,
                            indexBytes,
                            kMapFlashIndexOffset,
                            buffer,
                            &indexFNV)) {
        return failStage("index-copy");
    }

    uint32_t sourceCursor = sourceDataOffset;
    uint32_t flashCursor = payloadFlashOffset;
    for (uint32_t i = 0U; i < excludedCount; ++i) {
        const MapFlashExcludedSpan& span = spans[i];
        if (span.sourceOffset > sourceCursor) {
            const uint32_t copyBytes = span.sourceOffset - sourceCursor;
            if (!copySdRangeToFlash(sourceCursor,
                                    copyBytes,
                                    flashCursor,
                                    buffer,
                                    &payloadFNV)) {
                return failStage("payload-copy-before-bsp");
            }
            flashCursor += copyBytes;
        }
        sourceCursor = span.sourceOffset + span.size;
    }
    if (sourceCursor < sourcePackBytes) {
        const uint32_t copyBytes = sourcePackBytes - sourceCursor;
        if (!copySdRangeToFlash(sourceCursor,
                                copyBytes,
                                flashCursor,
                                buffer,
                                &payloadFNV)) {
            return failStage("payload-copy-tail");
        }
        flashCursor += copyBytes;
    }
    if (flashCursor != payloadFlashOffset + stagedBytes) {
        return failStage("payload-size-mismatch");
    }

    if (!fnvFlashRange(kMapFlashIndexOffset,
                       indexBytes,
                       buffer,
                       &verifyIndexFNV) ||
        !fnvFlashRange(payloadFlashOffset,
                       stagedBytes,
                       buffer,
                       &verifyPayloadFNV) ||
        verifyIndexFNV != indexFNV || verifyPayloadFNV != payloadFNV) {
        return failStage("flash-readback-fnv");
    }

    MapFlashHeader committed = {};
    committed.magic = kMapFlashMagic;
    committed.version = kMapFlashVersion;
    committed.headerBytes = (uint16_t)sizeof(MapFlashHeader);
    committed.committed = kMapFlashCommitted;
    committed.sourcePackBytes = sourcePackBytes;
    committed.sourceIndexOffset = sourceIndexOffset;
    committed.sourceDataOffset = sourceDataOffset;
    committed.entryCount = sourceEntryCount;
    committed.indexFlashOffset = kMapFlashIndexOffset;
    committed.indexBytes = indexBytes;
    committed.payloadFlashOffset = payloadFlashOffset;
    committed.stagedBytes = stagedBytes;
    committed.excludedBytes = excludedBytes;
    committed.indexFNV1a = indexFNV;
    committed.payloadFNV1a = payloadFNV;
    committed.currentMapHash = currentMapHash;
    committed.currentMapId = currentMapId;
    committed.excludedCount = (uint8_t)excludedCount;
    for (uint32_t i = 0U; i < excludedCount; ++i) {
        committed.excluded[i] = spans[i];
    }

    if (esp_partition_write(partition, 0U, &committed, sizeof(committed)) != ESP_OK) {
        return failStage("header-commit");
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
    memset(&mapFlash.stats, 0, sizeof(mapFlash.stats));
    mapFlash.stats.partitionBytes = partition->size;
    mapFlash.stats.sourcePackBytes = sourcePackBytes;
    mapFlash.stats.sourceDataBytes = sourceDataBytes;
    mapFlash.stats.indexBytes = indexBytes;
    mapFlash.stats.metadataBytes = payloadFlashOffset;
    mapFlash.stats.stagedBytes = stagedBytes;
    mapFlash.stats.excludedBytes = excludedBytes;
    mapFlash.stats.buildMicros =
        (uint32_t)(esp_timer_get_time() - buildStart);
    mapFlash.stats.entryCount = (uint16_t)sourceEntryCount;
    mapFlash.stats.currentMapId = currentMapId;
    mapFlash.stats.excludedMaps = (uint8_t)excludedCount;
    mapFlash.stats.active = 1U;
    mapFlash.stats.verified = 1U;

    printf("[MAPFLASH] COPY indexFNV=%08x payloadFNV=%08x verified=yes\n",
           (unsigned int)indexFNV,
           (unsigned int)payloadFNV);
    printf("[MAPFLASH] READY map=%u staged=%u metadata=%u excluded=%u/%u buildUs=%u backing=raw-internal-flash SDGameplayReads=forbidden\n",
           (unsigned int)currentMapId,
           (unsigned int)stagedBytes,
           (unsigned int)payloadFlashOffset,
           (unsigned int)excludedCount,
           (unsigned int)excludedBytes,
           (unsigned int)mapFlash.stats.buildMicros);
    return 1;
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

int EspAssetPack_residentBegin(void)
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
    memset(&residentStats, 0, sizeof(residentStats));
}

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

int EspAssetPack_readRange(const EspAssetPackEntry* entry,
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

    cacheableSmall = residentEnabled && residentCache != nullptr &&
                     length <= kResidentMaxCachedRangeBytes;
    cacheableLarge = residentEnabled && residentLargeRangeEnabled &&
                     residentCache != nullptr &&
                     length == kResidentLargeRangeBytes;

    if (cacheableSmall || cacheableLarge) {
        ResidentRangeRecord* record =
            findResidentRange(entry->nameHash, relativeOffset, (uint32_t)length);
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

    const uint32_t absoluteOffset = entry->offset + relativeOffset;
    if (physicalUsesMapFlash) {
        uint32_t flashOffset = 0U;
        if (!mapFlashTranslate(absoluteOffset, (uint32_t)length, &flashOffset) ||
            !mapFlashRead(flashOffset, destination, length, true)) {
            logMapFlashStrictMiss(entry, relativeOffset, length);
            return 0;
        }
    }
    else {
        if (!packFile.seek(absoluteOffset) || !readSdExact(destination, length)) {
            return 0;
        }
    }

    if (cacheableSmall) {
        storeResidentSmallRange(entry->nameHash,
                                relativeOffset,
                                destination,
                                (uint32_t)length);
    }
    else if (cacheableLarge) {
        storeResidentLargeRange(entry->nameHash,
                                relativeOffset,
                                destination,
                                (uint32_t)length);
    }
    return 1;
}
