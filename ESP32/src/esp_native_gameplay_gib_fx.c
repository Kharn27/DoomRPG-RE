#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_map_sprite_topology.h"
#include "esp_asset_pack.h"
#include "esp_entity_def_type_catalog.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_hit_feedback.h"
#include "esp_native_gameplay_controls.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_monster_attack_visual.h"
#include "esp_native_gameplay_monster_movement_probe.h"
#include "esp_native_gameplay_monster_retaliation.h"
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

#define HITFX_MAX_PARTICLES 64U
#define HITFX_DISPLAY_MS 350U
#define HITFX_RED565 0xb800U
#define HITFX_GREEN565 0x0600U
#define HITFX_BLUE565 0x0017U

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

typedef struct HitFxOwner_s {
    uint32_t sourceArenaFNV1a;
    uint32_t sequence;
    uint32_t seed;
    uint32_t armedAtMs;
    uint32_t clearAtMs;
    uint32_t paints;
    uint32_t pixels;
    uint16_t spriteIndex;
    uint16_t color565;
    uint8_t distance;
    uint8_t particleCount;
    uint8_t active;
    uint8_t reserved;
} HitFxOwner;

static HitFxOwner hitFxOwner;

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

static uint32_t hitDistanceScale(uint8_t distance) {
    uint32_t scale = 256U;
    uint8_t i;

    if (distance <= 1U) return scale;
    /*
     * Legacy ParticleSystem_calculateScales() uses the recovered 174/256
     * vertical factor for the effective particle-count scale at tile distance
     * 2..4. Preserve that integer decay without importing ParticleSystem.
     */
    scale = 174U;
    for (i = 1U; i < distance; ++i) {
        scale = (scale * 174U) >> 8;
    }
    return scale;
}

static uint32_t hitParticleCount(
    const EspNativeGameplayMonsterRecord* monster,
    int32_t healthBefore,
    int32_t armorBefore,
    int32_t totalDamage,
    int32_t totalArmorDamage,
    uint8_t distance) {
    uint32_t maxHealth;
    uint32_t maxArmor;
    uint32_t denominator;
    uint32_t damageTotal;
    uint32_t intensity;
    uint32_t count;
    uint32_t scale;
    int32_t remaining;

    if (monster == NULL || totalDamage < 0 || totalArmorDamage < 0) return 0U;
    damageTotal = (uint32_t)(totalDamage + totalArmorDamage);
    if (damageTotal == 0U) return 0U;

    maxHealth = (monster->param1 >> 8) & 0xffU;
    maxArmor = (monster->param1 >> 24) & 0xffU;
    denominator = maxHealth;
    if (armorBefore > 0) denominator += maxArmor;
    if (denominator == 0U) denominator = 1U;

    /* Exact integer shape of legacy Combat_calcParticleIntensity(). */
    intensity = ((((damageTotal << 16) / (denominator << 8)) *
                  12288U) >> 8);
    remaining = healthBefore + armorBefore -
                totalDamage - totalArmorDamage;
    if (remaining <= 0) intensity = (intensity * 512U) >> 8;
    intensity += 128U;
    if (intensity < 256U) intensity = 256U;
    intensity >>= 8;

    scale = hitDistanceScale(distance);
    count = ((intensity * scale) + 128U) >> 8;
    if (count == 0U) count = 1U;
    if (count > HITFX_MAX_PARTICLES) count = HITFX_MAX_PARTICLES;
    return count;
}

static uint16_t hitBloodColor(const EspNativeGameplayMonsterRecord* monster) {
    int32_t parm = 0;
    if (monster == NULL) return HITFX_RED565;
    (void)EspEntityDefTypeCatalog_getParm(monster->defTile, &parm);
    if (parm == 467 && monster->subtype == 6U) return HITFX_BLUE565;
    if (parm == 467 && monster->subtype == 11U) return HITFX_GREEN565;
    return HITFX_RED565;
}

int EspNativeGameplayHitFeedback_arm(uint32_t sequence,
                                     uint16_t spriteIndex,
                                     uint8_t distance,
                                     int32_t healthBefore,
                                     int32_t armorBefore,
                                     int32_t totalDamage,
                                     int32_t totalArmorDamage) {
    const EspNativeGameplayMonsterView* view = syncOwner();
    const EspNativeGameplayMonsterRecord* monster;
    uint32_t count;

    if (view == NULL || distance == 0U || distance > 4U ||
        totalDamage < 0 || totalArmorDamage < 0 ||
        totalDamage + totalArmorDamage <= 0) {
        return 0;
    }
    monster = EspNativeGameplayMonsterState_find(spriteIndex);
    if (monster == NULL) return 0;
    count = hitParticleCount(monster, healthBefore, armorBefore,
                             totalDamage, totalArmorDamage, distance);
    if (count == 0U) return 0;

    memset(&hitFxOwner, 0, sizeof(hitFxOwner));
    hitFxOwner.sourceArenaFNV1a = view->sourceArenaFNV1a;
    hitFxOwner.sequence = sequence;
    hitFxOwner.spriteIndex = spriteIndex;
    hitFxOwner.distance = distance;
    hitFxOwner.particleCount = (uint8_t)count;
    hitFxOwner.color565 = hitBloodColor(monster);
    hitFxOwner.seed = view->sourceArenaFNV1a ^ view->stateFNV1a ^
                      sequence ^ ((uint32_t)spriteIndex * 0x9e3779b9U) ^
                      0x51ed270bU;
    if (hitFxOwner.seed == 0U) hitFxOwner.seed = 0x6d2b79f5U;
    hitFxOwner.armedAtMs = DoomRPG_GetUpTimeMS();
    hitFxOwner.active = 1U;

    printf("[HITFX] ARM seq=%u sprite=%u subtype=%u distance=%u damage=%d+%d total=%d color565=%04x particles=%u ownerBytes=%u visualRng=local gameplayRng=untouched\n",
           (unsigned int)sequence,
           (unsigned int)spriteIndex,
           (unsigned int)monster->subtype,
           (unsigned int)distance,
           (int)totalDamage,
           (int)totalArmorDamage,
           (int)(totalDamage + totalArmorDamage),
           (unsigned int)hitFxOwner.color565,
           (unsigned int)hitFxOwner.particleCount,
           (unsigned int)sizeof(hitFxOwner));
    return 1;
}

int EspNativeGameplayHitFeedback_cancel(uint32_t sequence) {
    if (hitFxOwner.active == 0U || hitFxOwner.sequence != sequence) {
        return 0;
    }
    printf("[HITFX] CANCEL seq=%u sprite=%u painted=%u rollback=yes gameplayRng=untouched\n",
           (unsigned int)hitFxOwner.sequence,
           (unsigned int)hitFxOwner.spriteIndex,
           (unsigned int)hitFxOwner.paints);
    memset(&hitFxOwner, 0, sizeof(hitFxOwner));
    hitFxOwner.spriteIndex = GIBFX_NO_SPRITE;
    return 1;
}

static uint16_t hitDarkColor(uint16_t color) {
    return (uint16_t)((color >> 1) & 0x7befU);
}

static int signInt(int value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

static void drawHitBurst(uint16_t* framebuffer) {
    const EspNativeGameplayMonsterView* view = syncOwner();
    uint32_t seed;
    uint32_t scale;
    uint32_t pixels = 0U;
    uint32_t now;
    uint32_t ageMs;
    uint32_t i;
    int gravity;

    if (framebuffer == NULL || view == NULL || hitFxOwner.active == 0U ||
        hitFxOwner.sourceArenaFNV1a != view->sourceArenaFNV1a) {
        return;
    }

    now = DoomRPG_GetUpTimeMS();
    if (hitFxOwner.clearAtMs != 0U &&
        (int32_t)(now - hitFxOwner.clearAtMs) >= 0) {
        return;
    }
    if (hitFxOwner.clearAtMs == 0U) {
        hitFxOwner.clearAtMs = now + HITFX_DISPLAY_MS;
    }

    /*
     * Recompute a deterministic legacy-shaped particle field for every present
     * instead of storing 64 ParticleNode objects. The recovered ParticleSystem
     * ranges are:
     *   startX -6..6, startY -9..11
     *   velX   -150..100, velY -160..-60, gravity 10, size 1..4
     *
     * Legacy fixed-point integration reduces to approximately:
     *   dx = velX * ageMs / 1000
     *   dy = velY * ageMs / 1000 + gravity * ageMs^2 / 25600
     *
     * Because armedAtMs is captured before the attack-frame render, the first
     * physical present already has meaningful particle travel. This avoids the
     * dense red stamp produced by rendering all particles at their spawn point.
     */
    ageMs = now - hitFxOwner.armedAtMs;
    if (ageMs > HITFX_DISPLAY_MS) ageMs = HITFX_DISPLAY_MS;
    scale = hitDistanceScale(hitFxOwner.distance);
    gravity = (10 * (int)scale + 128) >> 8;
    if (gravity < 1) gravity = 1;
    seed = hitFxOwner.seed;

    for (i = 0U; i < hitFxOwner.particleCount; ++i) {
        uint32_t r0 = xorshift32(&seed);
        uint32_t r1 = xorshift32(&seed);
        uint32_t r2 = xorshift32(&seed);
        int startX = (int)(r0 % 13U) - 6;
        int startY = (int)((r0 >> 8) % 21U) - 9;
        int velX = -150 + (int)(r1 % 251U);
        int velY = -160 + (int)((r1 >> 8) % 101U);
        int size = 1 + (int)(r2 & 3U);
        int x;
        int y;
        int tailX;
        int tailY;
        uint16_t color = hitFxOwner.color565;

        startX = (startX * (int)scale) >> 8;
        startY = (startY * (int)scale) >> 8;
        velX = (velX * (int)scale) >> 8;
        velY = (velY * (int)scale) >> 8;
        size = (size * (int)scale + 128) >> 8;
        if (size < 1) size = 1;

        x = GIBFX_CENTER_X + startX +
            (int)(((int64_t)velX * (int64_t)ageMs) / 1000);
        y = GIBFX_CENTER_Y + startY +
            (int)(((int64_t)velY * (int64_t)ageMs) / 1000) +
            (int)(((int64_t)gravity * (int64_t)ageMs *
                   (int64_t)ageMs) / 25600);

        /*
         * One logical pixel is already 2x2 physical pixels on the CYD. Keep
         * most droplets one pixel and turn only the larger legacy sizes into a
         * one-pixel tail. This reads as a spray rather than an opaque blob.
         */
        putPixel(framebuffer, x, y, color, &pixels);
        if (size >= 3) {
            tailX = x - signInt(velX);
            tailY = y - signInt(velY);
            putPixel(framebuffer, tailX, tailY,
                     hitDarkColor(color), &pixels);
        }
    }

    ++hitFxOwner.paints;
    hitFxOwner.pixels += pixels;
    if (hitFxOwner.paints == 1U) {
        printf("[HITFX] PAINT seq=%u sprite=%u particles=%u pixels=%u center=%d,%d ageMs=%u leaseMs=%u color565=%04x motion=legacy-kinematic-spray presentOverlay=yes gameplayRng=untouched\n",
               (unsigned int)hitFxOwner.sequence,
               (unsigned int)hitFxOwner.spriteIndex,
               (unsigned int)hitFxOwner.particleCount,
               (unsigned int)pixels,
               GIBFX_CENTER_X,
               GIBFX_CENTER_Y,
               (unsigned int)ageMs,
               (unsigned int)HITFX_DISPLAY_MS,
               (unsigned int)hitFxOwner.color565);
    }
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

static void decorateActiveHit(void) {
    uint16_t* framebuffer;
    size_t expectedBytes;

    if (hitFxOwner.active == 0U) return;
    expectedBytes = (size_t)DOOMRPG_LOGICAL_WIDTH *
                    (size_t)DOOMRPG_LOGICAL_HEIGHT * sizeof(uint16_t);
    if (Esp32PlatformVideo_framebufferSizeBytes() != expectedBytes) return;
    framebuffer = (uint16_t*)Esp32PlatformVideo_framebuffer();
    if (framebuffer == NULL) return;
    drawHitBurst(framebuffer);
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
    memset(&hitFxOwner, 0, sizeof(hitFxOwner));
    hitFxOwner.spriteIndex = GIBFX_NO_SPRITE;
}

static void serviceHitExpiry(struct DoomRPG_s* doomRpg) {
    DoomRPG_t* runtime = (DoomRPG_t*)doomRpg;
    const EspPlayerViewState* playerView;
    EspNativeGameplayFrameStats frame;
    uint32_t now;
    uint16_t spriteIndex;

    if (hitFxOwner.active == 0U || hitFxOwner.clearAtMs == 0U) return;
    now = DoomRPG_GetUpTimeMS();
    if ((int32_t)(now - hitFxOwner.clearAtMs) < 0) return;

    /* Keep exact short framebuffer owners and dialog PAK leases authoritative.
     * Action feedback is serviced earlier in the same session chain, so a fresh
     * redraw here can repaint an unexpired top-bar lease without truncating it. */
    if (EspNativeGameplayControls_isActive() || EspAssetPack_isOpen()) return;

    playerView = EspPlayerView_view();
    if (runtime == NULL || runtime->render == NULL || playerView == NULL ||
        playerView->active != 1U ||
        playerView->viewX != playerView->destX ||
        playerView->viewY != playerView->destY ||
        playerView->viewAngle != playerView->destAngle) {
        return;
    }

    spriteIndex = hitFxOwner.spriteIndex;
    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(runtime->render,
                                           (uint8_t)playerView->viewAngle,
                                           &frame)) {
        printf("[HITFX] EXPIRE-REDRAW-FAILED sprite=%u recovery=next-service gameplayRng=untouched\n",
               (unsigned int)spriteIndex);
        return;
    }

    hitFxOwner.active = 0U;
    hitFxOwner.clearAtMs = 0U;
    hitFxOwner.spriteIndex = GIBFX_NO_SPRITE;
    printf("[HITFX] EXPIRE sprite=%u leaseMs=%u paints=%u pixels=%u frame=%08x presented=%u restored=world-redraw gameplayRng=untouched\n",
           (unsigned int)spriteIndex,
           (unsigned int)HITFX_DISPLAY_MS,
           (unsigned int)hitFxOwner.paints,
           (unsigned int)hitFxOwner.pixels,
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
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
    decorateActiveHit();
    decorateNewGibs();
    return EspNativeGameplayActionEngine_presentBase();
}

/* Public gameplay-session wrapper and explicit permanent composition point.
 * Run player resources/action/monster combat/turn first, then arm the bounded
 * primary monster attack pose before the existing retaliation consumes the same
 * proven turn probe. Movement probing and presentation-only gib expiry remain
 * downstream. The movement adapter borrows an exact reserved post-refill RNG
 * table when a movement turn lands on nextRand==127, then restores the live
 * Random_t byte-for-byte; the already-proven byte RNG guard owns the future
 * exact live replay. */
void __wrap_EspNativeGameplaySession_service(struct DoomRPG_s* doomRpg) {
    EspNativeGameplayPlayerResources_sessionService(doomRpg);
    EspNativeGameplayMonsterAttackVisual_service(doomRpg);
    EspNativeGameplayMonsterRetaliation_service(doomRpg);
    EspNativeGameplayMonsterMovementProbe_service(doomRpg);
    serviceHitExpiry(doomRpg);
    serviceExpiry(doomRpg);
}

void __wrap_EspNativeGameplaySession_reset(void) {
    EspNativeGameplayPlayerResources_sessionReset();
    EspNativeGameplayMonsterAttackVisual_reset();
    EspNativeGameplayMonsterRetaliation_reset();
    EspNativeGameplayMonsterMovementProbe_reset();
    resetFx();
}
