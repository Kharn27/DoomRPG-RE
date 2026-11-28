#ifndef ESP_NATIVE_SPRITE_MASK_H
#define ESP_NATIVE_SPRITE_MASK_H

#include <stdint.h>
#include <limits.h>

/* Packed columns are LSB-first; the last byte may contain arbitrary padding.
 * All callers validate height 1..64 and pitch ceil(height/8) first. */
static inline unsigned EspNativeSpriteMask_countByte(unsigned value) {
    value -= (value >> 1U) & 0x55U;
    value = (value & 0x33U) + ((value >> 2U) & 0x33U);
    return (value + (value >> 4U)) & 0x0fU;
}

/* P4a: sum four complete bytes in one 32-bit SWAR count. Assemble
 * explicitly: no alignment, endianness, aliasing or padding assumption. */
static inline unsigned EspNativeSpriteMask_countFour(const uint8_t* bits) {
    uint32_t value = (uint32_t)bits[0] | ((uint32_t)bits[1] << 8U) |
                     ((uint32_t)bits[2] << 16U) | ((uint32_t)bits[3] << 24U);
    value -= (value >> 1U) & UINT32_C(0x55555555);
    value = (value & UINT32_C(0x33333333)) + ((value >> 2U) & UINT32_C(0x33333333));
    value = (value + (value >> 4U)) & UINT32_C(0x0f0f0f0f);
    return (unsigned)((value * UINT32_C(0x01010101)) >> 24U);
}

static inline unsigned EspNativeSpriteMask_countColumn(
    const uint8_t* bits, unsigned height) {
    unsigned result = 0U, i = 0U;
    const unsigned whole = height >> 3U;
    const unsigned tail = height & 7U;
    for (; whole - i >= 4U; i += 4U)
        result += EspNativeSpriteMask_countFour(bits + i);
    for (; i < whole; ++i)
        result += EspNativeSpriteMask_countByte(bits[i]);
    if (tail)
        result += EspNativeSpriteMask_countByte(
            bits[whole] & ((1U << tail) - 1U));
    return result;
}

/* P3n: find the first differing bit within the current byte, including
 * an unaligned start. No speculative next-byte read and no lookup table.
 * Padding may affect the intermediate mismatch, never the clamped result.
 * P4d: the immediate mismatch still exits first. For a nonzero byte
 * mismatch, isolate its lowest set bit and use a 32-bit leading-zero
 * count (Xtensa NSAU candidate); keep the portable 4/2/1 fallback. */
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
#if defined(__GNUC__) && UINT_MAX == UINT32_MAX
        row += 31 - __builtin_clz(mismatch & (0U - mismatch));
#else
        if ((mismatch & 15U) == 0U) { row += 4; mismatch >>= 4U; }
        if ((mismatch & 3U) == 0U) { row += 2; mismatch >>= 2U; }
        if ((mismatch & 1U) == 0U) ++row;
#endif
        break;
    }
    return row < height ? row : height;
}
#endif
