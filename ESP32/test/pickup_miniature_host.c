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
    for (y = 0; y < 20U; ++y)
        for (x = 0; x < 160U; ++x) assert(pixel(x,y) == expected);
    for (y = 100U; y < 120U; ++y)
        for (x = 0; x < 160U; ++x) assert(pixel(x,y) == expected);
    assert(pixel(12,76) == expected);
    assert(pixel(33,95) == expected);
}

int main(void) {
    unsigned before;
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
    assert(pixel(13,76) == (uint16_t)(0x4000U | 80U));
    unchangedHudAndGuard(0x1234U);
    EspPickupMini_afterPresent(100U);
    assert(!state.first && state.shownMs == 100U);
    assert(!EspPickupMini_due(104U));
    assert(decodeCount == 1U);

    assert(EspPickupMini_due(205U));
    EspPickupMini_beforePresent(205U);
    EspPickupMini_afterPresent(205U);
    assert(!state.lit && pixel(13,76) == 0x1234U);
    assert(decodeCount == 1U);
    assert(EspPickupMini_due(265U));
    EspPickupMini_beforePresent(265U);
    EspPickupMini_afterPresent(265U);
    assert(state.lit && decodeCount == 1U);
    assert(pixel(13,76) == (uint16_t)(0x4000U | 80U));

    /* New camera frame during lit interval: its NEW underlay wins. */
    fill(0x5678U);
    EspPickupMini_markFreshFrame();
    EspPickupMini_beforePresent(275U);
    EspPickupMini_afterPresent(275U);
    assert(state.lit && pixel(13,76) == (uint16_t)(0x4000U | 80U));
    assert(decodeCount == 1U);
    assert(EspPickupMini_due(370U)); /* 270ms elapsed => OFF. */
    EspPickupMini_beforePresent(370U);
    assert(!state.lit && pixel(13,76) == 0x5678U);
    unchangedHudAndGuard(0x5678U);

    /* Elapsed 500ms: next visual begins without game/input gating. */
    assert(EspPickupMini_due(600U));
    EspPickupMini_beforePresent(600U);
    assert(state.active && state.first && !state.count);
    assert(pixel(13,76) == (uint16_t)(0x4000U | 81U));
    assert(decodeCount == 2U);
    EspPickupMini_afterPresent(600U);
    EspPickupMini_beforePresent(1100U);
    assert(!state.active && pixel(13,76) == 0x5678U);
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
    assert(pixel(13,76) == (uint16_t)(0x4000U | 83U));

    EspPickupMini_reset();
    fill(0x2222U);
    for (unsigned n = 0; n < ESP_PICKUP_MINI_FIFO_CAP + 1U; ++n)
        EspPickupMini_enqueue((uint16_t)(100U + n));
    assert(state.count == ESP_PICKUP_MINI_FIFO_CAP && !state.active);
    assert(state.pending[state.head] == 101U); /* Drop only oldest waiting. */
    EspPickupMini_beforePresent(1400U);
    assert(state.active && state.count == ESP_PICKUP_MINI_FIFO_CAP - 1U);
    assert(pixel(13,76) == (uint16_t)(0x4000U | 101U));
    unchangedHudAndGuard(0x2222U);

    puts("pickup_miniature_host: PASS FIFO rapid pickups, 500ms blink, underlay restores, live frame refresh, HUD guards, PAK busy, absent sprite, overflow cosmetic-only");
    return 0;
}
