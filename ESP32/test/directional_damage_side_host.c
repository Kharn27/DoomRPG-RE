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

/* One red 2px perimeter and exactly one translucent INNER 10px zone.
 * The two use the SAME ON/OFF timeline and clean world baseline. */
static uint16_t fixture[160U * 120U];
static uint16_t original[160U * 120U];
static uint16_t borderPixels[944U];
static uint16_t extraPixels[1560U];
static uint16_t blend565(uint16_t world) {
    return (uint16_t)(((world & 0xf7deU) >> 1U) +
                      ((0xfa20U & 0xf7deU) >> 1U));
}
static int isOuter(unsigned x, unsigned y) {
    return y < 22U || y >= 98U || x < 2U || x >= 158U;
}
static int isInner(unsigned x, unsigned y, unsigned side) {
    switch (side) {
    case 1U: return y >= 22U && y < 32U && x >= 2U && x < 158U;
    case 2U: return x >= 148U && x < 158U && y >= 22U && y < 98U;
    case 3U: return y >= 88U && y < 98U && x >= 2U && x < 158U;
    case 4U: return x >= 2U && x < 12U && y >= 22U && y < 98U;
    default: return 0;
    }
}
static void testUnifiedBlinkGeometry(void) {
    for (unsigned side = 0U; side <= 4U; ++side) {
        unsigned outerAt = 0U, innerAt = 0U;
        for (unsigned y = 0U; y < 120U; ++y)
            for (unsigned x = 0U; x < 160U; ++x)
                fixture[y * 160U + x] =
                    (uint16_t)((x * 13U + y * 59U) & 0xffffU);
        memcpy(original, fixture, sizeof(fixture));
        for (unsigned pulse = 0U; pulse < 3U; ++pulse) {
            outerAt = innerAt = 0U;
            for (unsigned y = 20U; y < 100U; ++y)
                for (unsigned x = 0U; x < 160U; ++x) {
                    uint16_t* p = &fixture[y * 160U + x];
                    if (isOuter(x, y)) {
                        assert(outerAt < 944U);
                        if (pulse == 0U) borderPixels[outerAt] = *p;
                        *p = 0xb800U;
                        ++outerAt;
                    }
                    else if (isInner(x, y, side)) {
                        assert(innerAt < 1560U);
                        if (pulse == 0U) extraPixels[innerAt] = *p;
                        *p = blend565(extraPixels[innerAt]);
                        ++innerAt;
                    }
                }
            assert(outerAt == 944U);
            assert(innerAt == (side == 0U ? 0U :
                             side == 1U || side == 3U ? 1560U : 760U));
            assert(fixture[20U * 160U + 80U] == 0xb800U);
            if (side != 0U) {
                const unsigned x = side == 2U ? 148U :
                                   side == 4U ? 2U : 80U;
                const unsigned y = side == 1U ? 31U :
                                   side == 3U ? 88U : 50U;
                assert(fixture[y * 160U + x] ==
                       blend565(original[y * 160U + x]));
            }
            outerAt = innerAt = 0U;
            for (unsigned y = 20U; y < 100U; ++y)
                for (unsigned x = 0U; x < 160U; ++x) {
                    uint16_t* p = &fixture[y * 160U + x];
                    if (isOuter(x, y)) *p = borderPixels[outerAt++];
                    else if (isInner(x, y, side)) *p = extraPixels[innerAt++];
                }
            /* Every OFF frame restores the EXACT original world. */
            assert(memcmp(fixture, original, sizeof(fixture)) == 0);
        }
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
    testUnifiedBlinkGeometry();
    puts("DAMAGEBORDER host: cardinal=16/16 behind=ok diagonal=ok oblique=ok invalid=ok single-500ms-clock=3-pulses, opaque-2px + translucent-inner-10px, 4-sides, 3 exact-restores");
    return 0;
}
