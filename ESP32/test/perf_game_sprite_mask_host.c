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
    puts("sprite mask count and run parity PASS");
    return 0;
}
