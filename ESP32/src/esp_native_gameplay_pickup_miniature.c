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

/* HUD bottom starts at y=100. The 13px inset avoids the directional
 * player-damage cue at the left edge; no HUD pixels are touched. */
#define MINI_X 13U
#define MINI_Y 76U
#define MINI_DURATION 500U
#define MINI_ON 105U
#define MINI_PERIOD 165U
#define MINI_PULSES 3U

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

void EspPickupMini_enqueue(uint16_t logical) {
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
    state.pending[tail] = logical;
    ++state.count;
    printf("[PICKUPMINI] QUEUE logical=%u pending=%u active=%u input=unblocked\n",
           (unsigned)logical, (unsigned)state.count, (unsigned)state.active);
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
        const uint16_t logical = state.pending[state.head];
        state.head = (uint8_t)((state.head + 1U) % ESP_PICKUP_MINI_FIFO_CAP);
        --state.count;
        if (!EspNativeSpriteRenderer_decodePickupMini(
                logical, state.colors, state.mask)) {
            printf("[PICKUPMINI] SKIP logical=%u reason=decode gameplay=committed\n",
                   (unsigned)logical);
            continue;
        }
        state.active = 1U;
        state.first = 1U;
        state.shownMs = 0U;
        state.captured = 0U;
        state.lit = 0U;
        printf("[PICKUPMINI] START logical=%u at=13,76 size=20x20 blink=3x105ms/500ms fifo=%u input=unblocked\n",
               (unsigned)logical, (unsigned)state.count);
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
