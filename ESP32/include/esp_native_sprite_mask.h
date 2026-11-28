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

/* P3n: find the first differing bit within the current byte, including
 * an unaligned start. No speculative next-byte read and no lookup table.
 * Padding may affect the intermediate mismatch, never the clamped result.
 * The common immediate mismatch exits before the bounded 4/2/1 search. */
static inline int EspNativeSpriteMask_runEnd(
    const uint8_t* bits, int row, int height, int opaque) {
    const unsigned fill = opaque ? 255U : 0U;
    while (row < height) {
        const unsigned offset = (unsigned)row & 7U;
        unsigned mismatch = (bits[(unsigned)row >> 3U] ^ fill) >> offset;
        if (mismatch == 0U) {
            row += (int)(8U - offset);
            continue;
        }
        if ((mismatch & 1U) != 0U) return row;
        if ((mismatch & 15U) == 0U) { row += 4; mismatch >>= 4U; }
        if ((mismatch & 3U) == 0U) { row += 2; mismatch >>= 2U; }
        if ((mismatch & 1U) == 0U) ++row;
        break;
    }
    return row < height ? row : height;
}
#endif
