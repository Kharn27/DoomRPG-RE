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
#include "esp_native_gameplay_present_gate.h"
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
#define WORLD_Y HUD_ROWS
#define WORLD_ROWS (DOOMRPG_LOGICAL_HEIGHT - (HUD_ROWS * 2U))
#define WORLD_PIXELS (DOOMRPG_LOGICAL_WIDTH * WORLD_ROWS)

typedef struct GameplayFrameScratch_s {
    EspNativeGameplayFrameStats stats;
    EspNativeJunctionSpriteStats sprites;
    EspNativeGameplayHudDirectionStats hud;
    uint8_t busy;
    uint8_t reserved[3];
} GameplayFrameScratch;

/* Single gameplay service, non-reentrant by contract. The scratch is permanent
 * bounded BSS so repeated TURN rendering does not deepen loopTask stack with
 * the large sprite/stat aggregates. Pixel payload still lives only in the
 * shared framebuffer or the bounded temporary HUD save below. */
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
 * visible mode7/glow at all. If that strict fixed-pose witness is absent, the
 * gameplay compositor accepts the render only when every admitted base/glow
 * object is fully accounted for. Any unsupported object, deferred glow, short
 * draw, or renderer scratch mutation remains fail-closed. */
static int spriteViewAccountingComplete(
    const EspNativeJunctionSpriteStats* sprites) {
    uint32_t classified;
    uint32_t basesFinished;
    uint32_t glowsFinished;

    if (sprites == NULL || sprites->objects == 0U ||
        sprites->bspCandidates == 0U || sprites->unsupported != 0U ||
        sprites->glowDeferred != 0U) {
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
    const EspNativeFirstFrameState* world;
    const EspNativePlaneRenderStats* planes;
    EspNativeGameplayFrameStats* stats = &frameScratch.stats;
    EspNativeJunctionSpriteStats* sprites = &frameScratch.sprites;
    EspNativeGameplayHudDirectionStats* hud = &frameScratch.hud;
    uint16_t* framebuffer;
    uint16_t* savedHud = NULL;
    uint32_t renderBeforeSpritesFNV;
    uint32_t renderAfterSpritesFNV;
    unsigned int suppressedBefore;
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

    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    stats->frameBeforeFNV = frameFNV();
    stats->viewportBeforeFNV = viewportFNV();
    stats->hudBandsBeforeFNV = hudBandsFNV();
    if (framebuffer == NULL || stats->frameBeforeFNV == 0U ||
        stats->viewportBeforeFNV == 0U || stats->hudBandsBeforeFNV == 0U) {
        goto done;
    }

    savedHud = (uint16_t*)malloc(HUD_SAVED_BYTES);
    if (savedHud == NULL) goto done;
    stats->temporaryHudBytes = (uint32_t)HUD_SAVED_BYTES;
    memcpy(savedHud, framebuffer, HUD_BAND_PIXELS * sizeof(uint16_t));
    memcpy(savedHud + HUD_BAND_PIXELS,
           framebuffer + BOTTOM_HUD_Y * DOOMRPG_LOGICAL_WIDTH,
           HUD_BAND_PIXELS * sizeof(uint16_t));

    EspNativeFirstFrame_reset();
    EspNativePlaneRenderer_reset();
    suppressedBefore = EspNativeGameplayPresentGate_suppressedCount();
    if (!EspNativeGameplayPresentGate_armOne()) goto done;
    if (EspNativeFirstFrame_route(render, view) != ESP_NATIVE_FIRST_FRAME_OK) {
        EspNativeGameplayPresentGate_cancel();
        goto done;
    }
    if (EspNativeGameplayPresentGate_isArmed() ||
        EspNativeGameplayPresentGate_suppressedCount() != suppressedBefore + 1U) {
        EspNativeGameplayPresentGate_cancel();
        goto done;
    }
    stats->intermediatePresentSuppressed = 1U;

    world = EspNativeFirstFrame_view();
    planes = EspNativePlaneRenderer_view();
    if (world == NULL || planes == NULL) goto done;
    stats->worldFrameFNV = world->frameAfterFNV;
    stats->viewportAfterWorldFNV = viewportFNV();
    stats->wallDraws = world->wallDraws;
    stats->wallPixels = world->pixelsDrawn;
    stats->planePixels = planes->pixelsRendered;

    renderBeforeSpritesFNV = fnv1a(render, (uint32_t)sizeof(*render));
    strictSpriteWitness = EspNativeJunctionSprite_render(render, sprites);
    renderAfterSpritesFNV = fnv1a(render, (uint32_t)sizeof(*render));
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

    memcpy(framebuffer, savedHud, HUD_BAND_PIXELS * sizeof(uint16_t));
    memcpy(framebuffer + BOTTOM_HUD_Y * DOOMRPG_LOGICAL_WIDTH,
           savedHud + HUD_BAND_PIXELS,
           HUD_BAND_PIXELS * sizeof(uint16_t));
    stats->hudBandsRestoredFNV = hudBandsFNV();
    if (stats->hudBandsRestoredFNV != stats->hudBandsBeforeFNV) goto done;

    if (!EspNativeGameplayHudDirection_render(angle, hud)) goto done;
    stats->hudPackReads = hud->packReads;
    stats->hudPixels = hud->pixelsWritten;
    stats->hudBandsAfterFNV = hudBandsFNV();
    stats->angle = angle;
    stats->frameAfterFNV = frameFNV();
    if (stats->frameAfterFNV == 0U || stats->viewportAfterSpritesFNV == 0U ||
        stats->hudBandsAfterFNV == 0U) {
        goto done;
    }

    if (!Esp32PlatformVideo_present()) goto done;
    stats->finalPresented = 1U;
    stats->active = 1U;
    ok = 1;

done:
    if (EspNativeGameplayPresentGate_isArmed()) {
        EspNativeGameplayPresentGate_cancel();
    }
    if (EspAssetPack_isOpen()) EspAssetPack_close();
    free(savedHud);
    if (!ok) stats->active = 0U;
    *outStats = *stats;
    frameScratch.busy = 0U;
    return ok;
}
