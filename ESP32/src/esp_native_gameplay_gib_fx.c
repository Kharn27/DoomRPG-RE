#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_player_resources.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

/* This translation unit now owns the public session wrapper in front of the
 * proven player-resource chain. The resource implementation is renamed to the
 * private leaves declared by esp_native_gameplay_player_resources.h. */
#undef __wrap_EspNativeGameplaySession_reset
#undef __wrap_EspNativeGameplaySession_service

#define GIBFX_MAX_SPRITES 1024U
#define GIBFX_SEEN_BYTES (GIBFX_MAX_SPRITES / 8U)
#define GIBFX_VISUAL_HIDDEN 0x80U
#define GIBFX_WORLD_TOP 20
#define GIBFX_WORLD_BOTTOM 99
#define GIBFX_CENTER_X 80
#define GIBFX_CENTER_Y 52
#define GIBFX_MIN_PARTICLES 14U
#define GIBFX_MAX_PARTICLES 26U
#define GIBFX_CHUNKS 5U
#define GIBFX_DISPLAY_MS 350U
#define GIBFX_NO_SPRITE 0xffffU

#define GIBFX_RED_DARK 0x6000U
#define GIBFX_RED 0xb800U
#define GIBFX_RED_BRIGHT 0xf800U

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Native gib FX requires the 160x120 logical framebuffer"
#endif

typedef struct GibFxOwner_s {
    uint8_t seenHidden[GIBFX_SEEN_BYTES];
    uint32_t sourceArenaFNV1a;
    uint32_t bursts;
    uint32_t pixels;
    uint32_t clearAtMs;
    uint16_t activeSpriteIndex;
    uint8_t active;
    uint8_t reserved;
} GibFxOwner;

static GibFxOwner gibFxOwner;

static uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    if (x == 0U) x = 0x6d2b79f5U;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static const EspNativeGameplayMonsterView* syncOwner(void) {
    const EspNativeGameplayMonsterView* view =
        EspNativeGameplayMonsterState_view();

    if (view == NULL || view->records == NULL || view->count == 0U ||
        view->count > ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT ||
        view->sourceArenaFNV1a == 0U) {
        return NULL;
    }

    if (gibFxOwner.sourceArenaFNV1a != view->sourceArenaFNV1a) {
        memset(&gibFxOwner, 0, sizeof(gibFxOwner));
        gibFxOwner.sourceArenaFNV1a = view->sourceArenaFNV1a;
        gibFxOwner.activeSpriteIndex = GIBFX_NO_SPRITE;
        printf("[GIBFX] READY arena=%08x ownerBytes=%u maxSprites=%u mode=present-overlay expiryMs=%u gameplayRng=decoupled\n",
               (unsigned int)view->sourceArenaFNV1a,
               (unsigned int)sizeof(gibFxOwner),
               (unsigned int)GIBFX_MAX_SPRITES,
               (unsigned int)GIBFX_DISPLAY_MS);
    }
    return view;
}

static int seen(uint16_t spriteIndex) {
    return spriteIndex < GIBFX_MAX_SPRITES &&
           ((gibFxOwner.seenHidden[spriteIndex >> 3] >>
             (spriteIndex & 7U)) & 1U) != 0U;
}

static void setSeen(uint16_t spriteIndex, int value) {
    uint8_t mask;
    if (spriteIndex >= GIBFX_MAX_SPRITES) return;
    mask = (uint8_t)(1U << (spriteIndex & 7U));
    if (value) gibFxOwner.seenHidden[spriteIndex >> 3] |= mask;
    else gibFxOwner.seenHidden[spriteIndex >> 3] &= (uint8_t)~mask;
}

static void putPixel(uint16_t* framebuffer,
                     int x,
                     int y,
                     uint16_t color,
                     uint32_t* ioPixels) {
    if (framebuffer == NULL || ioPixels == NULL ||
        x < 0 || x >= DOOMRPG_LOGICAL_WIDTH ||
        y < GIBFX_WORLD_TOP || y > GIBFX_WORLD_BOTTOM) {
        return;
    }
    framebuffer[(unsigned int)y * DOOMRPG_LOGICAL_WIDTH + (unsigned int)x] = color;
    ++(*ioPixels);
}

static void drawDisc(uint16_t* framebuffer,
                     int cx,
                     int cy,
                     int radius,
                     uint16_t color,
                     uint32_t* ioPixels) {
    int y;
    int x;
    for (y = -radius; y <= radius; ++y) {
        for (x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= radius * radius) {
                putPixel(framebuffer, cx + x, cy + y, color, ioPixels);
            }
        }
    }
}

static uint32_t particleCount(const EspNativeGameplayMonsterRecord* monster) {
    uint32_t maxHealth;
    uint32_t count;
    if (monster == NULL) return GIBFX_MIN_PARTICLES;
    maxHealth = (monster->param1 >> 8) & 0xffU;
    count = GIBFX_MIN_PARTICLES + (maxHealth >> 1);
    if (count > GIBFX_MAX_PARTICLES) count = GIBFX_MAX_PARTICLES;
    return count;
}

static void drawBurst(uint16_t* framebuffer,
                      const EspNativeGameplayMonsterRecord* monster,
                      const EspNativeGameplayMonsterView* view) {
    uint32_t seed;
    uint32_t particles;
    uint32_t pixels = 0U;
    uint32_t i;

    if (framebuffer == NULL || monster == NULL || view == NULL) return;
    seed = view->sourceArenaFNV1a ^ view->stateFNV1a ^
           ((uint32_t)monster->spriteIndex * 0x9e3779b9U) ^ 0xa511e9b3U;
    particles = particleCount(monster);

    /* Legacy Combat_spawnBloodParticles() is a screen-space effect around the
     * crosshair. Native SELECT combat is currently a cardinal forward trace, so
     * its victim is centered in the viewport. Keep this owner bounded and
     * presentation-only; later projection ownership can replace the center
     * constants without changing monster/gameplay state. */
    for (i = 0U; i < particles; ++i) {
        uint32_t r = xorshift32(&seed);
        int x = GIBFX_CENTER_X + (int)(r % 45U) - 22;
        int y = GIBFX_CENTER_Y + (int)((r >> 8) % 31U) - 13;
        int radius = ((r >> 16) & 3U) == 0U ? 1 : 0;
        uint16_t color = ((r >> 20) & 3U) == 0U
                             ? GIBFX_RED_BRIGHT
                             : GIBFX_RED;
        drawDisc(framebuffer, x, y, radius, color, &pixels);
    }

    /* The legacy z=true path promotes five particles into gib chunks. We do not
     * import imgGibs or ParticleSystem ownership; five larger dark-red fragments
     * preserve that readable overkill cue with no asset allocation or gameplay
     * RNG consumption. */
    for (i = 0U; i < GIBFX_CHUNKS; ++i) {
        uint32_t r = xorshift32(&seed);
        int x = GIBFX_CENTER_X + (int)(r % 35U) - 17;
        int y = GIBFX_CENTER_Y + (int)((r >> 8) % 23U) - 13;
        drawDisc(framebuffer, x, y, 2, GIBFX_RED_DARK, &pixels);
    }

    ++gibFxOwner.bursts;
    gibFxOwner.pixels += pixels;
    gibFxOwner.activeSpriteIndex = monster->spriteIndex;
    gibFxOwner.clearAtMs = DoomRPG_GetUpTimeMS() + GIBFX_DISPLAY_MS;
    gibFxOwner.active = 1U;
    printf("[GIBFX] PAINT sprite=%u subtype=%u particles=%u chunks=%u pixels=%u center=%d,%d ownerBytes=%u leaseMs=%u visualRng=local gameplayRng=untouched legacyParticleSystem=no\n",
           (unsigned int)monster->spriteIndex,
           (unsigned int)monster->subtype,
           (unsigned int)particles,
           (unsigned int)GIBFX_CHUNKS,
           (unsigned int)pixels,
           GIBFX_CENTER_X,
           GIBFX_CENTER_Y,
           (unsigned int)sizeof(gibFxOwner),
           (unsigned int)GIBFX_DISPLAY_MS);
}

static void decorateNewGibs(void) {
    const EspNativeGameplayMonsterView* view = syncOwner();
    uint16_t* framebuffer;
    size_t expectedBytes;
    uint32_t i;

    if (view == NULL) return;

    expectedBytes = (size_t)DOOMRPG_LOGICAL_WIDTH *
                    (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (Esp32PlatformVideo_framebufferSizeBytes() != expectedBytes) return;
    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    if (framebuffer == NULL) return;

    for (i = 0U; i < view->count; ++i) {
        const EspNativeGameplayMonsterRecord* monster = &view->records[i];
        uint8_t visual = 0U;

        if (monster->spriteIndex >= GIBFX_MAX_SPRITES) continue;
        if (monster->alive != 0U) {
            setSeen(monster->spriteIndex, 0);
            continue;
        }
        if (!EspMapSpriteTopology_getVisualState(monster->spriteIndex, &visual)) {
            continue;
        }
        if ((visual & GIBFX_VISUAL_HIDDEN) == 0U || seen(monster->spriteIndex)) {
            continue;
        }

        setSeen(monster->spriteIndex, 1);
        drawBurst(framebuffer, monster, view);
    }
}

static void resetFx(void) {
    memset(&gibFxOwner, 0, sizeof(gibFxOwner));
    gibFxOwner.activeSpriteIndex = GIBFX_NO_SPRITE;
}

static void serviceExpiry(struct DoomRPG_s* doomRpg) {
    DoomRPG_t* runtime = (DoomRPG_t*)doomRpg;
    const EspPlayerViewState* playerView;
    EspNativeGameplayFrameStats frame;
    uint32_t now;
    uint16_t spriteIndex;

    if (syncOwner() == NULL || gibFxOwner.active == 0U) return;
    now = DoomRPG_GetUpTimeMS();
    if ((int32_t)(now - gibFxOwner.clearAtMs) < 0) return;

    /* Touch feedback owns an exact framebuffer snapshot for its short lease.
     * Clearing underneath it would be restored by the touch layer afterwards,
     * resurrecting the gib pixels. Retry on the next gameplay service instead. */
    if (EspNativeGameplayControls_isActive()) return;

    playerView = EspPlayerView_view();
    if (runtime == NULL || runtime->render == NULL || playerView == NULL ||
        playerView->active != 1U ||
        playerView->viewX != playerView->destX ||
        playerView->viewY != playerView->destY ||
        playerView->viewAngle != playerView->destAngle) {
        return;
    }

    spriteIndex = gibFxOwner.activeSpriteIndex;
    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(runtime->render,
                                           (uint8_t)playerView->viewAngle,
                                           &frame)) {
        printf("[GIBFX] EXPIRE-REDRAW-FAILED sprite=%u recovery=next-service gameplayRng=untouched\n",
               (unsigned int)spriteIndex);
        return;
    }

    gibFxOwner.active = 0U;
    gibFxOwner.activeSpriteIndex = GIBFX_NO_SPRITE;
    gibFxOwner.clearAtMs = 0U;
    printf("[GIBFX] EXPIRE sprite=%u leaseMs=%u frame=%08x presented=%u restored=world-redraw gameplayRng=untouched\n",
           (unsigned int)spriteIndex,
           (unsigned int)GIBFX_DISPLAY_MS,
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
}

/* esp_native_gameplay_present_gate.c calls this public leaf. The historical
 * action-feedback presenter is now presentBase(), allowing this generic effect
 * layer to decorate only the shared framebuffer before the already-proven
 * feedback + physical-present chain. */
int EspNativeGameplayActionEngine_present(void) {
    decorateNewGibs();
    return EspNativeGameplayActionEngine_presentBase();
}

/* Public gameplay-session wrappers. Run the complete existing player-resource /
 * action / monster-combat service first, then own only the bounded presentation
 * lease. No gameplay state or gameplay RNG is touched by this layer. */
void __wrap_EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg) {
    EspNativeGameplayPlayerResources_sessionService(doomRpg);
    serviceExpiry(doomRpg);
}

void __wrap_EspNativeGameplaySession_reset(void) {
    EspNativeGameplayPlayerResources_sessionReset();
    resetFx();
}
