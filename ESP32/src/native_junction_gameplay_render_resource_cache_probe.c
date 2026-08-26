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
#include "native_junction_gameplay_render_hotpath_probe.h"
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
#define EXPECTED_CACHE_BYTES 16384U
#define EXPECTED_CACHE_ENTRIES 256U

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

static void failProbe(const char* reason) {
    EspAssetPackResidentStats stats;
    memset(&stats, 0, sizeof(stats));
    EspAssetPack_residentGetStats(&stats);
    printf("[RENDERCACHEPROBE] FAILED %s frame=%08x viewport=%08x hud=%08x pack=%d resident=%u reads=%u hits=%u cache=%u/%uB\n",
           reason, (unsigned int)frameFNV(), (unsigned int)viewportFNV(),
           (unsigned int)hudFNV(), EspAssetPack_isOpen(),
           (unsigned int)stats.ready, (unsigned int)stats.physicalReads,
           (unsigned int)stats.rangeCacheHits,
           (unsigned int)stats.rangeCacheBytesUsed,
           (unsigned int)stats.rangeCacheCapacityBytes);
    probeFailed = 1U;
    probeDone = 1U;
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

void Esp32JunctionGameplayRenderResourceCacheProbe_reset(void) {
    if (!EspAssetPack_isOpen()) (void)EspAssetPack_residentEnd();
    probeDone = 0U;
    probeFailed = 0U;
}

int Esp32JunctionGameplayRenderResourceCacheProbe_isDone(void) {
    return probeDone != 0U && probeFailed == 0U;
}

void Esp32JunctionGameplayRenderResourceCacheProbe_service(
    struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view;
    EspPlayerViewState viewBefore;
    EspNativeGameplayFrameStats coldFrame;
    EspNativeGameplayFrameStats warmFrame;
    EspAssetPackResidentStats owner;
    EspAssetPackResidentStats coldPack;
    EspAssetPackResidentStats warmPack;
    uint32_t heapBefore;
    uint32_t heapResident;
    uint32_t largestResident;

    if (probeDone || probeFailed ||
        !Esp32JunctionGameplayRenderHotpathProbe_isDone()) return;

    printf("\n=== Doom RPG ESP32-native persistent gameplay render resource cache ===\n");
    printf("[RENDERCACHEPROBE] CONTRACT one validated resident default PAK; exact immutable ranges <=1024B cached in a bounded owner; large world texels stay PAK-backed; cold/warm North remain bit-exact, logical PAK closes after every phase, shapeData/mediaTexels stay NULL, heap stays stable after owner creation, and warm physical reads must be lower than cold.\n");

    view = EspPlayerView_view();
    if (doomRpg == NULL || doomRpg->render == NULL || view == NULL ||
        view->active != 1U || view->viewX != 992 || view->viewY != 1888 ||
        view->viewAngle != 64 || view->destAngle != 64 ||
        frameFNV() != EXPECTED_FRAME_FNV ||
        viewportFNV() != EXPECTED_VIEWPORT_FNV ||
        hudFNV() != EXPECTED_HUD_BANDS_FNV || EspAssetPack_isOpen() ||
        EspAssetPack_isResident() || doomRpg->render->shapeData != NULL ||
        doomRpg->render->mediaTexels != NULL) {
        failProbe("activation boundary");
        return;
    }

    viewBefore = *view;
    heapBefore = heap8();
    if (!EspAssetPack_residentBegin() || EspAssetPack_isOpen() ||
        !EspAssetPack_isResident()) {
        failProbe("resident begin");
        return;
    }
    heapResident = heap8();
    largestResident = largest8();
    memset(&owner, 0, sizeof(owner));
    EspAssetPack_residentGetStats(&owner);
    if (owner.enabled != 1U || owner.ready != 1U ||
        owner.physicalOpens != 1U || owner.validationPasses != 1U ||
        owner.ownerBytes == 0U ||
        owner.rangeCacheCapacityBytes != EXPECTED_CACHE_BYTES ||
        owner.rangeCacheEntryCapacity != EXPECTED_CACHE_ENTRIES) {
        failProbe("resident owner contract");
        return;
    }

    memset(&coldFrame, 0, sizeof(coldFrame));
    EspAssetPack_residentResetStats();
    if (!EspNativeGameplayFrame_renderTurn(doomRpg->render, 64U, &coldFrame)) {
        failProbe("cold render");
        return;
    }
    memset(&coldPack, 0, sizeof(coldPack));
    EspAssetPack_residentGetStats(&coldPack);
    view = EspPlayerView_view();
    if (!exactFrame(&coldFrame) || view == NULL ||
        memcmp(view, &viewBefore, sizeof(viewBefore)) != 0 ||
        EspAssetPack_isOpen() || !EspAssetPack_isResident() ||
        coldPack.physicalOpens != 0U || coldPack.validationPasses != 0U ||
        coldPack.residentReuses < 3U || coldPack.physicalReads == 0U ||
        coldPack.rangeCacheStores == 0U || heap8() != heapResident ||
        largest8() != largestResident || doomRpg->render->shapeData != NULL ||
        doomRpg->render->mediaTexels != NULL) {
        failProbe("cold postcondition");
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
        warmPack.physicalOpens != 0U || warmPack.validationPasses != 0U ||
        warmPack.residentReuses < 3U || warmPack.rangeCacheHits == 0U ||
        warmPack.entryCacheHits == 0U ||
        warmPack.physicalReads >= coldPack.physicalReads ||
        heap8() != heapResident || largest8() != largestResident ||
        doomRpg->render->shapeData != NULL ||
        doomRpg->render->mediaTexels != NULL) {
        failProbe("warm postcondition");
        return;
    }

    printf("[RENDERCACHE] OWNER struct=%uB heap=%u->%u cost=%u largest=%u cache=%u/%uB entries=%u/%u physicalResident=yes logicalPackClosed=yes\n",
           (unsigned int)warmPack.ownerBytes,
           (unsigned int)heapBefore, (unsigned int)heapResident,
           (unsigned int)(heapBefore >= heapResident ? heapBefore - heapResident : 0U),
           (unsigned int)largestResident,
           (unsigned int)warmPack.rangeCacheBytesUsed,
           (unsigned int)warmPack.rangeCacheCapacityBytes,
           (unsigned int)warmPack.rangeCacheEntries,
           (unsigned int)warmPack.rangeCacheEntryCapacity);
    printf("[RENDERCACHE] COLD leases=%u reuse=%u physicalOpen=%u validate=%u sdReads=%u sdBytes=%u entry=%uH/%uM range=%uH/%uM/%uS/%uB timeUs=%u/%u/%u/%u total=%u exact=yes\n",
           (unsigned int)coldPack.logicalOpens, (unsigned int)coldPack.residentReuses,
           (unsigned int)coldPack.physicalOpens, (unsigned int)coldPack.validationPasses,
           (unsigned int)coldPack.physicalReads, (unsigned int)coldPack.physicalBytes,
           (unsigned int)coldPack.entryCacheHits, (unsigned int)coldPack.entryCacheMisses,
           (unsigned int)coldPack.rangeCacheHits, (unsigned int)coldPack.rangeCacheMisses,
           (unsigned int)coldPack.rangeCacheStores, (unsigned int)coldPack.rangeCacheBypasses,
           (unsigned int)coldFrame.worldMicros, (unsigned int)coldFrame.spriteMicros,
           (unsigned int)coldFrame.hudMicros, (unsigned int)coldFrame.presentMicros,
           (unsigned int)coldFrame.totalMicros);
    printf("[RENDERCACHE] WARM leases=%u reuse=%u physicalOpen=%u validate=%u sdReads=%u sdBytes=%u entry=%uH/%uM range=%uH/%uM/%uS/%uB timeUs=%u/%u/%u/%u total=%u exact=yes\n",
           (unsigned int)warmPack.logicalOpens, (unsigned int)warmPack.residentReuses,
           (unsigned int)warmPack.physicalOpens, (unsigned int)warmPack.validationPasses,
           (unsigned int)warmPack.physicalReads, (unsigned int)warmPack.physicalBytes,
           (unsigned int)warmPack.entryCacheHits, (unsigned int)warmPack.entryCacheMisses,
           (unsigned int)warmPack.rangeCacheHits, (unsigned int)warmPack.rangeCacheMisses,
           (unsigned int)warmPack.rangeCacheStores, (unsigned int)warmPack.rangeCacheBypasses,
           (unsigned int)warmFrame.worldMicros, (unsigned int)warmFrame.spriteMicros,
           (unsigned int)warmFrame.hudMicros, (unsigned int)warmFrame.presentMicros,
           (unsigned int)warmFrame.totalMicros);
    printf("[RENDERCACHE] READY coldReads=%u warmReads=%u saved=%u cacheHits=%u frame=%08x viewport=%08x hud=%08x heapStable=yes shapeData=NULL mediaTexels=NULL owner stays resident for MOVE/TURN\n",
           (unsigned int)coldPack.physicalReads,
           (unsigned int)warmPack.physicalReads,
           (unsigned int)(coldPack.physicalReads - warmPack.physicalReads),
           (unsigned int)warmPack.rangeCacheHits,
           (unsigned int)warmFrame.frameAfterFNV,
           (unsigned int)warmFrame.viewportAfterSpritesFNV,
           (unsigned int)warmFrame.hudBandsAfterFNV);
    probeDone = 1U;
}
