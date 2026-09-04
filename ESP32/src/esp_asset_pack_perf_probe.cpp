#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_player_view_state.h"

namespace {

constexpr uint32_t kReportLogicalCallThreshold = 64U;

struct PakIoSample {
    uint32_t logicalCalls;
    uint64_t logicalMicros;
    uint32_t maxLogicalMicros;
};

PakIoSample ioSample = {};
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

    printf("[PAKIO] SAMPLE reason=%s backing=%s logical=%u totalUs=%lluus avgUs=%lluus maxUs=%uus physicalReads=%u physicalBytes=%u resident=%u\n",
           reason != nullptr ? reason : "threshold",
           EspAssetPack_isMapFlashActive() ? "raw-flash" : "sd",
           (unsigned int)ioSample.logicalCalls,
           (unsigned long long)ioSample.logicalMicros,
           (unsigned long long)avgLogical,
           (unsigned int)ioSample.maxLogicalMicros,
           (unsigned int)physicalReads,
           (unsigned int)physicalBytes,
           (unsigned int)EspAssetPack_isResident());
    resetIoSample();
}

} // namespace

extern "C" {

int __real_EspAssetPack_residentBegin(void);
int __real_EspAssetPack_readRange(const EspAssetPackEntry* entry,
                                  uint32_t relativeOffset,
                                  void* destination,
                                  size_t length);
void __real_EspAssetPack_residentResetStats(void);
int __real_EspAssetPack_residentEnd(void);

int __wrap_EspAssetPack_residentBegin(void)
{
    const EspPlayerViewState* view = EspPlayerView_view();
    const int64_t prepareStart = esp_timer_get_time();
    if (view == nullptr || view->active != 1U ||
        !EspAssetPack_mapFlashPrepare(view->targetMapId)) {
        printf("[MAPFLASH] ARM failed view=%u map=%u residentBegin=blocked\n",
               view != nullptr ? (unsigned int)view->active : 0U,
               view != nullptr ? (unsigned int)view->targetMapId : 0U);
        return 0;
    }
    const uint32_t prepareUs =
        elapsedMicros(prepareStart, esp_timer_get_time());

    const int result = __real_EspAssetPack_residentBegin();
    if (!result) {
        printf("[MAPFLASH] ARM failed map=%u reason=resident-cache-begin\n",
               (unsigned int)view->targetMapId);
        EspAssetPack_mapFlashDeactivate();
        return 0;
    }

    EspAssetPackMapFlashStats flash = {};
    EspAssetPack_mapFlashGetStats(&flash);
    printf("[MAPFLASH] ARM map=%u active=%u verified=%u reused=%u staged=%u metadata=%u prepareUs=%u buildUs=%u resident=1\n",
           (unsigned int)flash.currentMapId,
           (unsigned int)flash.active,
           (unsigned int)flash.verified,
           (unsigned int)flash.reused,
           (unsigned int)flash.stagedBytes,
           (unsigned int)flash.metadataBytes,
           (unsigned int)prepareUs,
           (unsigned int)flash.buildMicros);
    return 1;
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
