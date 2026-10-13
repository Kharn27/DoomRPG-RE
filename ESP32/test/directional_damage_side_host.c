/* Pure projection fixture: real native presentation-side contract, no SDL. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

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
    puts("DAMAGECUE host: cardinal=16/16 behind=ok diagonal=ok oblique=ok invalid=ok fifo-order=ok wrap=ok full=guarded");
    return 0;
}
