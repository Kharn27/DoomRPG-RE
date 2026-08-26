#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_heap_caps.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_native_gameplay_frame.h"
#include "esp_player_view_state.h"
#include "native_junction_gameplay_render_hotpath_probe.h"
#include "native_junction_move_collision_probe.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define EXPECTED_FRAME_FNV 0xba3e5182U
#define EXPECTED_VIEWPORT_FNV 0x9206eb24U
#define EXPECTED_HUD_BANDS_FNV 0x6c2aa46fU
#define WORLD_Y 20U
#define WORLD_ROWS 80U
#define HUD_ROWS 20U
#define BOTTOM_HUD_Y 100U

static uint8_t probeDone;
static uint8_t probeFailed;
static EspNativeGameplayFrameStats frameStats;
static EspPlayerViewState viewBefore;

static uint32_t fnv1aUpdate(uint32_t hash, const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t fnv1a(const void* data, uint32_t bytes) {
    return fnv1aUpdate(2166136261U, data, bytes);
}

static uint32_t frameFNV(void) {
    const void* framebuffer = Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    const size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                            DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (framebuffer == NULL || bytes != expected) return 0U;
    return fnv1a(framebuffer, (uint32_t)bytes);
}

static uint32_t viewportFNV(void) {
    const uint16_t* framebuffer =
        (const uint16_t*)Esp32PlatformVideo_framebuffer();
    if (framebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() !=
            (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                sizeof(uint16_t)) return 0U;
    return fnv1a(framebuffer + WORLD_Y * DOOMRPG_LOGICAL_WIDTH,
                 DOOMRPG_LOGICAL_WIDTH * WORLD_ROWS * sizeof(uint16_t));
}

static uint32_t hudBandsFNV(void) {
    const uint16_t* framebuffer =
        (const uint16_t*)Esp32PlatformVideo_framebuffer();
    uint32_t hash = 2166136261U;
    if (framebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() !=
            (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                sizeof(uint16_t)) return 0U;
    hash = fnv1aUpdate(hash, framebuffer,
                       DOOMRPG_LOGICAL_WIDTH * HUD_ROWS * sizeof(uint16_t));
    return fnv1aUpdate(hash,
                       framebuffer + BOTTOM_HUD_Y * DOOMRPG_LOGICAL_WIDTH,
                       DOOMRPG_LOGICAL_WIDTH * HUD_ROWS * sizeof(uint16_t));
}

static uint32_t heap8(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

static uint32_t largest8(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static void failProbe(const char* reason) {
    printf("[HOTPATHPROBE] FAILED %s frame=%08x viewport=%08x hud=%08x pack=%d\n",
           reason,
           (unsigned int)frameFNV(),
           (unsigned int)viewportFNV(),
           (unsigned int)hudBandsFNV(),
           EspAssetPack_isOpen());
    probeFailed = 1U;
    probeDone = 1U;
}

void Esp32JunctionGameplayRenderHotpathProbe_reset(void) {
    probeDone = 0U;
    probeFailed = 0U;
    memset(&frameStats, 0, sizeof(frameStats));
    memset(&viewBefore, 0, sizeof(viewBefore));
}

int Esp32JunctionGameplayRenderHotpathProbe_isDone(void) {
    return probeDone != 0U && probeFailed == 0U;
}

void Esp32JunctionGameplayRenderHotpathProbe_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspPlayerViewState* view;
    uint32_t heapBefore;
    uint32_t heapAfter;
    uint32_t largestBefore;
    uint32_t largestAfter;
    uint32_t frameBefore;
    uint32_t viewportBefore;
    uint32_t hudBefore;

    if (probeDone || probeFailed ||
        !Esp32JunctionMoveCollisionProbe_isActive()) return;

    printf("\n=== Doom RPG ESP32-native gameplay renderer viewport hot path ===\n");
    printf("[HOTPATHPROBE] CONTRACT re-render the already displayed canonical North gameplay pose through the new gameplay-only world route. The result must be bit-exact while the world phase touches only 160x80@0,20, performs no intermediate present, allocates no 12.8 KiB HUD save, preserves both HUD bands in place, leaves native/legacy gameplay state untouched, and still issues exactly one final complete-frame present.\n");

    view = EspPlayerView_view();
    if (doomRpg == NULL || doomRpg->render == NULL || view == NULL ||
        view->active != 1U || view->viewX != 992 || view->viewY != 1888 ||
        view->viewAngle != 64 || view->destAngle != 64 ||
        doomRpg->render->shapeData != NULL || doomRpg->render->mediaTexels != NULL ||
        EspAssetPack_isOpen()) {
        failProbe("activation boundary");
        return;
    }

    frameBefore = frameFNV();
    viewportBefore = viewportFNV();
    hudBefore = hudBandsFNV();
    if (frameBefore != EXPECTED_FRAME_FNV ||
        viewportBefore != EXPECTED_VIEWPORT_FNV ||
        hudBefore != EXPECTED_HUD_BANDS_FNV) {
        failProbe("canonical predecessor");
        return;
    }

    viewBefore = *view;
    heapBefore = heap8();
    largestBefore = largest8();
    memset(&frameStats, 0, sizeof(frameStats));

    if (!EspNativeGameplayFrame_renderTurn(doomRpg->render, 64U, &frameStats)) {
        failProbe("viewport gameplay render");
        return;
    }

    heapAfter = heap8();
    largestAfter = largest8();
    view = EspPlayerView_view();
    if (view == NULL || memcmp(view, &viewBefore, sizeof(viewBefore)) != 0 ||
        frameFNV() != EXPECTED_FRAME_FNV ||
        viewportFNV() != EXPECTED_VIEWPORT_FNV ||
        hudBandsFNV() != EXPECTED_HUD_BANDS_FNV ||
        frameStats.frameBeforeFNV != EXPECTED_FRAME_FNV ||
        frameStats.frameAfterFNV != EXPECTED_FRAME_FNV ||
        frameStats.viewportBeforeFNV != EXPECTED_VIEWPORT_FNV ||
        frameStats.viewportAfterSpritesFNV != EXPECTED_VIEWPORT_FNV ||
        frameStats.hudBandsBeforeFNV != EXPECTED_HUD_BANDS_FNV ||
        frameStats.hudBandsRestoredFNV != EXPECTED_HUD_BANDS_FNV ||
        frameStats.hudBandsAfterFNV != EXPECTED_HUD_BANDS_FNV ||
        frameStats.temporaryHudBytes != 0U ||
        frameStats.worldRouteNoPresent != 1U ||
        frameStats.finalPresented != 1U || frameStats.active != 1U ||
        heapAfter != heapBefore || largestAfter != largestBefore ||
        EspAssetPack_isOpen() || doomRpg->render->shapeData != NULL ||
        doomRpg->render->mediaTexels != NULL) {
        failProbe("postcondition");
        return;
    }

    printf("[HOTPATH] READY frameStatsBytes=%u frame=%08x viewport=%08x hud=%08x tempHud=%u routeNoPresent=%u finalPresent=%u timeUs=world:%u sprite:%u hud:%u present:%u total:%u heap=%u->%u largest=%u->%u exact=yes\n",
           (unsigned int)sizeof(frameStats),
           (unsigned int)frameStats.frameAfterFNV,
           (unsigned int)frameStats.viewportAfterSpritesFNV,
           (unsigned int)frameStats.hudBandsAfterFNV,
           (unsigned int)frameStats.temporaryHudBytes,
           (unsigned int)frameStats.worldRouteNoPresent,
           (unsigned int)frameStats.finalPresented,
           (unsigned int)frameStats.worldMicros,
           (unsigned int)frameStats.spriteMicros,
           (unsigned int)frameStats.hudMicros,
           (unsigned int)frameStats.presentMicros,
           (unsigned int)frameStats.totalMicros,
           (unsigned int)heapBefore, (unsigned int)heapAfter,
           (unsigned int)largestBefore, (unsigned int)largestAfter);
    printf("[HOTPATH] PARK MOVE/TURN interactive probes remain active; compare their timeUs breakdown with prior perceived latency. No cache-persistence optimization is enabled in this milestone.\n");
    probeDone = 1U;
}
