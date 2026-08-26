#include <Arduino.h>
#include <SD.h>
#include <stdlib.h>
#include <string.h>

#include "esp_asset_pack.h"

namespace {

constexpr uint8_t kMagic[8] = {'D', 'R', 'P', 'G', 'E', 'S', 'P', '2'};
constexpr uint32_t kVersion = ESP_ASSET_PACK_FORMAT_VERSION;
constexpr size_t kHeaderBytes = 24;
constexpr size_t kEntryBytes = 20;
constexpr uint32_t kResidentRangeCapacityBytes = 16U * 1024U;
constexpr uint32_t kResidentRangeEntryCapacity = 128U;
constexpr uint32_t kResidentEntryCacheSlots = 24U;
constexpr uint32_t kResidentMaxCachedRangeBytes = 1024U;

struct ResidentRangeRecord {
    uint32_t nameHash;
    uint32_t relativeOffset;
    uint32_t length;
    uint32_t dataOffset;
};

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

File packFile;
uint32_t entryCount = 0;
uint32_t packSize = 0;
uint32_t indexOffset = 0;
uint32_t dataOffset = 0;
bool openReady = false;
bool physicalReady = false;
bool physicalIsDefault = false;
bool residentEnabled = false;
ResidentCache* residentCache = nullptr;
EspAssetPackResidentStats residentStats = {};

uint32_t readLe32(const uint8_t* data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

void clearResidentCache()
{
    if (residentCache == nullptr) {
        return;
    }
    residentCache->rangeCount = 0U;
    residentCache->bytesUsed = 0U;
    memset(residentCache->entries, 0, sizeof(residentCache->entries));
}

bool readExact(void* destination, size_t length)
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
    if (entry.nameHash == 0) {
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

bool readEntryAt(uint32_t index, EspAssetPackEntry* outEntry)
{
    uint8_t raw[kEntryBytes];

    if (!packFile || outEntry == nullptr || index >= entryCount) {
        return false;
    }

    const uint64_t absoluteOffset =
        (uint64_t)indexOffset + ((uint64_t)index * (uint64_t)kEntryBytes);
    if (absoluteOffset > UINT32_MAX || !packFile.seek((uint32_t)absoluteOffset)) {
        return false;
    }
    if (!readExact(raw, sizeof(raw))) {
        return false;
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

void storeResidentRange(uint32_t nameHash,
                        uint32_t relativeOffset,
                        const void* source,
                        uint32_t length)
{
    if (!residentEnabled || residentCache == nullptr || source == nullptr ||
        length == 0U || length > kResidentMaxCachedRangeBytes) {
        ++residentStats.rangeCacheBypasses;
        return;
    }
    if (residentCache->rangeCount >= kResidentRangeEntryCapacity ||
        length > kResidentRangeCapacityBytes - residentCache->bytesUsed) {
        ++residentStats.rangeCacheBypasses;
        return;
    }

    ResidentRangeRecord* record =
        &residentCache->ranges[residentCache->rangeCount++];
    record->nameHash = nameHash;
    record->relativeOffset = relativeOffset;
    record->length = length;
    record->dataOffset = residentCache->bytesUsed;
    memcpy(residentCache->bytes + record->dataOffset, source, length);
    residentCache->bytesUsed += length;
    ++residentStats.rangeCacheStores;
}

} // namespace

int EspAssetPack_open(const char* path)
{
    uint8_t header[kHeaderBytes];
    uint32_t previousHash = 0;
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

    packFile = SD.open(path, FILE_READ);
    if (!packFile) {
        return 0;
    }
    if (residentEnabled) {
        ++residentStats.physicalOpens;
    }

    packSize = (uint32_t)packFile.size();
    if (packSize < kHeaderBytes || !readExact(header, sizeof(header))) {
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

    if (entryCount == 0 || entryCount > ESP_ASSET_PACK_MAX_ENTRY_COUNT ||
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

    /* Validate the complete disk index once at physical-open time. Resident
     * gameplay leases reuse this validated File without rescanning the index.
     */
    if (!packFile.seek(indexOffset)) {
        resetPhysicalState();
        return 0;
    }

    for (uint32_t i = 0; i < entryCount; ++i) {
        uint8_t raw[kEntryBytes];
        EspAssetPackEntry entry;

        if (!readExact(raw, sizeof(raw))) {
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
    return openReady ? packSize : 0;
}

int EspAssetPack_entryCount(void)
{
    return openReady ? (int)entryCount : 0;
}

uint32_t EspAssetPack_dataOffset(void)
{
    return openReady ? dataOffset : 0;
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
    residentEnabled = false;
    resetPhysicalState();
    if (residentCache != nullptr) {
        free(residentCache);
        residentCache = nullptr;
    }
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
    if (residentCache != nullptr) {
        outStats->rangeCacheBytesUsed = residentCache->bytesUsed;
        outStats->rangeCacheEntries = residentCache->rangeCount;
    }
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
        return 0;
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

    return sawByte ? hash : 0;
}

int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* outEntry)
{
    uint32_t low = 0;
    uint32_t high = entryCount;
    const uint32_t wantedHash = EspAssetPack_nameHash(name);

    if (!openReady || outEntry == nullptr || wantedHash == 0) {
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

    if (residentEnabled && residentCache != nullptr &&
        length <= kResidentMaxCachedRangeBytes) {
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
    if (!packFile.seek(absoluteOffset) || !readExact(destination, length)) {
        return 0;
    }

    if (residentEnabled && length <= kResidentMaxCachedRangeBytes) {
        storeResidentRange(entry->nameHash,
                           relativeOffset,
                           destination,
                           (uint32_t)length);
    }
    return 1;
}
