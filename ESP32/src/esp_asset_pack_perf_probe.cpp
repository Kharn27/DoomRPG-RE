#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_partition.h>
#include <esp_timer.h>

#include "esp_asset_pack.h"

namespace {

constexpr uint32_t kFlashMetadataReserveBytes = 4096U;
constexpr uint32_t kReportLogicalCallThreshold = 64U;

struct PakIoSample {
    uint32_t logicalCalls;
    uint64_t logicalMicros;
    uint32_t maxLogicalMicros;
};

PakIoSample ioSample = {};
bool flashFeasibilityLogged = false;
bool physicalBaselineValid = false;
uint32_t baselinePhysicalReads = 0U;
uint32_t baselinePhysicalBytes = 0U;

uint32_t elapsedMicros(int64_t start, int64_t end)
{
    if (end <= start) {
        return 0U;
    }
    const uint64_t delta = (uint64_t)(end - start);
    return delta > UINT32_MAX ? UINT32_MAX : (uint32_t)delta;
}

void resetIoSample()
{
    memset(&ioSample, 0, sizeof(ioSample));
}

void invalidatePhysicalBaseline()
{
    physicalBaselineValid = false;
    baselinePhysicalReads = 0U;
    baselinePhysicalBytes = 0U;
}

void capturePhysicalBaseline()
{
    EspAssetPackResidentStats stats = {};
    if (!EspAssetPack_isResident()) {
        invalidatePhysicalBaseline();
        return;
    }
    EspAssetPack_residentGetStats(&stats);
    baselinePhysicalReads = stats.physicalReads;
    baselinePhysicalBytes = stats.physicalBytes;
    physicalBaselineValid = true;
}

void reportIoSample(const char* reason)
{
    EspAssetPackResidentStats stats = {};
    uint32_t physicalReads = 0U;
    uint32_t physicalBytes = 0U;

    if (ioSample.logicalCalls == 0U) {
        return;
    }

    if (EspAssetPack_isResident()) {
        EspAssetPack_residentGetStats(&stats);
        if (physicalBaselineValid) {
            physicalReads = stats.physicalReads >= baselinePhysicalReads
                                ? stats.physicalReads - baselinePhysicalReads
                                : 0U;
            physicalBytes = stats.physicalBytes >= baselinePhysicalBytes
                                ? stats.physicalBytes - baselinePhysicalBytes
                                : 0U;
        }
        baselinePhysicalReads = stats.physicalReads;
        baselinePhysicalBytes = stats.physicalBytes;
        physicalBaselineValid = true;
    }
    else {
        invalidatePhysicalBaseline();
    }

    const uint64_t avgLogical =
        ioSample.logicalMicros / (uint64_t)ioSample.logicalCalls;

    printf("[PAKIO] SAMPLE reason=%s logical=%u totalUs=%lluus avgUs=%lluus maxUs=%uus physicalReads=%u physicalBytes=%u resident=%u\n",
           reason != nullptr ? reason : "threshold",
           (unsigned int)ioSample.logicalCalls,
           (unsigned long long)ioSample.logicalMicros,
           (unsigned long long)avgLogical,
           (unsigned int)ioSample.maxLogicalMicros,
           (unsigned int)physicalReads,
           (unsigned int)physicalBytes,
           (unsigned int)EspAssetPack_isResident());
    resetIoSample();
}

void logFlashFeasibility()
{
    const esp_partition_t* partition;
    uint32_t capacity = 0U;
    const uint32_t packBytes = EspAssetPack_fileSize();

    if (flashFeasibilityLogged || packBytes == 0U) {
        return;
    }
    flashFeasibilityLogged = true;

    partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                         ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                                         "spiffs");
    if (partition != nullptr && partition->size > kFlashMetadataReserveBytes) {
        capacity = partition->size - kFlashMetadataReserveBytes;
    }

    if (partition == nullptr) {
        printf("[PAKFLASH] FEASIBILITY pack=%u partition=missing reserve=%u capacity=0 fits=no mode=measurement-only\n",
               (unsigned int)packBytes,
               (unsigned int)kFlashMetadataReserveBytes);
        return;
    }

    const bool fits = packBytes <= capacity;
    const uint32_t headroom = fits ? capacity - packBytes : 0U;
    printf("[PAKFLASH] FEASIBILITY pack=%u partition=%s address=%08x size=%u reserve=%u capacity=%u fits=%s headroom=%u mode=measurement-only\n",
           (unsigned int)packBytes,
           partition->label,
           (unsigned int)partition->address,
           (unsigned int)partition->size,
           (unsigned int)kFlashMetadataReserveBytes,
           (unsigned int)capacity,
           fits ? "yes" : "no",
           (unsigned int)headroom);
    printf("[PAKFLASH] CONTRACT no erase/write/cache activation in this milestone; SD remains authoritative backing; PAKIO batches time only real readRange calls and samples cache counters outside the timed section\n");
}

} // namespace

extern "C" {

int __real_EspAssetPack_open(const char* path);
int __real_EspAssetPack_readRange(const EspAssetPackEntry* entry,
                                  uint32_t relativeOffset,
                                  void* destination,
                                  size_t length);
void __real_EspAssetPack_residentResetStats(void);
int __real_EspAssetPack_residentEnd(void);

int __wrap_EspAssetPack_open(const char* path)
{
    const int result = __real_EspAssetPack_open(path);
    if (result && path != nullptr &&
        strcmp(path, ESP_ASSET_PACK_DEFAULT_PATH) == 0) {
        logFlashFeasibility();
    }
    return result;
}

int __wrap_EspAssetPack_readRange(const EspAssetPackEntry* entry,
                                  uint32_t relativeOffset,
                                  void* destination,
                                  size_t length)
{
    if (EspAssetPack_isResident() && !physicalBaselineValid) {
        capturePhysicalBaseline();
    }

    const int64_t start = esp_timer_get_time();
    const int result = __real_EspAssetPack_readRange(
        entry, relativeOffset, destination, length);
    const uint32_t duration = elapsedMicros(start, esp_timer_get_time());

    ++ioSample.logicalCalls;
    ioSample.logicalMicros += duration;
    if (duration > ioSample.maxLogicalMicros) {
        ioSample.maxLogicalMicros = duration;
    }

    if (ioSample.logicalCalls >= kReportLogicalCallThreshold) {
        reportIoSample("threshold");
    }
    return result;
}

void __wrap_EspAssetPack_residentResetStats(void)
{
    reportIoSample("resident-reset");
    __real_EspAssetPack_residentResetStats();
    invalidatePhysicalBaseline();
}

int __wrap_EspAssetPack_residentEnd(void)
{
    reportIoSample("resident-end");
    invalidatePhysicalBaseline();
    return __real_EspAssetPack_residentEnd();
}

} // extern "C"
