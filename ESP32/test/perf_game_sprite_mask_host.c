#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_sprite_mask.h"

/* Independent original per-bit reference: compare every starting row,
 * both polarities, cumulative opaque cursor and partial padding. */
static void check(const uint8_t* bits, int height) {
    unsigned count = 0U;
    int row, opaque;
    for (row = 0; row < height; ++row)
        count += (bits[row / 8] >> (row & 7)) & 1U;
    assert(EspNativeSpriteMask_countColumn(bits, (unsigned)height) == count);
    for (row = 0; row <= height; ++row) {
        for (opaque = 0; opaque <= 1; ++opaque) {
            int end = row;
            while (end < height &&
                   ((bits[end / 8] >> (end & 7)) & 1U) == (unsigned)opaque)
                ++end;
            assert(EspNativeSpriteMask_runEnd(bits, row, height, opaque) == end);
        }
    }
    row = 0;
    unsigned cursor = 0U;
    while (row < height) {
        row = EspNativeSpriteMask_runEnd(bits, row, height, 0);
        if (row == height) break;
        const int end = EspNativeSpriteMask_runEnd(bits, row, height, 1);
        assert(end > row);
        cursor += (unsigned)(end - row);
        row = end;
    }
    assert(cursor == count);
}

int main(void) {
    uint8_t bits[8];
    for (unsigned pattern = 0U; pattern < 65536U; ++pattern) {
        bits[0] = (uint8_t)pattern;
        bits[1] = (uint8_t)(pattern >> 8U);
        for (int height = 1; height <= 16; ++height)
            check(bits, height);
    }
    uint32_t rng = 0x5b7a23d1U;
    for (unsigned sample = 0U; sample < 2048U; ++sample) {
        for (unsigned i = 0U; i < 8U; ++i) {
            rng = rng * 1664525U + 1013904223U;
            bits[i] = (uint8_t)(rng >> 24U);
        }
        for (int height = 1; height <= 64; ++height)
            check(bits, height);
    }
    for (unsigned byte = 0U; byte < 256U; ++byte) {
        for (unsigned i = 0U; i < 8U; ++i) bits[i] = (uint8_t)byte;
        for (int height = 1; height <= 64; ++height)
            check(bits, height);
    }
    /* Exercise the four-byte SWAR path at every alignment and tail.
     * Independent per-bit reference; no production helper in expected count. */
    for (unsigned sample = 0U; sample < 4096U; ++sample) {
        uint8_t raw[12];
        for (unsigned i = 0U; i < sizeof raw; ++i) {
            rng = rng * 1664525U + 1013904223U;
            raw[i] = (uint8_t)(rng >> 24U);
        }
        for (unsigned align = 0U; align < 4U; ++align) {
            for (unsigned height = 1U; height <= 64U; ++height) {
                unsigned reference = 0U;
                for (unsigned row = 0U; row < height; ++row)
                    reference += (raw[align + row / 8U] >> (row & 7U)) & 1U;
                assert(EspNativeSpriteMask_countColumn(raw + align, height) ==
                       reference);
            }
        }
    }
    /* P4d: long homogeneous runs, a single differing bit at each
     * possible position, every partial height, both polarities and
     * byte alignments. Each check compares ALL starting rows. */
    for (unsigned align = 0U; align < 4U; ++align) {
        for (unsigned polarity = 0U; polarity < 2U; ++polarity) {
            for (unsigned bit = 0U; bit < 64U; ++bit) {
                uint8_t raw[12];
                for (unsigned i = 0U; i < sizeof raw; ++i)
                    raw[i] = polarity ? 255U : 0U;
                raw[align + bit / 8U] ^= (uint8_t)(1U << (bit & 7U));
                for (int height = 1; height <= 64; ++height)
                    check(raw + align, height);
            }
        }
    }
    puts("sprite mask count and run parity PASS (including 1048576 unaligned/tail counts and 32768 single-transition columns)");
    return 0;
}
