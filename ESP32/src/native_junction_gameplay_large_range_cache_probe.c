#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include <esp_heap_caps.h>

#include "esp_asset_pack.h"
#include "esp_native_gameplay_frame.h"
#include "esp_player_view_state.h"
#include "native_junction_gameplay_large_range_cache_probe.h"
#include "native_junction_gameplay_render_resource_cache_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_FRAME_FNV 0xba3e5182U
#define EXPECTED_VIEWPORT_FNV 0x9206eb24U
#define EXPECTED_HUD_BANDS_FNV 0x6c2aa46fU
#define WORLD_Y 20U
#define WORLD_ROWS 80U
#define HUD_ROWS 20U
#define BOTTOM_HUD_Y 100U
#define LARGE_RANGE_BYTES 2048U
#define PREDECESSOR_WARM_READS 22U
#define PREDECESSOR_WARM_BYTES (PREDECESSOR_WARM_READS * LARGE_RANGE_BYTES)

static uint8_t probeDone;
static uint8_t probeFailed;

static uint32_t fnvUpdate(uint32_t hash, const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t fnv(const void* data, uint32_t bytes) {
    return fnvUpdate(2166136261U, data, bytes);
}

static uint32_t frameFNV(void) {
    const void* fb = Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    const size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                            DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    return fb != NULL && bytes == expected ? fnv(fb, (uint32_t)bytes) : 0U;
}

static uint32_t viewportFNV(void) {
    const uint16_t* fb = (const uint16_t*)Esp32PlatformVideo_framebuffer();
    if (fb == NULL) return 0U;
    return fnv(fb + WORLD_Y * DOOMRPG_LOGICAL_WIDTH,
               DOOMRPG_LOGICAL_WIDTH * WORLD_ROWS * sizeof(uint16_t));
}

static uint32_t hudFNV(void) {
    const uint16_t* fb = (const uint16_t*)Esp32PlatformVideo_framebuffer();
    uint32_t hash = 2166136261U;
    if (fb == NULL) return 0U;
    hash = fnvUpdate(hash, fb,
                     DOOMRPG_LOGICAL_WIDTH * HUD_ROWS * sizeof(uint16_t));
    return fnvUpdate(hash,
                     fb + BOTTOM_HUD_Y * DOOMRPG_LOGICAL_WIDTH,
                     DOOMRPG_LOGICAL_WIDTH * HUD_ROWS * sizeof(uint16_t));
}

static uint32_t heap8(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static int exactFrame(const EspNativeGameplayFrameStats* frame) {
    return frame != NULL && frameFNV() == EXPECTED_FRAME_FNV &&
           viewportFNV() == EXPECTED_VIEWPORT_FNV &&
           hudFNV() == EXPECTED_HUD_BANDS_FNV &&
           frame->frameAfterFNV == EXPECTED_FRAME_FNV &&
           frame->viewportAfterSpritesFNV == EXPECTED_VIEWPORT_FNV &&
           frame->hudBandsAfterFNV == EXPECTED_HUD_BANDS_FNV &&
           frame->temporaryHudBytes == 0U &&
           frame->worldRouteNoPresent == 1U &&
           frame->finalPresented == 1U && frame->active == 1U;
}

static void failProbe(const char* reason) {
    EspAssetPackResidentStats stats;
    memset(&stats, 0, sizeof(stats));
    EspAssetPack_residentGetStats(&stats);
    printf("[LARGECACHEPROBE] FAILED %s frame=%08x viewport=%08x hud=%08x pack=%d resident=%u large=%u/%u reads=%u bytes=%u cache=%u/%uB entries=%u/%u\n",
           reason, (unsigned int)frameFNV(), (unsigned int)viewportFNV(),
           (unsigned int)hudFNV(), EspAssetPack_isOpen(),
           (unsigned int)stats.ready,
           (unsigned int)stats.largeRangeEnabled,
           (unsigned int)stats.largeRangeEntries,
           (unsigned int)stats.physicalReads,
           (unsigned int)stats.physicalBytes,
           (unsigned int)stats.rangeCacheBytesUsed,
           (unsigned int)stats.rangeCacheCapacityBytes,
           (unsigned int)stats.rangeCacheEntries,
           (unsigned int)stats.rangeCacheEntryCapacity);
    if (!EspAssetPack_isOpen() && EspAssetPack_isResidentLargeRangeEnabled()) {
        (void)EspAssetPack_residentLargeRangeEnd();
    }
    probeFailed = 1U;
    probeDone = 1U;
}

void Esp32JunctionGameplayLargeRangeCacheProbe_reset(void) {
    if (!EspAssetPack_isOpen() && EspAssetPack_isResidentLargeRangeEnabled()) {
        (void)EspAssetPack_residentLargeRangeEnd();
    }
    probeDone = 0U;
    probeFailed = 0U;
}

int Esp32JunctionGameplayLargeRangeCacheProbe_isDone(void) {
    return probeDone != 0U && probeFailed == 0U;
}

void Esp32JunctionGameplayLargeRangeCacheProbe_service(
    struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view;
    EspPlayerViewState viewBefore;
    EspNativeGameplayFrameStats learnFrame;
    EspNativeGameplayFrameStats warmFrame;
    EspAssetPackResidentStats baseline;
    EspAssetPackResidentStats enabled;
    EspAssetPackResidentStats learnPack;
    EspAssetPackResidentStats warmPack;
    uint32_t heapBefore;
    uint32_t largestBefore;
    uint32_t expectedLargeEntries;

    if (probeDone || probeFailed ||
        !Esp32JunctionGameplayRenderResourceCacheProbe_isDone()) return;

    printf("\n=== Doom RPG ESP32-native shared-payload large texture cache ===\n");
    printf("[LARGECACHEPROBE] CONTRACT preserve the proven <=1024B resident cache unchanged, then opt in exact 2048B immutable ranges using only free tail bytes of the same 16KiB owner. Small ranges keep priority, activation allocates nothing, canonical North stays bit-exact, and the second large-cache frame must save one physical 2048B read per retained tail slot.\n");

    memset(&baseline, 0, sizeof(baseline));
    EspAssetPack_residentGetStats(&baseline);
    view = EspPlayerView_view();
    if (doomRpg == NULL || doomRpg->render == NULL || view == NULL ||
        view->active != 1U || view->viewX != 992 || view->viewY != 1888 ||
        view->viewAngle != 64 || view->destAngle != 64 ||
        frameFNV() != EXPECTED_FRAME_FNV ||
        viewportFNV() != EXPECTED_VIEWPORT_FNV ||
        hudFNV() != EXPECTED_HUD_BANDS_FNV || EspAssetPack_isOpen() ||
        !EspAssetPack_isResident() || EspAssetPack_isResidentLargeRangeEnabled() ||
        baseline.enabled != 1U || baseline.ready != 1U ||
        baseline.largeRangeEnabled != 0U || baseline.largeRangeEntries != 0U ||
        baseline.physicalReads != PREDECESSOR_WARM_READS ||
        baseline.physicalBytes != PREDECESSOR_WARM_BYTES ||
        baseline.rangeCacheCapacityBytes <= baseline.rangeCacheBytesUsed ||
        doomRpg->render->shapeData != NULL ||
        doomRpg->render->mediaTexels != NULL) {
        failProbe("predecessor warm boundary");
        return;
    }

    expectedLargeEntries =
        (baseline.rangeCacheCapacityBytes - baseline.rangeCacheBytesUsed) /
        LARGE_RANGE_BYTES;
    if (expectedLargeEntries == 0U || expectedLargeEntries > UINT8_MAX ||
        expectedLargeEntries > baseline.rangeCacheEntryCapacity -
                                   baseline.rangeCacheEntries) {
        failProbe("resident slack cannot hold bounded large slot");
        return;
    }

    viewBefore = *view;
    heapBefore = heap8();
    largestBefore = largest8();
    if (!EspAssetPack_residentLargeRangeBegin() ||
        !EspAssetPack_isResidentLargeRangeEnabled() || EspAssetPack_isOpen() ||
        heap8() != heapBefore || largest8() != largestBefore) {
        failProbe("large range activation");
        return;
    }
    memset(&enabled, 0, sizeof(enabled));
    EspAssetPack_residentGetStats(&enabled);
    if (enabled.ownerBytes != baseline.ownerBytes ||
        enabled.rangeCacheBytesUsed != baseline.rangeCacheBytesUsed ||
        enabled.rangeCacheEntries != baseline.rangeCacheEntries ||
        enabled.largeRangeEnabled != 1U || enabled.largeRangeEntries != 0U) {
        failProbe("activation changed resident owner");
        return;
    }

    memset(&learnFrame, 0, sizeof(learnFrame));
    EspAssetPack_residentResetStats();
    if (!EspNativeGameplayFrame_renderTurn(doomRpg->render, 64U, &learnFrame)) {
        failProbe("learn render");
        return;
    }
    memset(&learnPack, 0, sizeof(learnPack));
    EspAssetPack_residentGetStats(&learnPack);
    view = EspPlayerView_view();
    if (!exactFrame(&learnFrame) || view == NULL ||
        memcmp(view, &viewBefore, sizeof(viewBefore)) != 0 ||
        EspAssetPack_isOpen() || !EspAssetPack_isResident() ||
        !EspAssetPack_isResidentLargeRangeEnabled() ||
        learnPack.physicalOpens != 0U || learnPack.validationPasses != 0U ||
        learnPack.residentReuses < 3U ||
        learnPack.physicalReads > PREDECESSOR_WARM_READS ||
        learnPack.physicalBytes != learnPack.physicalReads * LARGE_RANGE_BYTES ||
        learnPack.rangeCacheStores != expectedLargeEntries ||
        learnPack.largeRangeEntries != expectedLargeEntries ||
        learnPack.rangeCacheBytesUsed != baseline.rangeCacheBytesUsed +
                                             expectedLargeEntries * LARGE_RANGE_BYTES ||
        learnPack.rangeCacheEntries != baseline.rangeCacheEntries +
                                           expectedLargeEntries ||
        heap8() != heapBefore || largest8() != largestBefore ||
        doomRpg->render->shapeData != NULL ||
        doomRpg->render->mediaTexels != NULL) {
        failProbe("learn postcondition");
        return;
    }

    memset(&warmFrame, 0, sizeof(warmFrame));
    EspAssetPack_residentResetStats();
    if (!EspNativeGameplayFrame_renderTurn(doomRpg->render, 64U, &warmFrame)) {
        failProbe("warm render");
        return;
    }
    memset(&warmPack, 0, sizeof(warmPack));
    EspAssetPack_residentGetStats(&warmPack);
    view = EspPlayerView_view();
    if (!exactFrame(&warmFrame) || view == NULL ||
        memcmp(view, &viewBefore, sizeof(viewBefore)) != 0 ||
        EspAssetPack_isOpen() || !EspAssetPack_isResident() ||
        !EspAssetPack_isResidentLargeRangeEnabled() ||
        warmPack.physicalOpens != 0U || warmPack.validationPasses != 0U ||
        warmPack.residentReuses < 3U || warmPack.rangeCacheStores != 0U ||
        warmPack.largeRangeEntries != expectedLargeEntries ||
        warmPack.rangeCacheBytesUsed != learnPack.rangeCacheBytesUsed ||
        warmPack.rangeCacheEntries != learnPack.rangeCacheEntries ||
        warmPack.physicalReads + expectedLargeEntries != learnPack.physicalReads ||
        warmPack.physicalBytes != warmPack.physicalReads * LARGE_RANGE_BYTES ||
        warmPack.rangeCacheHits < learnPack.rangeCacheHits + expectedLargeEntries ||
        heap8() != heapBefore || largest8() != largestBefore ||
        doomRpg->render->shapeData != NULL ||
        doomRpg->render->mediaTexels != NULL) {
        failProbe("warm postcondition");
        return;
    }

    printf("[LARGECACHE] BASE sdReads=%u sdBytes=%u cache=%u/%uB entries=%u/%u large=off owner=%uB heap=%u largest=%u\n",
           (unsigned int)baseline.physicalReads,
           (unsigned int)baseline.physicalBytes,
           (unsigned int)baseline.rangeCacheBytesUsed,
           (unsigned int)baseline.rangeCacheCapacityBytes,
           (unsigned int)baseline.rangeCacheEntries,
           (unsigned int)baseline.rangeCacheEntryCapacity,
           (unsigned int)baseline.ownerBytes,
           (unsigned int)heapBefore, (unsigned int)largestBefore);
    printf("[LARGECACHE] LEARN slots=%u sdReads=%u sdBytes=%u range=%uH/%uM/%uS/%uB cache=%u/%uB entries=%u/%u large=%u timeUs=%u/%u/%u/%u total=%u exact=yes\n",
           (unsigned int)expectedLargeEntries,
           (unsigned int)learnPack.physicalReads,
           (unsigned int)learnPack.physicalBytes,
           (unsigned int)learnPack.rangeCacheHits,
           (unsigned int)learnPack.rangeCacheMisses,
           (unsigned int)learnPack.rangeCacheStores,
           (unsigned int)learnPack.rangeCacheBypasses,
           (unsigned int)learnPack.rangeCacheBytesUsed,
           (unsigned int)learnPack.rangeCacheCapacityBytes,
           (unsigned int)learnPack.rangeCacheEntries,
           (unsigned int)learnPack.rangeCacheEntryCapacity,
           (unsigned int)learnPack.largeRangeEntries,
           (unsigned int)learnFrame.worldMicros,
           (unsigned int)learnFrame.spriteMicros,
           (unsigned int)learnFrame.hudMicros,
           (unsigned int)learnFrame.presentMicros,
           (unsigned int)learnFrame.totalMicros);
    printf("[LARGECACHE] WARM slots=%u sdReads=%u sdBytes=%u range=%uH/%uM/%uS/%uB cache=%u/%uB entries=%u/%u large=%u timeUs=%u/%u/%u/%u total=%u exact=yes\n",
           (unsigned int)expectedLargeEntries,
           (unsigned int)warmPack.physicalReads,
           (unsigned int)warmPack.physicalBytes,
           (unsigned int)warmPack.rangeCacheHits,
           (unsigned int)warmPack.rangeCacheMisses,
           (unsigned int)warmPack.rangeCacheStores,
           (unsigned int)warmPack.rangeCacheBypasses,
           (unsigned int)warmPack.rangeCacheBytesUsed,
           (unsigned int)warmPack.rangeCacheCapacityBytes,
           (unsigned int)warmPack.rangeCacheEntries,
           (unsigned int)warmPack.rangeCacheEntryCapacity,
           (unsigned int)warmPack.largeRangeEntries,
           (unsigned int)warmFrame.worldMicros,
           (unsigned int)warmFrame.spriteMicros,
           (unsigned int)warmFrame.hudMicros,
           (unsigned int)warmFrame.presentMicros,
           (unsigned int)warmFrame.totalMicros);
    printf("[LARGECACHE] READY savedReads=%u savedBytes=%u ownerDelta=0 heapStable=yes frame=%08x viewport=%08x hud=%08x shapeData=NULL mediaTexels=NULL large cache stays enabled for MOVE/TURN\n",
           (unsigned int)expectedLargeEntries,
           (unsigned int)(expectedLargeEntries * LARGE_RANGE_BYTES),
           (unsigned int)warmFrame.frameAfterFNV,
           (unsigned int)warmFrame.viewportAfterSpritesFNV,
           (unsigned int)warmFrame.hudBandsAfterFNV);
    probeDone = 1U;
}