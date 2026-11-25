#ifndef ESP_NATIVE_SPRITE_MASK_H
#define ESP_NATIVE_SPRITE_MASK_H

#include <stdint.h>

/* Packed columns are LSB-first; the last byte may contain arbitrary padding.
 * All callers validate height 1..64 and pitch ceil(height/8) first. */
static inline unsigned EspNativeSpriteMask_countByte(unsigned value) {
    value -= (value >> 1U) & 0x55U;
    value = (value & 0x33U) + ((value >> 2U) & 0x33U);
    return (value + (value >> 4U)) & 0x0fU;
}

static inline unsigned EspNativeSpriteMask_countColumn(
    const uint8_t* bits, unsigned height) {
    unsigned result = 0U, i;
    const unsigned whole = height >> 3U;
    const unsigned tail = height & 7U;
    for (i = 0U; i < whole; ++i)
        result += EspNativeSpriteMask_countByte(bits[i]);
    if (tail)
        result += EspNativeSpriteMask_countByte(
            bits[whole] & ((1U << tail) - 1U));
    return result;
}

/* Return the first row not matching opaque (0/1), or height. Consume
 * whole homogeneous bytes when aligned; never inspect padding rows. */
static inline int EspNativeSpriteMask_runEnd(
    const uint8_t* bits, int row, int height, int opaque) {
    const unsigned fill = opaque ? 255U : 0U;
    while (row < height) {
        const unsigned byte = bits[(unsigned)row >> 3U];
        if ((row & 7) == 0 && height - row >= 8 && byte == fill) {
            row += 8;
        } else {
            if (((byte >> (row & 7)) & 1U) != (unsigned)opaque)
                break;
            ++row;
        }
    }
    return row;
}
#endif
