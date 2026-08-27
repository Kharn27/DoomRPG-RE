#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include <esp_timer.h>

#include "esp_asset_pack.h"
#include "esp_native_first_frame.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud_direction.h"
#include "esp_native_gameplay_present_gate.h"
#include "esp_native_plane_renderer.h"
#include "esp_native_sprite_renderer.h"
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
    EspNativeSpriteStats sprites;
    EspNativeGameplayHudDirectionStats hud;
    EspNativeFirstFrameState world;
    uint8_t busy;
    uint8_t reserved[3];
} GameplayFrameScratch;

/* Single gameplay service, non-reentrant by contract. The scratch is permanent
 * bounded BSS so repeated MOVE/TURN rendering does not deepen loopTask stack.
 * Pixel payload lives only in the shared framebuffer; the gameplay world route
 * no longer allocates or copies a temporary HUD save.
 *
 * Storage/cache activation is deliberately NOT owned here. A frame compositor
 * must be deterministic whether the PAK is in normal or resident lease mode;
 * the generic gameplay session owns the proven cold/warm/large-cache lifecycle.
 */
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

/* The historical fixed Junction pose deliberately required one mode7 object
 * and a glow. A normal resident-map view may contain neither. Production accepts
 * a non-strict result only when every map sprite is classified and every admitted
 * base/glow is fully accounted for. Unsupported/deferred work remains fail-closed.
 */
static int spriteViewAccountingComplete(const EspNativeSpriteStats* sprites) {
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
    EspNativeSpriteStats* sprites = &frameScratch.sprites;
    EspNativeGameplayHudDirectionStats* hud = &frameScratch.hud;
    EspNativeFirstFrameState* world = &frameScratch.world;
    uint32_t renderBeforeSpritesFNV;
    uint32_t renderAfterSpritesFNV;
    uint32_t hudAfterWorldFNV;
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
        printf("[TURNFRAME] DIAG fail=PRE_WORLD_FNV frame=%08x viewport=%08x hud=%08x\n",
               (unsigned int)stats->frameBeforeFNV,
               (unsigned int)stats->viewportBeforeFNV,
               (unsigned int)stats->hudBandsBeforeFNV);
        goto done;
    }

    phaseStart = esp_timer_get_time();
    EspNativePlaneRenderer_reset();
    if (EspNativeFirstFrame_renderGameplayViewport(render, view, world) !=
        ESP_NATIVE_FIRST_FRAME_OK) {
        printf("[TURNFRAME] DIAG fail=WORLD_RENDER player=%d,%d angle=%d\n",
               (int)view->viewX,
               (int)view->viewY,
               (int)view->viewAngle);
        goto done;
    }
    stats->worldMicros = elapsedMicros(phaseStart);
    stats->worldRouteNoPresent = 1U;

    stats->worldFrameFNV = world->frameAfterFNV;
    stats->viewportAfterWorldFNV = viewportFNV();
    stats->wallDraws = world->wallDraws;
    stats->wallPixels = world->pixelsDrawn;
    hudAfterWorldFNV = hudBandsFNV();
    planes = EspNativePlaneRenderer_view();

    if (planes == NULL || world->presented != 0U || world->rendered != 1U) {
        printf("[TURNFRAME] DIAG fail=WORLD_POST planes=%s rendered=%u presented=%u\n",
               planes != NULL ? "yes" : "no",
               (unsigned int)world->rendered,
               (unsigned int)world->presented);
        goto done;
    }
    stats->planePixels = planes->pixelsRendered;
    if (hudAfterWorldFNV != stats->hudBandsBeforeFNV) {
        printf("[TURNFRAME] DIAG fail=HUD_AFTER_WORLD before=%08x after=%08x\n",
               (unsigned int)stats->hudBandsBeforeFNV,
               (unsigned int)hudAfterWorldFNV);
        goto done;
    }

    phaseStart = esp_timer_get_time();
    renderBeforeSpritesFNV = fnv1a(render, (uint32_t)sizeof(*render));
    strictSpriteWitness = EspNativeSpriteRenderer_render(render, sprites);
    renderAfterSpritesFNV = fnv1a(render, (uint32_t)sizeof(*render));
    stats->spriteMicros = elapsedMicros(phaseStart);
    if (!strictSpriteWitness) {
        if (!spriteViewAccountingComplete(sprites) ||
            renderAfterSpritesFNV != renderBeforeSpritesFNV ||
            EspAssetPack_isOpen()) {
            printf("[TURNFRAME] DIAG fail=SPRITES strict=no complete=%s renderStable=%s pack=%u objects=%u hidden=%u rejected=%u candidates=%u unsupported=%u modes=%u/%u base=%u+%u+%u glows=%u:%u+%u+%u deferred=%u depth=%u/%u/%u/%u/%u/%u order=%08x reads=%u\n",
                   spriteViewAccountingComplete(sprites) ? "yes" : "no",
                   renderAfterSpritesFNV == renderBeforeSpritesFNV ? "yes" : "no",
                   (unsigned int)EspAssetPack_isOpen(),
                   (unsigned int)sprites->objects,
                   (unsigned int)sprites->hidden,
                   (unsigned int)sprites->bspRejected,
                   (unsigned int)sprites->bspCandidates,
                   (unsigned int)sprites->unsupported,
                   (unsigned int)sprites->mode0Objects,
                   (unsigned int)sprites->mode7Objects,
                   (unsigned int)sprites->draws,
                   (unsigned int)sprites->nearCulled,
                   (unsigned int)sprites->clipCulled,
                   (unsigned int)sprites->glowCompanions,
                   (unsigned int)sprites->glowDraws,
                   (unsigned int)sprites->glowNearCulled,
                   (unsigned int)sprites->glowClipCulled,
                   (unsigned int)sprites->glowDeferred,
                   (unsigned int)sprites->depthNodes,
                   (unsigned int)sprites->depthLeaves,
                   (unsigned int)sprites->depthNodeCulled,
                   (unsigned int)sprites->depthLines,
                   (unsigned int)sprites->depthBackfaceCulled,
                   (unsigned int)sprites->depthClipCulled,
                   (unsigned int)sprites->orderFNV1a,
                   (unsigned int)sprites->packReads);
            goto done;
        }
    }
    else if (renderAfterSpritesFNV != renderBeforeSpritesFNV) {
        printf("[TURNFRAME] DIAG fail=SPRITE_RENDER_SCRATCH strict=yes\n");
        goto done;
    }

    stats->spriteDraws = sprites->draws;
    stats->spritePixels = sprites->pixelsDrawn;
    stats->glowDraws = sprites->glowDraws;
    stats->glowPixels = sprites->glowPixels;
    stats->spritePackReads = sprites->packReads;
    stats->viewportAfterSpritesFNV = viewportFNV();

    stats->temporaryHudBytes = 0U;
    stats->hudBandsRestoredFNV = hudBandsFNV();
    if (stats->hudBandsRestoredFNV != stats->hudBandsBeforeFNV) {
        printf("[TURNFRAME] DIAG fail=HUD_AFTER_SPRITES before=%08x after=%08x\n",
               (unsigned int)stats->hudBandsBeforeFNV,
               (unsigned int)stats->hudBandsRestoredFNV);
        goto done;
    }

    phaseStart = esp_timer_get_time();
    if (!EspNativeGameplayHudDirection_render(angle, hud)) {
        printf("[TURNFRAME] DIAG fail=HUD_DIRECTION angle=%u\n",
               (unsigned int)angle);
        goto done;
    }
    stats->hudMicros = elapsedMicros(phaseStart);
    stats->hudPackReads = hud->packReads;
    stats->hudPixels = hud->pixelsWritten;
    stats->hudBandsAfterFNV = hudBandsFNV();
    stats->angle = angle;
    stats->frameAfterFNV = frameFNV();
    if (stats->frameAfterFNV == 0U || stats->viewportAfterSpritesFNV == 0U ||
        stats->hudBandsAfterFNV == 0U) {
        printf("[TURNFRAME] DIAG fail=POST_HUD_FNV frame=%08x viewport=%08x hud=%08x\n",
               (unsigned int)stats->frameAfterFNV,
               (unsigned int)stats->viewportAfterSpritesFNV,
               (unsigned int)stats->hudBandsAfterFNV);
        goto done;
    }

    phaseStart = esp_timer_get_time();
    if (!Esp32PlatformVideo_present()) {
        printf("[TURNFRAME] DIAG fail=PRESENT\n");
        goto done;
    }
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
