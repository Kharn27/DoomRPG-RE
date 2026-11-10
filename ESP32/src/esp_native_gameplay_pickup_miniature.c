#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_asset_pack.h"
#include "esp_native_gameplay_pickup_miniature.h"
#include "esp_native_sprite_renderer.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120
#error "Pickup miniature expects the 160x120 native framebuffer"
#endif

/* HUD bottom starts at y=100. Retain the exact former bottom edge y=95:
 * growing the icon from 20 to 24 pixels expands UP, never into the HUD.
 * The 13px left inset avoids the directional player-damage cue. */
#define MINI_X 13U
#define MINI_BOTTOM_EXCLUSIVE 96U
#define MINI_Y (MINI_BOTTOM_EXCLUSIVE - ESP_PICKUP_MINI_SIZE)

_Static_assert(MINI_Y >= 20U && MINI_BOTTOM_EXCLUSIVE <= 100U,
               "Pickup miniature must stay inside the 3D viewport");
_Static_assert(MINI_X >= 13U && MINI_X + ESP_PICKUP_MINI_SIZE <= 160U,
               "Pickup miniature must stay clear of the left cue and framebuffer edge");
#define MINI_DURATION 500U
#define MINI_ON 105U
#define MINI_PERIOD 165U
#define MINI_PULSES 3U
#define MINI_SHOTGUN_FLAG 0x0100U
#define MINI_LOGICAL_MASK 0x00ffU
#define MINI_SHOTGUN_MIN_SPAN 12U

typedef struct PickupMini_s {
    uint16_t pending[ESP_PICKUP_MINI_FIFO_CAP];
    uint16_t colors[ESP_PICKUP_MINI_PIXELS];
    uint16_t background[ESP_PICKUP_MINI_PIXELS];
    uint8_t mask[ESP_PICKUP_MINI_MASK_BYTES];
    uint32_t shownMs;
    uint8_t head;
    uint8_t count;
    uint8_t active;
    uint8_t lit;
    uint8_t captured;
    uint8_t first;
    uint8_t fresh;
} PickupMini;
static PickupMini state;

static uint16_t* fb(void) {
    if (Esp32PlatformVideo_framebufferSizeBytes() !=
        (size_t)DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT *
             sizeof(uint16_t)) return NULL;
    return (uint16_t*)Esp32PlatformVideo_framebuffer();
}

static int litAt(uint32_t elapsed) {
    return elapsed < MINI_DURATION &&
           elapsed / MINI_PERIOD < MINI_PULSES &&
           elapsed % MINI_PERIOD < MINI_ON;
}

void EspPickupMini_reset(void) {
    /* Menu/automap/dialog own their entire screen: no stale pixel restore. */
    memset(&state, 0, sizeof(state));
}

/* Most pickups deliberately retain the original aspect-safe 24px fitting.
 * The upright shotgun can have a very narrow opaque silhouette in the PAK
 * bitshape. Only its pickup overlay receives a small opaque-span floor:
 * nearest-neighbor resampling stays inside the SAME 24x24 canvas. The
 * background array is safe as temporary scratch here: the real underlay
 * is captured only on the first paint, after this operation completes. */
static void boostShotgun(void) {
    uint8_t originalMask[ESP_PICKUP_MINI_MASK_BYTES];
    uint32_t x, y;
    uint32_t minX = ESP_PICKUP_MINI_SIZE, minY = ESP_PICKUP_MINI_SIZE;
    uint32_t maxX = 0U, maxY = 0U;
    uint32_t sourceW, sourceH, drawW, drawH, left, top;

    for (y = 0U; y < ESP_PICKUP_MINI_SIZE; ++y) {
        for (x = 0U; x < ESP_PICKUP_MINI_SIZE; ++x) {
            const uint32_t i = y * ESP_PICKUP_MINI_SIZE + x;
            if ((state.mask[i >> 3U] & (1U << (i & 7U))) == 0U) continue;
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }
    if (minX == ESP_PICKUP_MINI_SIZE) return;
    sourceW = maxX - minX + 1U;
    sourceH = maxY - minY + 1U;
    drawW = sourceW < MINI_SHOTGUN_MIN_SPAN
                ? MINI_SHOTGUN_MIN_SPAN : sourceW;
    drawH = sourceH < MINI_SHOTGUN_MIN_SPAN
                ? MINI_SHOTGUN_MIN_SPAN : sourceH;
    if (drawW == sourceW && drawH == sourceH) return;

    memcpy(originalMask, state.mask, sizeof(originalMask));
    memcpy(state.background, state.colors, sizeof(state.colors));
    memset(state.colors, 0, sizeof(state.colors));
    memset(state.mask, 0, sizeof(state.mask));
    left = (ESP_PICKUP_MINI_SIZE - drawW) / 2U;
    top = (ESP_PICKUP_MINI_SIZE - drawH) / 2U;
    for (y = 0U; y < drawH; ++y) {
        const uint32_t sy =
            minY + ((2U * y + 1U) * sourceH) / (2U * drawH);
        for (x = 0U; x < drawW; ++x) {
            const uint32_t sx =
                minX + ((2U * x + 1U) * sourceW) / (2U * drawW);
            const uint32_t source = sy * ESP_PICKUP_MINI_SIZE + sx;
            const uint32_t target =
                (top + y) * ESP_PICKUP_MINI_SIZE + left + x;
            if ((originalMask[source >> 3U] & (1U << (source & 7U))) == 0U)
                continue;
            state.colors[target] = state.background[source];
            state.mask[target >> 3U] |= (uint8_t)(1U << (target & 7U));
        }
    }
    printf("[PICKUPMINI] SHOTGUN-FIT opaque=%ux%u fitted=%ux%u canvas=24x24 cosmetic-only\n",
           (unsigned)sourceW, (unsigned)sourceH,
           (unsigned)drawW, (unsigned)drawH);
}

static void enqueueWithFlags(uint16_t logical, uint16_t flags) {
    uint8_t tail;
    if (logical >= 256U) {
        printf("[PICKUPMINI] SKIP logical=%u reason=unsupported visual-only\n",
               (unsigned)logical);
        return;
    }
    if (state.count == ESP_PICKUP_MINI_FIFO_CAP) {
        /* The oldest waiting visual can be dropped; gameplay never waits. */
        state.head = (uint8_t)((state.head + 1U) % ESP_PICKUP_MINI_FIFO_CAP);
        --state.count;
        printf("[PICKUPMINI] OVERFLOW oldest-queued-dropped gameplay=unchanged\n");
    }
    tail = (uint8_t)((state.head + state.count) % ESP_PICKUP_MINI_FIFO_CAP);
    state.pending[tail] = (uint16_t)(logical | flags);
    ++state.count;
    printf("[PICKUPMINI] QUEUE logical=%u pending=%u active=%u input=unblocked\n",
           (unsigned)logical, (unsigned)state.count, (unsigned)state.active);
}

void EspPickupMini_enqueue(uint16_t logical) {
    enqueueWithFlags(logical, 0U);
}

void EspPickupMini_enqueueShotgun(uint16_t logical) {
    enqueueWithFlags(logical, MINI_SHOTGUN_FLAG);
}

void EspPickupMini_markFreshFrame(void) { state.fresh = 1U; }

static void restore(void) {
    uint16_t* dst = fb();
    uint32_t y;
    if (!state.captured || !state.lit || dst == NULL) return;
    for (y = 0; y < ESP_PICKUP_MINI_SIZE; ++y)
        memcpy(dst + (MINI_Y + y) * DOOMRPG_LOGICAL_WIDTH + MINI_X,
               state.background + y * ESP_PICKUP_MINI_SIZE,
               ESP_PICKUP_MINI_SIZE * sizeof(uint16_t));
    state.lit = 0U;
}

static void paint(void) {
    uint16_t* dst = fb();
    uint32_t y, x;
    if (dst == NULL) return;
    if (!state.captured) {
        for (y = 0; y < ESP_PICKUP_MINI_SIZE; ++y)
            memcpy(state.background + y * ESP_PICKUP_MINI_SIZE,
                   dst + (MINI_Y + y) * DOOMRPG_LOGICAL_WIDTH + MINI_X,
                   ESP_PICKUP_MINI_SIZE * sizeof(uint16_t));
        state.captured = 1U;
    }
    for (y = 0; y < ESP_PICKUP_MINI_SIZE; ++y) {
        for (x = 0; x < ESP_PICKUP_MINI_SIZE; ++x) {
            const uint32_t i = y * ESP_PICKUP_MINI_SIZE + x;
            if (state.mask[i >> 3] & (1U << (i & 7U)))
                dst[(MINI_Y + y) * DOOMRPG_LOGICAL_WIDTH + MINI_X + x] =
                    state.colors[i];
        }
    }
    state.lit = 1U;
}

static void startNext(void) {
    /* Do not steal the map/dialog PAK lease, or block input on bad art. */
    while (!state.active && state.count && !EspAssetPack_isOpen()) {
        const uint16_t queued = state.pending[state.head];
        const uint16_t logical = queued & MINI_LOGICAL_MASK;
        state.head = (uint8_t)((state.head + 1U) % ESP_PICKUP_MINI_FIFO_CAP);
        --state.count;
        if (!EspNativeSpriteRenderer_decodePickupMini(
                logical, state.colors, state.mask)) {
            printf("[PICKUPMINI] SKIP logical=%u reason=decode gameplay=committed\n",
                   (unsigned)logical);
            continue;
        }
        if ((queued & MINI_SHOTGUN_FLAG) != 0U) boostShotgun();
        state.active = 1U;
        state.first = 1U;
        state.shownMs = 0U;
        state.captured = 0U;
        state.lit = 0U;
        printf("[PICKUPMINI] START logical=%u at=%u,%u size=%ux%u blink=3x105ms/500ms fifo=%u input=unblocked\n",
               (unsigned)logical, (unsigned)MINI_X, (unsigned)MINI_Y,
               (unsigned)ESP_PICKUP_MINI_SIZE, (unsigned)ESP_PICKUP_MINI_SIZE,
               (unsigned)state.count);
    }
}

void EspPickupMini_beforePresent(uint32_t nowMs) {
    uint32_t elapsed;
    if (state.fresh) {
        /* Repainted world replaces the old background. Never restore stale
         * camera pixels into a new frame (including movement previews). */
        state.captured = 0U;
        state.lit = 0U;
        state.fresh = 0U;
    }
    if (state.active && !state.first &&
        nowMs - state.shownMs >= MINI_DURATION) {
        restore();
        state.active = 0U;
        state.captured = 0U;
        printf("[PICKUPMINI] END elapsedMs=%u pending=%u input=unblocked\n",
               (unsigned)(nowMs - state.shownMs), (unsigned)state.count);
    }
    if (!state.active) startNext();
    if (!state.active) return;
    elapsed = state.first ? 0U : nowMs - state.shownMs;
    if (litAt(elapsed)) {
        if (!state.lit) paint();
    } else if (state.lit) {
        restore();
    }
}

void EspPickupMini_afterPresent(uint32_t nowMs) {
    if (state.active && state.first && state.lit) {
        state.first = 0U;
        state.shownMs = nowMs;
    }
}

int EspPickupMini_due(uint32_t nowMs) {
    if (!state.active) return state.count != 0U;
    if (state.first) return 1;
    if (nowMs - state.shownMs >= MINI_DURATION) return 1;
    return litAt(nowMs - state.shownMs) != (state.lit != 0U);
}
