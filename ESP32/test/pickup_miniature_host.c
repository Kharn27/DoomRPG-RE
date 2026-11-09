/* Bounded PAK pickup icon: exercise the REAL compositor/FIFO/timing code
 * with deterministic fake asset decoder, shared 160x120 pixels and clock.
 * No hardware performance or palette equivalence is implied. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/esp_native_gameplay_pickup_miniature.c"

static uint16_t buffer[160U * 120U + 2U];
static unsigned decodeCount;
static unsigned busyPak;
static unsigned absentLogical = 255U;

void* Esp32PlatformVideo_framebuffer(void) { return buffer + 1U; }
size_t Esp32PlatformVideo_framebufferSizeBytes(void) {
    return 160U * 120U * sizeof(uint16_t);
}
int EspAssetPack_isOpen(void) { return busyPak; }

int EspNativeSpriteRenderer_decodePickupMini(
    uint16_t logical,
    uint16_t colors[ESP_PICKUP_MINI_PIXELS],
    uint8_t opaque[ESP_PICKUP_MINI_MASK_BYTES]) {
    unsigned i;
    ++decodeCount;
    if (logical == absentLogical) return 0;
    for (i = 0; i < ESP_PICKUP_MINI_PIXELS; ++i)
        colors[i] = (uint16_t)(0x4000U | logical);
    memset(opaque, 0xff, ESP_PICKUP_MINI_MASK_BYTES);
    return 1;
}

static void fill(uint16_t value) {
    unsigned i;
    for (i = 1U; i <= 160U * 120U; ++i) buffer[i] = value;
    buffer[0] = 0xabcdU;
    buffer[160U * 120U + 1U] = 0xdcbaU;
}
static uint16_t pixel(unsigned x, unsigned y) {
    return buffer[1U + y * 160U + x];
}
static void unchangedHudAndGuard(uint16_t expected) {
    unsigned x, y;
    assert(buffer[0] == 0xabcdU);
    assert(buffer[160U * 120U + 1U] == 0xdcbaU);
    /* Every pixel OUTSIDE the icon canvas must stay untouched:
     * top HUD, bottom HUD, left damage cue inset and nearby 3D world. */
    for (y = 0; y < 120U; ++y)
        for (x = 0; x < 160U; ++x)
            if (x < MINI_X || x >= MINI_X + ESP_PICKUP_MINI_SIZE ||
                y < MINI_Y || y >= MINI_Y + ESP_PICKUP_MINI_SIZE)
                assert(pixel(x, y) == expected);
}
static void entireBackground(uint16_t expected) {
    unsigned x, y;
    unchangedHudAndGuard(expected);
    for (y = MINI_Y; y < MINI_Y + ESP_PICKUP_MINI_SIZE; ++y)
        for (x = MINI_X; x < MINI_X + ESP_PICKUP_MINI_SIZE; ++x)
            assert(pixel(x, y) == expected);
}

int main(void) {
    unsigned before;
    uint16_t selected = 0xeeeeU;
    uint32_t drawW = 0U, drawH = 0U;
    /* The old 20px icon ended at y=95. The 24px version expands upward,
     * retaining the exact old bottom alignment and avoiding both HUD bands.
     * The cosmetic state stays bounded without an extra framebuffer. */
    _Static_assert(ESP_PICKUP_MINI_SIZE == 24U, "24px icon requested");
    _Static_assert(ESP_PICKUP_MINI_PIXELS == 576U, "24px RGB565 count");
    _Static_assert(ESP_PICKUP_MINI_MASK_BYTES == 72U, "24px opacity mask");
    _Static_assert(MINI_X == 13U && MINI_Y == 72U, "same bottom anchor");
    _Static_assert(sizeof(state.colors) == 1152U, "bounded 24px colors");
    _Static_assert(sizeof(state.background) == 1152U, "bounded underlay");
    /* Real PAK bitshapes can be much smaller than the icon canvas.
     * The actual pixels must grow, not just transparent padding. */
    assert(EspPickupMini_fitDimensions(12U, 12U, &drawW, &drawH));
    assert(drawW == 24U && drawH == 24U); /* Small original UPSCALES. */
    assert(EspPickupMini_fitDimensions(20U, 8U, &drawW, &drawH));
    assert(drawW == 24U && drawH == 10U); /* Aspect ratio retained. */
    assert(EspPickupMini_fitDimensions(64U, 32U, &drawW, &drawH));
    assert(drawW == 24U && drawH == 12U); /* Large original DOWNSCALES. */
    assert(EspPickupMini_fitDimensions(12U, 20U, &drawW, &drawH));
    assert(drawW == 14U && drawH == 24U); /* Tall original. */
    assert(EspPickupMini_fitDimensions(1U, 64U, &drawW, &drawH));
    assert(drawW == 1U && drawH == 24U); /* Extremely narrow. */
    assert(!EspPickupMini_fitDimensions(0U, 8U, &drawW, &drawH));
    assert(!EspPickupMini_fitDimensions(12U, 65U, &drawW, &drawH));
    assert(!EspPickupMini_fitDimensions(12U, 12U, NULL, &drawH));
    /* The immutable sprite of a smashed crate is still the CRATE.
     * The effective entity-def tile is the loot visual, just as in the
     * production crate-state/renderer overlay. Verify all three sources. */
    assert(EspPickupMini_resolveVisualLogical(140U, 79U, 1, &selected));
    assert(selected == 79U); /* Transformed crate -> actual loot, not 140. */
    assert(EspPickupMini_resolveVisualLogical(140U, 79U, 0, &selected));
    assert(selected == 140U); /* Regular immutable world sprite. */
    assert(EspPickupMini_resolveVisualLogical(83U, 83U, 0, &selected));
    assert(selected == 83U); /* Dynamic monster drop. */
    assert(EspPickupMini_resolveVisualLogical(82U, 82U, 0, &selected));
    assert(selected == 81U); /* Original CROSS atlas resource. */
    assert(EspPickupMini_resolveVisualLogical(140U, 82U, 1, &selected));
    assert(selected == 81U); /* Crate loot with CROSS format. */
    assert(!EspPickupMini_resolveVisualLogical(140U, 256U, 1, &selected));
    assert(selected == 81U); /* Invalid art never queues or overwrites. */
    assert(!EspPickupMini_resolveVisualLogical(400U, 77U, 0, &selected));
    assert(!EspPickupMini_resolveVisualLogical(140U, 79U, 1, NULL));
    /* The actual sprite decoder calls this helper once per opaque texel.
     * Saturation, black shadow lines and source color hue are invariant.
     * The animation/underlay fixture below remains pixel-exact. */
    assert(EspPickupMini_brighten565(0x0000U) == 0x0000U);
    assert(EspPickupMini_brighten565(0xffffU) == 0xffffU);
    assert(EspPickupMini_brighten565(0xf800U) == 0xf800U);
    assert(EspPickupMini_brighten565(0x07e0U) == 0x07e0U);
    assert(EspPickupMini_brighten565(0x001fU) == 0x001fU);
    assert(EspPickupMini_brighten565(0x2104U) == 0x3186U);
    assert(EspPickupMini_brighten565(0x4208U) == 0x630cU);
    for (unsigned n = 0U; n < 32U; ++n) {
        uint16_t c = (uint16_t)(n << 11);
        assert(EspPickupMini_brighten565(c) >= c);
        assert((EspPickupMini_brighten565(c) & 0x07ffU) == 0U);
    }
    fill(0x1234U);
    EspPickupMini_reset();
    assert(!EspPickupMini_due(100U));
    EspPickupMini_enqueue(80U);
    EspPickupMini_enqueue(81U); /* Already collected while first still pending. */
    assert(state.count == 2U && !state.active);
    busyPak = 1U;
    EspPickupMini_beforePresent(100U);
    assert(state.count == 2U && !state.active && !decodeCount);
    busyPak = 0U;
    assert(EspPickupMini_due(100U));
    EspPickupMini_beforePresent(100U);
    assert(state.active && state.count == 1U && state.first);
    assert(pixel(MINI_X, MINI_Y) == (uint16_t)(0x4000U | 80U));
    assert(pixel(MINI_X + ESP_PICKUP_MINI_SIZE - 1U,
                 MINI_Y + ESP_PICKUP_MINI_SIZE - 1U) ==
           (uint16_t)(0x4000U | 80U));
    unchangedHudAndGuard(0x1234U);
    EspPickupMini_afterPresent(100U);
    assert(!state.first && state.shownMs == 100U);
    assert(!EspPickupMini_due(104U));
    assert(decodeCount == 1U);

    assert(EspPickupMini_due(205U));
    EspPickupMini_beforePresent(205U);
    EspPickupMini_afterPresent(205U);
    assert(!state.lit);
    entireBackground(0x1234U);
    assert(decodeCount == 1U);
    assert(EspPickupMini_due(265U));
    EspPickupMini_beforePresent(265U);
    EspPickupMini_afterPresent(265U);
    assert(state.lit && decodeCount == 1U);
    assert(pixel(MINI_X, MINI_Y) == (uint16_t)(0x4000U | 80U));

    /* New camera frame during lit interval: its NEW underlay wins. */
    fill(0x5678U);
    EspPickupMini_markFreshFrame();
    EspPickupMini_beforePresent(275U);
    EspPickupMini_afterPresent(275U);
    assert(state.lit && pixel(MINI_X, MINI_Y) == (uint16_t)(0x4000U | 80U));
    assert(decodeCount == 1U);
    assert(EspPickupMini_due(370U)); /* 270ms elapsed => OFF. */
    EspPickupMini_beforePresent(370U);
    assert(!state.lit);
    entireBackground(0x5678U);
    unchangedHudAndGuard(0x5678U);

    /* Elapsed 500ms: next visual begins without game/input gating. */
    assert(EspPickupMini_due(600U));
    EspPickupMini_beforePresent(600U);
    assert(state.active && state.first && !state.count);
    assert(pixel(MINI_X, MINI_Y) == (uint16_t)(0x4000U | 81U));
    assert(decodeCount == 2U);
    EspPickupMini_afterPresent(600U);
    EspPickupMini_beforePresent(1100U);
    assert(!state.active);
    entireBackground(0x5678U);
    assert(!EspPickupMini_due(1100U));
    unchangedHudAndGuard(0x5678U);

    /* Absent source does not poison queue, and waits for PAK unlock. */
    EspPickupMini_enqueue(255U);
    EspPickupMini_enqueue(83U);
    before = decodeCount;
    busyPak = 1U;
    EspPickupMini_beforePresent(1200U);
    assert(state.count == 2U && decodeCount == before);
    busyPak = 0U;
    EspPickupMini_beforePresent(1200U);
    EspPickupMini_afterPresent(1200U);
    assert(state.active && !state.count && decodeCount == before + 2U);
    assert(pixel(MINI_X, MINI_Y) == (uint16_t)(0x4000U | 83U));

    EspPickupMini_reset();
    fill(0x2222U);
    for (unsigned n = 0; n < ESP_PICKUP_MINI_FIFO_CAP + 1U; ++n)
        EspPickupMini_enqueue((uint16_t)(100U + n));
    assert(state.count == ESP_PICKUP_MINI_FIFO_CAP && !state.active);
    assert(state.pending[state.head] == 101U); /* Drop only oldest waiting. */
    EspPickupMini_beforePresent(1400U);
    assert(state.active && state.count == ESP_PICKUP_MINI_FIFO_CAP - 1U);
    assert(pixel(MINI_X, MINI_Y) == (uint16_t)(0x4000U | 101U));
    unchangedHudAndGuard(0x2222U);

    puts("pickup_miniature_host: PASS 24px actual up/downscale, aspect-safe, bottom-anchored, full HUD/world guards, exact underlay restore, crate-loot, CROSS, RGB565, FIFO, 500ms, PAK lease, cosmetic-only");
    return 0;
}
