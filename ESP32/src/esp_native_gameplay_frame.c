#include <SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_asset_pack.h"
#include "esp_native_first_frame.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud_direction.h"
#include "esp_native_junction_sprite_renderer.h"
#include "esp_native_plane_renderer.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define HUD_ROWS 20U
#define HUD_BAND_PIXELS (DOOMRPG_LOGICAL_WIDTH * HUD_ROWS)
#define HUD_SAVED_PIXELS (HUD_BAND_PIXELS * 2U)
#define HUD_SAVED_BYTES (HUD_SAVED_PIXELS * sizeof(uint16_t))
#define BOTTOM_HUD_Y (DOOMRPG_LOGICAL_HEIGHT - HUD_ROWS)

static uint32_t fnv1a(const void* data, uint32_t bytes) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (p == NULL && bytes != 0U) return 0U;
    for (i = 0U; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t frameFNV(void) {
    const void* framebuffer = Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    const size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                            (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (framebuffer == NULL || bytes != expected) return 0U;
    return fnv1a(framebuffer, (uint32_t)bytes);
}

int EspNativeGameplayFrame_renderTurn(
    struct Render_s* renderBase,
    uint8_t angle,
    EspNativeGameplayFrameStats* outStats) {
    Render_t* render = (Render_t*)renderBase;
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeFirstFrameState* world;
    const EspNativePlaneRenderStats* planes;
    EspNativeJunctionSpriteStats sprites;
    EspNativeGameplayHudDirectionStats hud;
    EspNativeGameplayFrameStats stats;
    uint16_t* framebuffer;
    uint16_t* savedHud = NULL;
    int ok = 0;

    memset(&stats, 0, sizeof(stats));
    memset(&sprites, 0, sizeof(sprites));
    memset(&hud, 0, sizeof(hud));
    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));
    if (render == NULL || outStats == NULL || view == NULL ||
        view->active != 1U || view->viewAngle != (int32_t)angle ||
        view->destAngle != (int32_t)angle || (angle & 63U) != 0U ||
        render->framebuffer != Esp32PlatformVideo_framebuffer() ||
        render->screenX != 0 || render->screenY != 20 ||
        render->screenWidth != 160 || render->screenHeight != 80 ||
        EspAssetPack_isOpen()) {
        return 0;
    }

    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    stats.frameBeforeFNV = frameFNV();
    if (framebuffer == NULL || stats.frameBeforeFNV == 0U) return 0;

    savedHud = (uint16_t*)malloc(HUD_SAVED_BYTES);
    if (savedHud == NULL) return 0;
    stats.temporaryHudBytes = (uint32_t)HUD_SAVED_BYTES;
    memcpy(savedHud, framebuffer, HUD_BAND_PIXELS * sizeof(uint16_t));
    memcpy(savedHud + HUD_BAND_PIXELS,
           framebuffer + BOTTOM_HUD_Y * DOOMRPG_LOGICAL_WIDTH,
           HUD_BAND_PIXELS * sizeof(uint16_t));

    EspNativeFirstFrame_reset();
    EspNativePlaneRenderer_reset();
    if (EspNativeFirstFrame_route(render, view) != ESP_NATIVE_FIRST_FRAME_OK) {
        goto done;
    }
    stats.worldPresented = 1U;
    world = EspNativeFirstFrame_view();
    planes = EspNativePlaneRenderer_view();
    if (world == NULL || planes == NULL) goto done;
    stats.worldFrameFNV = world->frameAfterFNV;
    stats.wallDraws = world->wallDraws;
    stats.wallPixels = world->pixelsDrawn;
    stats.planePixels = planes->pixelsRendered;

    if (!EspNativeJunctionSprite_render(render, &sprites)) goto done;
    stats.spriteDraws = sprites.draws;
    stats.spritePixels = sprites.pixelsDrawn;
    stats.glowDraws = sprites.glowDraws;
    stats.glowPixels = sprites.glowPixels;
    stats.spritePackReads = sprites.packReads;

    memcpy(framebuffer, savedHud, HUD_BAND_PIXELS * sizeof(uint16_t));
    memcpy(framebuffer + BOTTOM_HUD_Y * DOOMRPG_LOGICAL_WIDTH,
           savedHud + HUD_BAND_PIXELS,
           HUD_BAND_PIXELS * sizeof(uint16_t));

    if (!EspNativeGameplayHudDirection_render(angle, &hud)) goto done;
    stats.hudPackReads = hud.packReads;
    stats.hudPixels = hud.pixelsWritten;
    stats.angle = angle;
    stats.frameAfterFNV = frameFNV();
    if (stats.frameAfterFNV == 0U) goto done;

    if (!Esp32PlatformVideo_present()) goto done;
    stats.finalPresented = 1U;
    stats.active = 1U;
    ok = 1;

done:
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    free(savedHud);
    if (!ok) stats.active = 0U;
    *outStats = stats;
    return ok;
}
