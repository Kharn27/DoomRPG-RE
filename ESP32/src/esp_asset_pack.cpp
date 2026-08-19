#include <Arduino.h>
#include <SD.h>
#include <string.h>

#include "esp_asset_pack.h"

namespace {

constexpr uint8_t kMagic[8] = {'D', 'R', 'P', 'G', 'E', 'S', 'P', '1'};
constexpr uint32_t kVersion = 1;
constexpr size_t kHeaderBytes = 16;
constexpr size_t kEntryBytes = 32;

File packFile;
EspAssetPackEntry entries[ESP_ASSET_PACK_MAX_ENTRIES];
int entryCount = 0;
uint32_t packSize = 0;
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
    memset(entries, 0, sizeof(entries));
    entryCount = 0;
    packSize = 0;
    openReady = false;
}

bool validateEntry(const EspAssetPackEntry& entry)
{
    if (entry.name[0] == '\0') {
        return false;
    }
    if (entry.offset < kHeaderBytes) {
        return false;
    }
    if (entry.offset > packSize || entry.size > packSize - entry.offset) {
        return false;
    }
    return true;
}

} // namespace

int EspAssetPack_open(const char* path)
{
    uint8_t header[kHeaderBytes];

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

    const uint32_t count = readLe32(header + 12);
    if (count == 0 || count > ESP_ASSET_PACK_MAX_ENTRIES) {
        resetState();
        return 0;
    }

    const uint64_t indexEnd =
        (uint64_t)kHeaderBytes + ((uint64_t)count * (uint64_t)kEntryBytes);
    if (indexEnd > packSize) {
        resetState();
        return 0;
    }

    for (uint32_t i = 0; i < count; ++i) {
        uint8_t raw[kEntryBytes];
        if (!readExact(raw, sizeof(raw))) {
            resetState();
            return 0;
        }

        memcpy(entries[i].name, raw, 16);
        entries[i].name[16] = '\0';
        entries[i].offset = readLe32(raw + 16);
        entries[i].size = readLe32(raw + 20);
        entries[i].crc32 = readLe32(raw + 24);
        entries[i].flags = readLe32(raw + 28);

        if (!validateEntry(entries[i]) || entries[i].offset < indexEnd) {
            resetState();
            return 0;
        }
    }

    entryCount = (int)count;
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
    return openReady ? entryCount : 0;
}

int EspAssetPack_getEntryByIndex(int index, EspAssetPackEntry* outEntry)
{
    if (!openReady || outEntry == nullptr || index < 0 || index >= entryCount) {
        return 0;
    }
    *outEntry = entries[index];
    return 1;
}

int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* outEntry)
{
    if (!openReady || name == nullptr || outEntry == nullptr) {
        return 0;
    }

    for (int i = 0; i < entryCount; ++i) {
        if (strcasecmp(name, entries[i].name) == 0) {
            *outEntry = entries[i];
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

    if (relativeOffset > entry->size ||
        length > (size_t)(entry->size - relativeOffset)) {
        return 0;
    }

    const uint32_t absoluteOffset = entry->offset + relativeOffset;
    if (!packFile.seek(absoluteOffset)) {
        return 0;
    }

    return packFile.read(static_cast<uint8_t*>(destination), length) == length ? 1 : 0;
}
