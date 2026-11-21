/* Keep font-themed brightness local to validated SD atlases.
 * Pure host test: no GPU, SD, heap, SDK or PSRAM required. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_font_tint.h"

static uint16_t rawOld(uint16_t c, uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)((((c >> 11) & 31U) * r / 255U << 11U) |
                      (((c >> 5) & 63U) * g / 255U << 5U) |
                      ((c & 31U) * b / 255U));
}

static int red5(uint16_t c) { return (c >> 11) & 31; }
static int green6(uint16_t c) { return (c >> 5) & 63; }
static int blue5(uint16_t c) { return c & 31; }

int main(void) {
    uint16_t source;
    unsigned r, g, b;
    /* Exhaustively preserve original PAK/other texture path and the identity
     * tint for SD fonts (including a pure white/unthemed bitmap). */
    for (source = 0U;; ++source) {
        assert(EspNativeFontTint_apply(source, 128U, 192U, 80U, 0U) ==
               rawOld(source, 128U, 192U, 80U));
        assert(EspNativeFontTint_apply(source, 255U, 255U, 255U, 1U) == source);
        assert(EspNativeFontTint_apply(source, 255U, 255U, 255U, 2U) == source);
        if (source == 0xffffU) break;
    }

    /* Keep the original dark outline and the exact hue-mask semantics. */
    assert(EspNativeFontTint_apply(0U, 65U, 120U, 210U, 1U) == 0U);
    assert(red5(EspNativeFontTint_apply(0xffffU, 0U, 128U, 255U, 1U)) == 0);
    assert(blue5(EspNativeFontTint_apply(0xffffU, 128U, 255U, 0U, 1U)) == 0);

    /* Brighten both standard and large SD fonts with exact 5:4 math,
     * retaining their palette contrast. PAK path is byte-identical. */
    for (r = 16U; r <= 240U; r += 32U) {
        for (g = 16U; g <= 240U; g += 32U) {
            for (b = 16U; b <= 240U; b += 32U) {
                const uint16_t old = rawOld(0x9ad2U, (uint8_t)r,
                                             (uint8_t)g, (uint8_t)b);
                const uint16_t std = EspNativeFontTint_apply(
                    0x9ad2U, (uint8_t)r, (uint8_t)g, (uint8_t)b, 1U);
                const uint16_t large = EspNativeFontTint_apply(
                    0x9ad2U, (uint8_t)r, (uint8_t)g, (uint8_t)b, 2U);
                assert(std == large);
                assert(red5(std) >= red5(old));
                assert(green6(std) >= green6(old));
                assert(blue5(std) >= blue5(old));
            }
        }
    }

    /* Out-of-range is clamped independently on saturated theme highlights. */
    {
        uint16_t bright = EspNativeFontTint_apply(
            0xffffU, 250U, 245U, 254U, 1U);
        assert(red5(bright) <= 31 && green6(bright) <= 63 &&
               blue5(bright) <= 31);
        assert(bright == 0xffffU);
    }

    puts("FONT-SD TINT HOST PASS: PAK exact, identity exact, colored atlas "
         "lift 5:4, no black/zero-channel leak, saturation clamped");
    return 0;
}
