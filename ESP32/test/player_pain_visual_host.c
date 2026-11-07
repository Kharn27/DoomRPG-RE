/* No mocks of combat, canvas, allocator or random are required: this
 * test compiles the REAL permanent presentation state owner unchanged. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_gameplay_player_pain_visual.h"

static uint16_t refColor(uint16_t pixel) {
    int r = ((pixel >> 11) & 31) + 8;
    int g = ((pixel >> 5) & 63) + 16;
    int b = (pixel & 31) + 8;
    return (uint16_t)(((r > 31 ? 31 : r) << 11) |
                      ((g > 63 ? 63 : g) << 5) |
                      (b > 31 ? 31 : b));
}

int main(void) {
    const EspNativePlayerPainSide sides[] = {
        ESP_NATIVE_PLAYER_PAIN_NONE, ESP_NATIVE_PLAYER_PAIN_FRONT,
        ESP_NATIVE_PLAYER_PAIN_RIGHT, ESP_NATIVE_PLAYER_PAIN_BACK,
        ESP_NATIVE_PLAYER_PAIN_LEFT
    };
    const uint8_t faces[] = {4, 4, 6, 7, 5};
    uint32_t tag0, oldTag, i;
    EspNativePlayerPainVisual_reset();
    tag0 = EspNativePlayerPainVisual_tag(1000U);
    assert(!EspNativePlayerPainVisual_isActive(1000U));
    assert(!EspNativePlayerPainVisual_stage(0U, ESP_NATIVE_PLAYER_PAIN_FRONT, 1000U));
    assert(!EspNativePlayerPainVisual_stage(7U, (EspNativePlayerPainSide)5, 1000U));
    assert(EspNativePlayerPainVisual_tag(1000U) == tag0);
    for (i = 0; i < 65536U; ++i) {
        assert(EspNativePlayerPainVisual_weaponColor((uint16_t)i) ==
               refColor((uint16_t)i));
    }
    /* Regression for the physical CYD witness: legacy ordered monster
     * movement may consume 784ms AFTER staging. Under the old clock the
     * new l.bmp face, weapon tint and shake were all already expired at
     * the first LCD present, while the independent old flash still showed.
     * COMMIT does not imply visible, and a delayed LCD must get 500ms. */
    assert(EspNativePlayerPainVisual_stage(
        901U,ESP_NATIVE_PLAYER_PAIN_RIGHT,1000U));
    assert(!EspNativePlayerPainVisual_firstPresented(1001U)); /* staged */
    assert(EspNativePlayerPainVisual_face(25U,30U,0U,1784U)==6U);
    assert(EspNativePlayerPainVisual_isActive(1784U));
    assert(!EspNativePlayerPainVisual_expiryDue(1784U));
    assert(EspNativePlayerPainVisual_commit(901U));
    assert(EspNativePlayerPainVisual_isActive(1784U));
    assert(!EspNativePlayerPainVisual_expiryDue(1784U));
    assert(EspNativePlayerPainVisual_firstPresented(1784U));
    assert(!EspNativePlayerPainVisual_firstPresented(1785U));
    assert(EspNativePlayerPainVisual_face(25U,30U,0U,2283U)==6U);
    assert(EspNativePlayerPainVisual_isActive(2283U));
    assert(!EspNativePlayerPainVisual_expiryDue(2283U));
    assert(!EspNativePlayerPainVisual_isActive(2284U));
    assert(EspNativePlayerPainVisual_expiryDue(2284U));
    assert(EspNativePlayerPainVisual_ackExpiry(2284U));
    assert(!EspNativePlayerPainVisual_firstPresented(2285U));
    /* Also cover COMMIT that waits across an even longer renderer tail:
     * no expiry or invisible-shake progression before a real publish. */
    EspNativePlayerPainVisual_reset();
    assert(EspNativePlayerPainVisual_stage(
        902U,ESP_NATIVE_PLAYER_PAIN_LEFT,3000U));
    assert(EspNativePlayerPainVisual_commit(902U));
    assert(EspNativePlayerPainVisual_isActive(9000U));
    assert(!EspNativePlayerPainVisual_expiryDue(9000U));
    assert(EspNativePlayerPainVisual_firstPresented(9000U));
    assert(EspNativePlayerPainVisual_isActive(9499U));
    assert(EspNativePlayerPainVisual_expiryDue(9500U));

    for (i = 0; i < sizeof(sides) / sizeof(sides[0]); ++i) {
        EspNativePlayerPainVisual_reset();
        assert(EspNativePlayerPainVisual_stage(10U + i, sides[i], 1000U));
        assert(EspNativePlayerPainVisual_isActive(1000U));
        assert(EspNativePlayerPainVisual_face(25U, 30U, 0U, 1000U) == faces[i]);
        assert(EspNativePlayerPainVisual_face(25U, 30U, 0U, 1499U) == faces[i]);
        assert(EspNativePlayerPainVisual_face(25U, 30U, 1U, 1200U) == 8U);
        assert(EspNativePlayerPainVisual_face(0U, 30U, 0U, 1200U) == 3U);
        assert(EspNativePlayerPainVisual_commit(10U + i));
        assert(EspNativePlayerPainVisual_firstPresented(1000U));
        assert(!EspNativePlayerPainVisual_expiryDue(1499U));
        assert(!EspNativePlayerPainVisual_isActive(1500U));
        assert(EspNativePlayerPainVisual_expiryDue(1500U));
        assert(EspNativePlayerPainVisual_face(25U, 30U, 0U, 1500U) == 0U);
        assert(EspNativePlayerPainVisual_ackExpiry(1500U));
        assert(!EspNativePlayerPainVisual_expiryDue(1501U));
        assert(!EspNativePlayerPainVisual_ackExpiry(1501U));
    }
    /* All four health tiers and gotFace precedence in the source Hud.c. */
    EspNativePlayerPainVisual_reset();
    assert(EspNativePlayerPainVisual_face(30U, 30U, 0U, 100U) == 0U);
    assert(EspNativePlayerPainVisual_face(15U, 30U, 0U, 100U) == 1U);
    assert(EspNativePlayerPainVisual_face(10U, 30U, 0U, 100U) == 2U);
    assert(EspNativePlayerPainVisual_face(7U, 30U, 0U, 100U) == 3U);
    assert(EspNativePlayerPainVisual_face(0U, 30U, 1U, 100U) == 3U);
    /* A failed transaction must restore a previously committed hit EXACTLY. */
    assert(EspNativePlayerPainVisual_stage(100U, ESP_NATIVE_PLAYER_PAIN_LEFT, 2000U));
    assert(EspNativePlayerPainVisual_commit(100U));
    assert(EspNativePlayerPainVisual_firstPresented(2000U));
    oldTag = EspNativePlayerPainVisual_tag(2075U);
    assert(EspNativePlayerPainVisual_face(20U, 30U, 0U, 2075U) == 5U);
    assert(EspNativePlayerPainVisual_stage(101U, ESP_NATIVE_PLAYER_PAIN_BACK, 2075U));
    assert(!EspNativePlayerPainVisual_stage(102U, ESP_NATIVE_PLAYER_PAIN_RIGHT, 2075U));
    assert(!EspNativePlayerPainVisual_cancel(102U));
    assert(EspNativePlayerPainVisual_cancel(101U));
    assert(EspNativePlayerPainVisual_tag(2075U) == oldTag);
    assert(EspNativePlayerPainVisual_face(20U, 30U, 0U, 2075U) == 5U);
    assert(EspNativePlayerPainVisual_stage(102U, ESP_NATIVE_PLAYER_PAIN_RIGHT, 2100U));
    assert(EspNativePlayerPainVisual_commit(102U));
    assert(EspNativePlayerPainVisual_firstPresented(2100U));
    assert(!EspNativePlayerPainVisual_cancel(102U));
    assert(EspNativePlayerPainVisual_face(20U, 30U, 0U, 2100U) == 6U);
    assert(EspNativePlayerPainVisual_isActive(2599U));
    assert(!EspNativePlayerPainVisual_isActive(2600U));
    /* Hud.c original anchors: (140,60), (80,80), (20,60), with
     * centered image flags 0x30 and source rows 36,0,18 respectively. */
    for (i = 0U; i < sizeof(sides) / sizeof(sides[0]); ++i) {
        int16_t x = -1, y = -1;
        uint16_t srcY = 999U;
        EspNativePlayerPainVisual_reset();
        assert(EspNativePlayerPainVisual_stage(200U+i, sides[i], 3000U));
        if (i <= 1U) {
            assert(!EspNativePlayerPainVisual_arrow(3000U, &x, &y, &srcY));
        } else {
            assert(EspNativePlayerPainVisual_arrow(3000U, &x, &y, &srcY));
            if (i == 2U) assert(x == 131 && y == 51 && srcY == 36U);
            if (i == 3U) assert(x == 71 && y == 71 && srcY == 0U);
            if (i == 4U) assert(x == 11 && y == 51 && srcY == 18U);
            assert(x >= 0 && x + 18 <= 160);
            assert(y >= 20 && y + 18 <= 100);
        }
        assert(EspNativePlayerPainVisual_commit(200U+i));
        assert(EspNativePlayerPainVisual_firstPresented(3000U));
        assert(!EspNativePlayerPainVisual_arrow(3500U, &x, &y, &srcY));
    }
    puts("PASS player pain: 65536 palette colors, 5 sides, 9 faces, n.bmp source rectangles, 784ms monster-tail and 6000ms pending expiry held, clock starts at physical LCD, commit/rollback/overlap and exact 500ms visible window");
    return 0;
}
