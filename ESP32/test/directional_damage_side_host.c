/* Pure projection fixture: real native presentation-side contract, no SDL. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_action_engine.h"

typedef struct Facing_s {
    int32_t stepX;
    int32_t stepY;
    int32_t positions[4][2]; /* FRONT RIGHT BACK LEFT */
} Facing;

static const Facing cases[4] = {
    { 64,  0, { {+64, 0}, {0, +64}, {-64, 0}, {0, -64} } },
    {  0,-64, { {0, -64}, {+64, 0}, {0, +64}, {-64, 0} } },
    {-64,  0, { {-64, 0}, {0, -64}, {+64, 0}, {0, +64} } },
    {  0, 64, { {0, +64}, {-64, 0}, {0, -64}, {+64, 0} } }
};

/* A pure pixel-leasing fixture for the exact 160x80 gameplay viewport:
 * the red border is the lower layer, then the translucent side. Snapshots
 * must restore LIFO (side -> red -> native world). No other framebuffer. */
static uint16_t fixture[160U * 120U];
static uint16_t original[160U * 120U];
static uint16_t borderPixels[944U];
static uint16_t cuePixels[1600U];

static void paintBorder(unsigned int mode) {
    unsigned int x, y, n = 0U;
    for (y = 20U; y < 100U; ++y) {
        for (x = 0U; x < 160U; ++x) {
            const int edge = y < 22U || y >= 98U ||
                             x < 2U || x >= 158U;
            uint16_t* p;
            if (!edge) continue;
            p = &fixture[y * 160U + x];
            if (mode == 0U) {
                assert(n < 944U);
                borderPixels[n] = *p;
                *p = 0xb800U;
            } else {
                assert(n < 944U);
                *p = borderPixels[n];
            }
            ++n;
        }
    }
    assert(n == 944U);
}

static void testOverlayDuringRedBorder(void) {
    unsigned int side;
    for (side = 1U; side <= 4U; ++side) {
        unsigned int x, y, x0 = 0U, y0 = 20U;
        unsigned int width = 160U, height = 10U;
        unsigned int n = 0U, pulse;
        if (side == 2U) { x0 = 150U; width = 10U; height = 80U; }
        if (side == 3U) y0 = 90U;
        if (side == 4U) { width = 10U; height = 80U; }
        for (y = 0U; y < 120U; ++y) {
            for (x = 0U; x < 160U; ++x) {
                fixture[y * 160U + x] =
                    (uint16_t)((y * 59U + x * 13U) & 0xffffU);
            }
        }
        memcpy(original, fixture, sizeof(fixture));
        paintBorder(0U);
        for (pulse = 0U; pulse < 2U; ++pulse) {
            n = 0U;
            for (y = y0; y < y0 + height; ++y) {
                for (x = x0; x < x0 + width; ++x) {
                    uint16_t* p = &fixture[y * 160U + x];
                    assert(n < 1600U);
                    cuePixels[n++] = *p;
                    *p = EspNativeGameplayDamageCue_blend565(*p, 0xfa20U);
                }
            }
            assert(n == width * height);
            /* The overlay really appears OVER the already-painted red,
             * not as an isolated replacement or a flash scheduled later. */
            assert(fixture[y0 * 160U + x0] ==
                   EspNativeGameplayDamageCue_blend565(0xb800U, 0xfa20U));
            n = 0U;
            for (y = y0; y < y0 + height; ++y)
                for (x = x0; x < x0 + width; ++x)
                    fixture[y * 160U + x] = cuePixels[n++];
            assert(fixture[y0 * 160U + x0] == 0xb800U);
        }
        paintBorder(1U);
        assert(memcmp(fixture, original, sizeof(fixture)) == 0);
    }
}
int main(void) {
    unsigned int facing;
    unsigned int side;
    unsigned int probes = 0U;
    const int32_t playerX = 736;
    const int32_t playerY = 1504;
    for (facing = 0U; facing < 4U; ++facing) {
        for (side = 0U; side < 4U; ++side) {
            const int32_t x = playerX + cases[facing].positions[side][0];
            const int32_t y = playerY + cases[facing].positions[side][1];
            const EspNativeGameplayDamageSide actual =
                EspNativeGameplayDamageSide_fromWorld(
                    playerX, playerY, cases[facing].stepX,
                    cases[facing].stepY, x, y);
            assert(actual == (EspNativeGameplayDamageSide)(side + 1U));
            ++probes;
        }
    }
    /* Directly behind is always the BOTTOM side, even after a rotation. */
    assert(EspNativeGameplayDamageSide_fromWorld(
        736, 1504, 64, 0, 100, 1504) ==
        ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK);
    /* Diagonal ties prefer FRONT/BACK over LEFT/RIGHT. */
    assert(EspNativeGameplayDamageSide_fromWorld(
        736, 1504, 64, 0, 800, 1568) ==
        ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT);
    assert(EspNativeGameplayDamageSide_fromWorld(
        736, 1504, 64, 0, 672, 1440) ==
        ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK);
    /* Oblique attackers choose their dominant projected direction. */
    assert(EspNativeGameplayDamageSide_fromWorld(
        736, 1504, 64, 0, 800, 1696) ==
        ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT);
    assert(EspNativeGameplayDamageSide_fromWorld(
        736, 1504, 64, 0, 800, 1312) ==
        ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT);
    assert(EspNativeGameplayDamageSide_fromWorld(
        736, 1504, 0, 0, 800, 1504) ==
        ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_NONE);
    assert(EspNativeGameplayDamageSide_fromWorld(
        736, 1504, 1, 0, 800, 1504) ==
        ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_NONE);
    assert(EspNativeGameplayDamageSide_fromWorld(
        736, 1504, 64, 0, 736, 1504) ==
        ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_NONE);
    {
        EspNativeGameplayDamageCueQueue fifo = {0};
        EspNativeGameplayDamageCue popped;
        unsigned int i;
        const EspNativeGameplayDamageSide sides[4] = {
            ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT,
            ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT,
            ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK,
            ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_RIGHT
        };
        /* Four different real attackers: order and identity, not last-wins. */
        for (i = 0U; i < 4U; ++i)
            assert(EspNativeGameplayDamageCueQueue_push(
                &fifo, 100U + i, (uint16_t)(220U + i), sides[i]));
        assert(fifo.count == 4U);
        for (i = 0U; i < 4U; ++i) {
            assert(EspNativeGameplayDamageCueQueue_pop(&fifo, &popped));
            assert(popped.probe == 100U + i);
            assert(popped.attackerSprite == 220U + i);
            assert(popped.side == (uint8_t)sides[i]);
        }
        assert(fifo.count == 0U);
        assert(!EspNativeGameplayDamageCueQueue_pop(&fifo, &popped));

        /* Exhaust the queue and prove fail-closed overflow; then wrap. */
        for (i = 0U; i < ESP_NATIVE_GAMEPLAY_DAMAGE_CUE_QUEUE_CAP; ++i)
            assert(EspNativeGameplayDamageCueQueue_push(
                &fifo, i + 1U, (uint16_t)(300U + i),
                ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK));
        assert(!EspNativeGameplayDamageCueQueue_push(
            &fifo, 1234U, 999U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_FRONT));
        for (i = 0U; i < 3U; ++i) {
            assert(EspNativeGameplayDamageCueQueue_pop(&fifo, &popped));
            assert(popped.probe == i + 1U);
        }
        for (i = 0U; i < 3U; ++i)
            assert(EspNativeGameplayDamageCueQueue_push(
                &fifo, 501U + i, (uint16_t)(501U + i),
                ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT));
        for (i = 3U; i < ESP_NATIVE_GAMEPLAY_DAMAGE_CUE_QUEUE_CAP; ++i) {
            assert(EspNativeGameplayDamageCueQueue_pop(&fifo, &popped));
            assert(popped.probe == i + 1U);
        }
        for (i = 0U; i < 3U; ++i) {
            assert(EspNativeGameplayDamageCueQueue_pop(&fifo, &popped));
            assert(popped.probe == 501U + i);
            assert(popped.side == ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_LEFT);
        }
        assert(fifo.count == 0U);
        assert(!EspNativeGameplayDamageCueQueue_push(
            &fifo, 0U, 5U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK));
        assert(!EspNativeGameplayDamageCueQueue_push(
            &fifo, 600U, 0xffffU, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_BACK));
        assert(!EspNativeGameplayDamageCueQueue_push(
            &fifo, 600U, 5U, ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_NONE));
    }
    testOverlayDuringRedBorder();
    puts("DAMAGECUE host: cardinal=16/16 behind=ok diagonal=ok oblique=ok invalid=ok fifo-order=ok wrap=ok full=guarded red+side=concurrent 2-pulses=exact restore=LIFO");
    return 0;
}
