#include <Arduino.h>
#include <SD.h>
#include <string.h>

#include "esp_asset_pack.h"

namespace {

constexpr uint8_t kMagic[8] = {'D', 'R', 'P', 'G', 'E', 'S', 'P', '2'};
constexpr uint32_t kVersion = ESP_ASSET_PACK_FORMAT_VERSION;
constexpr size_t kHeaderBytes = 24;
constexpr size_t kEntryBytes = 20;

File packFile;
uint32_t entryCount = 0;
uint32_t packSize = 0;
uint32_t indexOffset = 0;
uint32_t dataOffset = 0;
bool openReady = false;

uint32_t readLe32(const uint8_t* data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

bool readExact(void* destination, size_t length)
{
    if (!packFile || destination == nullptr) {
        return false;
    }
    return packFile.read(static_cast<uint8_t*>(destination), length) == length;
}

void resetState()
{
    if (packFile) {
        packFile.close();
    }
    entryCount = 0;
    packSize = 0;
    indexOffset = 0;
    dataOffset = 0;
    openReady = false;
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

} // namespace

int EspAssetPack_open(const char* path)
{
    uint8_t header[kHeaderBytes];
    uint32_t previousHash = 0;
    bool havePreviousHash = false;

    resetState();

    if (path == nullptr || path[0] == '\0') {
        return 0;
    }

    packFile = SD.open(path, FILE_READ);
    if (!packFile) {
        return 0;
    }

    packSize = (uint32_t)packFile.size();
    if (packSize < kHeaderBytes || !readExact(header, sizeof(header))) {
        resetState();
        return 0;
    }

    if (memcmp(header, kMagic, sizeof(kMagic)) != 0 ||
        readLe32(header + 8) != kVersion) {
        resetState();
        return 0;
    }

    entryCount = readLe32(header + 12);
    indexOffset = readLe32(header + 16);
    dataOffset = readLe32(header + 20);

    if (entryCount == 0 || entryCount > ESP_ASSET_PACK_MAX_ENTRY_COUNT ||
        indexOffset < kHeaderBytes || indexOffset > packSize) {
        resetState();
        return 0;
    }

    const uint64_t indexEnd =
        (uint64_t)indexOffset + ((uint64_t)entryCount * (uint64_t)kEntryBytes);
    if (indexEnd > packSize || dataOffset < indexEnd || dataOffset > packSize) {
        resetState();
        return 0;
    }

    /* Validate the complete disk index once at open time. This is a sequential
     * 20-byte scan: no resident index allocation and no payload reads.
     */
    if (!packFile.seek(indexOffset)) {
        resetState();
        return 0;
    }

    for (uint32_t i = 0; i < entryCount; ++i) {
        uint8_t raw[kEntryBytes];
        EspAssetPackEntry entry;

        if (!readExact(raw, sizeof(raw))) {
            resetState();
            return 0;
        }
        decodeEntry(raw, &entry);

        if (!validateEntry(entry) ||
            (havePreviousHash && entry.nameHash <= previousHash)) {
            resetState();
            return 0;
        }

        previousHash = entry.nameHash;
        havePreviousHash = true;
    }

    openReady = true;
    return 1;
}

void EspAssetPack_close(void)
{
    resetState();
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

    const uint32_t absoluteOffset = entry->offset + relativeOffset;
    if (!packFile.seek(absoluteOffset)) {
        return 0;
    }

    return packFile.read(static_cast<uint8_t*>(destination), length) == length ? 1 : 0;
}
