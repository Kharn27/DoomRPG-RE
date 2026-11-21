#ifndef DOOMRPG_ESP32_NATIVE_FONT_TINT_H
#define DOOMRPG_ESP32_NATIVE_FONT_TINT_H

#include <stdint.h>

/* Pure RGB565 font ink modulation. The first stage is exactly the preexisting
 * native texture colorMod. A modest 25% luminance lift is applied ONLY when
 * an actual SD font is tinted; original PAK/other images and unmodulated
 * metallic artwork stay bit-identical. No white overlay, heap, LUT or alpha.
 *
 * Black outline remains black; zeroed theme channels stay zero. The blitter
 * must check the magenta colorkey BEFORE invoking this helper. */
static inline uint16_t EspNativeFontTint_apply(uint16_t source,
                                               uint8_t red, uint8_t green,
                                               uint8_t blue,
                                               uint8_t sdFont) {
    uint32_t r = (((uint32_t)source >> 11U) & 31U) * red / 255U;
    uint32_t g = (((uint32_t)source >> 5U) & 63U) * green / 255U;
    uint32_t b = ((uint32_t)source & 31U) * blue / 255U;

    if (sdFont != 0U && (red != 255U || green != 255U || blue != 255U)) {
        r = (r * 5U + 2U) / 4U;
        g = (g * 5U + 2U) / 4U;
        b = (b * 5U + 2U) / 4U;
        if (r > 31U) r = 31U;
        if (g > 63U) g = 63U;
        if (b > 31U) b = 31U;
    }

    return (uint16_t)((r << 11U) | (g << 5U) | b);
}

#endif /* DOOMRPG_ESP32_NATIVE_FONT_TINT_H */
