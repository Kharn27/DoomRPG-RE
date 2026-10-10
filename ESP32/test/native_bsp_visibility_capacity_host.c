#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_bsp_visibility.h"

/* CYD playtest: /level02.bsp (mapId=3) contains 283 BSP nodes.
 * A 256-bit visibility mask used to fail in buildForView before a sprite
 * could be counted, then leave ENGINESESSION failed at SMALL-COLD stage 5.
 * Verify the fixed footprint still covers the real node 282 and the maximum.
 */
_Static_assert(ESP_NATIVE_BSP_VISIBILITY_MAX_NODES >= 283U,
               "Sector 2 BSP needs at least 283 native visibility nodes");
_Static_assert((ESP_NATIVE_BSP_VISIBILITY_MAX_NODES % 32U) == 0U,
               "visibility leaf mask must contain integral uint32 words");
_Static_assert(ESP_NATIVE_BSP_VISIBILITY_LEAF_WORDS * 32U ==
                   ESP_NATIVE_BSP_VISIBILITY_MAX_NODES,
               "all bounded node IDs must have a leaf bit");
_Static_assert(sizeof(EspNativeBspVisibilityState) <= 1024U,
               "keep the complete renderer visibility scratch bounded");

static void markAndRead(EspNativeBspVisibilityState* state, uint32_t node) {
    uint32_t word = node >> 5U;
    uint32_t bit = 1UL << (node & 31U);
    assert(node < ESP_NATIVE_BSP_VISIBILITY_MAX_NODES);
    assert(word < ESP_NATIVE_BSP_VISIBILITY_LEAF_WORDS);
    state->visibleLeaves[word] |= bit;
    assert((state->visibleLeaves[word] & bit) != 0U);
}

int main(void) {
    EspNativeBspVisibilityState visibility;
    memset(&visibility, 0, sizeof(visibility));
    markAndRead(&visibility, 0U);
    markAndRead(&visibility, 255U);
    markAndRead(&visibility, 256U);
    markAndRead(&visibility, 282U);
    markAndRead(&visibility, ESP_NATIVE_BSP_VISIBILITY_MAX_NODES - 1U);
    assert(visibility.visibleLeaves[8] & (1UL << 26U));
    puts("native_bsp_visibility_capacity_host: PASS map3=283 bounded=1024");
    return 0;
}
