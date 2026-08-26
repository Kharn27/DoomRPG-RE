#include <SDL.h>
#include <stdint.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_native_first_frame.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud_direction.h"
#include "esp_native_gameplay_present_gate.h"
#include "esp_native_junction_sprite_renderer.h"
#include "esp_native_plane_renderer.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define HUD_ROWS 20U
#define HUD_BAND_PIXELS (DOOMRPG_LOGICAL_WIDTH * HUD_ROWS)
#define BOTTOM_HUD_Y (DOOMRPG_LOGICAL_HEIGHT - HUD_ROWS)
#define WORLD_Y HUD_ROWS
#define WORLD_ROWS (DOOMRPG_LOGICAL_HEIGHT - (HUD_ROWS * 2U))
#define WORLD_PIXELS (DOOMRPG_LOGICAL_WIDTH * WORLD_ROWS)

typedef struct GameplayFrameScratch_s {
    EspNativeGameplayFrameStats stats;
    EspNativeJunctionSpriteStats sprites;
    EspNativeGameplayHudDirectionStats hud;
    EspNativeFirstFrameState world;
    uint8_t busy;
    uint8_t reserved[3];
} GameplayFrameScratch;

/* Single gameplay service, non-reentrant by contract. The scratch is permanent
 * bounded BSS so repeated MOVE/TURN rendering does not deepen loopTask stack.
 * Pixel payload lives only in the shared framebuffer; the gameplay world route
 * no longer allocates or copies a temporary HUD save. */
static GameplayFrameScratch frameScratch;

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

static uint32_t elapsedMicros(int64_t start) {
    int64_t elapsed = esp_timer_get_time() - start;
    if (elapsed <= 0) return 0U;
    if ((uint64_t)elapsed > UINT32_MAX) return UINT32_MAX;
    return (uint32_t)elapsed;
}

static uint32_t frameFNV(void) {
    const void* framebuffer = Esp32PlatformVideo_framebuffer();
    const size_t bytes = Esp32PlatformVideo_framebufferSizeBytes();
    const size_t expected = (size_t)DOOMRPG_LOGICAL_WIDTH *
                            (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (framebuffer == NULL || bytes != expected) return 0U;
    return fnv1a(framebuffer, (uint32_t)bytes);
}

static uint32_t viewportFNV(void) {
    const uint16_t* framebuffer =
        (const uint16_t*)Esp32PlatformVideo_framebuffer();
    if (framebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() !=
            (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                sizeof(uint16_t)) {
        return 0U;
    }
    return fnv1a(framebuffer + WORLD_Y * DOOMRPG_LOGICAL_WIDTH,
                 WORLD_PIXELS * (uint32_t)sizeof(uint16_t));
}

static uint32_t hudBandsFNV(void) {
    const uint16_t* framebuffer =
        (const uint16_t*)Esp32PlatformVideo_framebuffer();
    uint32_t hash = 2166136261U;
    if (framebuffer == NULL ||
        Esp32PlatformVideo_framebufferSizeBytes() !=
            (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
                sizeof(uint16_t)) {
        return 0U;
    }
    hash = fnv1aUpdate(hash, framebuffer,
                       HUD_BAND_PIXELS * (uint32_t)sizeof(uint16_t));
    return fnv1aUpdate(
        hash,
        framebuffer + BOTTOM_HUD_Y * DOOMRPG_LOGICAL_WIDTH,
        HUD_BAND_PIXELS * (uint32_t)sizeof(uint16_t));
}

/* The historical Junction sprite milestone deliberately required one mode7
 * object and at least one rendered glow because its fixed north-facing pose was
 * proving those families. A runtime cardinal view may legitimately contain no
 * admitted sprite at all, or no visible mode7/glow. If that strict fixed-pose
 * witness is absent, the gameplay compositor accepts the render only when every
 * map sprite is fully classified and every admitted base/glow is accounted for.
 * Any unsupported object, deferred glow, short draw, or renderer scratch
 * mutation remains fail-closed. */
static int spriteViewAccountingComplete(
    const EspNativeJunctionSpriteStats* sprites) {
    uint32_t classified;
    uint32_t basesFinished;
    uint32_t glowsFinished;

    if (sprites == NULL || sprites->objects == 0U ||
        sprites->unsupported != 0U || sprites->glowDeferred != 0U) {
        return 0;
    }

    classified = sprites->hidden + sprites->bspRejected +
                 sprites->bspCandidates;
    basesFinished = sprites->draws + sprites->nearCulled +
                    sprites->clipCulled;
    glowsFinished = sprites->glowDraws + sprites->glowNearCulled +
                    sprites->glowClipCulled;

    return classified == sprites->objects &&
           sprites->mode0Objects + sprites->mode7Objects ==
               sprites->bspCandidates &&
           basesFinished == sprites->bspCandidates &&
           glowsFinished == sprites->glowCompanions;
}

int EspNativeGameplayFrame_renderTurn(
    struct Render_s* renderBase,
    uint8_t angle,
    EspNativeGameplayFrameStats* outStats) {
    Render_t* render = (Render_t*)renderBase;
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativePlaneRenderStats* planes;
    EspNativeGameplayFrameStats* stats = &frameScratch.stats;
    EspNativeJunctionSpriteStats* sprites = &frameScratch.sprites;
    EspNativeGameplayHudDirectionStats* hud = &frameScratch.hud;
    EspNativeFirstFrameState* world = &frameScratch.world;
    uint32_t renderBeforeSpritesFNV;
    uint32_t renderAfterSpritesFNV;
    int64_t totalStart;
    int64_t phaseStart;
    int strictSpriteWitness;
    int ok = 0;

    if (outStats != NULL) memset(outStats, 0, sizeof(*outStats));
    if (frameScratch.busy) return 0;
    if (render == NULL || outStats == NULL || view == NULL ||
        view->active != 1U || view->viewAngle != (int32_t)angle ||
        view->destAngle != (int32_t)angle || (angle & 63U) != 0U ||
        render->framebuffer != Esp32PlatformVideo_framebuffer() ||
        render->screenX != 0 || render->screenY != 20 ||
        render->screenWidth != 160 || render->screenHeight != 80 ||
        EspAssetPack_isOpen() || EspNativeGameplayPresentGate_isArmed()) {
        return 0;
    }

    frameScratch.busy = 1U;
    memset(stats, 0, sizeof(*stats));
    memset(sprites, 0, sizeof(*sprites));
    memset(hud, 0, sizeof(*hud));
    memset(world, 0, sizeof(*world));
    totalStart = esp_timer_get_time();

    stats->frameBeforeFNV = frameFNV();
    stats->viewportBeforeFNV = viewportFNV();
    stats->hudBandsBeforeFNV = hudBandsFNV();
    if (stats->frameBeforeFNV == 0U || stats->viewportBeforeFNV == 0U ||
        stats->hudBandsBeforeFNV == 0U) {
        goto done;
    }

    phaseStart = esp_timer_get_time();
    EspNativePlaneRenderer_reset();
    if (EspNativeFirstFrame_renderGameplayViewport(render, view, world) !=
        ESP_NATIVE_FIRST_FRAME_OK) {
        goto done;
    }
    stats->worldMicros = elapsedMicros(phaseStart);
    stats->worldRouteNoPresent = 1U;

    planes = EspNativePlaneRenderer_view();
    if (planes == NULL || world->presented != 0U || world->rendered != 1U) {
        goto done;
    }
    stats->worldFrameFNV = world->frameAfterFNV;
    stats->viewportAfterWorldFNV = viewportFNV();
    stats->wallDraws = world->wallDraws;
    stats->wallPixels = world->pixelsDrawn;
    stats->planePixels = planes->pixelsRendered;
    if (hudBandsFNV() != stats->hudBandsBeforeFNV) goto done;

    phaseStart = esp_timer_get_time();
    renderBeforeSpritesFNV = fnv1a(render, (uint32_t)sizeof(*render));
    strictSpriteWitness = EspNativeJunctionSprite_render(render, sprites);
    renderAfterSpritesFNV = fnv1a(render, (uint32_t)sizeof(*render));
    stats->spriteMicros = elapsedMicros(phaseStart);
    if (!strictSpriteWitness) {
        if (!spriteViewAccountingComplete(sprites) ||
            renderAfterSpritesFNV != renderBeforeSpritesFNV ||
            EspAssetPack_isOpen()) {
            goto done;
        }
        printf("[TURNFRAME] SPRITES viewComplete=yes strictFixedPoseWitness=no objects=%u candidates=%u modes=%u/%u base=%u+%u+%u glows=%u:%u+%u+%u unsupported=%u deferred=%u renderStable=yes\n",
               (unsigned int)sprites->objects,
               (unsigned int)sprites->bspCandidates,
               (unsigned int)sprites->mode0Objects,
               (unsigned int)sprites->mode7Objects,
               (unsigned int)sprites->draws,
               (unsigned int)sprites->nearCulled,
               (unsigned int)sprites->clipCulled,
               (unsigned int)sprites->glowCompanions,
               (unsigned int)sprites->glowDraws,
               (unsigned int)sprites->glowNearCulled,
               (unsigned int)sprites->glowClipCulled,
               (unsigned int)sprites->unsupported,
               (unsigned int)sprites->glowDeferred);
    }
    else if (renderAfterSpritesFNV != renderBeforeSpritesFNV) {
        goto done;
    }

    stats->spriteDraws = sprites->draws;
    stats->spritePixels = sprites->pixelsDrawn;
    stats->glowDraws = sprites->glowDraws;
    stats->glowPixels = sprites->glowPixels;
    stats->spritePackReads = sprites->packReads;
    stats->viewportAfterSpritesFNV = viewportFNV();

    /* The viewport-only world and sprite routes must leave both HUD bands
     * bit-exact. This replaces the old 12.8 KiB save/restore bridge. */
    stats->temporaryHudBytes = 0U;
    stats->hudBandsRestoredFNV = hudBandsFNV();
    if (stats->hudBandsRestoredFNV != stats->hudBandsBeforeFNV) goto done;

    phaseStart = esp_timer_get_time();
    if (!EspNativeGameplayHudDirection_render(angle, hud)) goto done;
    stats->hudMicros = elapsedMicros(phaseStart);
    stats->hudPackReads = hud->packReads;
    stats->hudPixels = hud->pixelsWritten;
    stats->hudBandsAfterFNV = hudBandsFNV();
    stats->angle = angle;
    stats->frameAfterFNV = frameFNV();
    if (stats->frameAfterFNV == 0U || stats->viewportAfterSpritesFNV == 0U ||
        stats->hudBandsAfterFNV == 0U) {
        goto done;
    }

    phaseStart = esp_timer_get_time();
    if (!Esp32PlatformVideo_present()) goto done;
    stats->presentMicros = elapsedMicros(phaseStart);
    stats->finalPresented = 1U;
    stats->active = 1U;
    ok = 1;

done:
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    stats->totalMicros = elapsedMicros(totalStart);
    if (!ok) stats->active = 0U;
    *outStats = *stats;
    frameScratch.busy = 0U;
    return ok;
}
