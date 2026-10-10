#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_native_level_gate.h"

/* Hardware witness: Junction event43 command39 arg1=00040031,
 * arg2=0000000f. completedLevels is indexed by legacy map ID - 1. */
static void check(uint32_t arg1, uint16_t completed,
                  uint16_t expectedMask, uint8_t expectedWarning) {
    uint16_t mask = 0xffffU;
    uint8_t warning = 0xffU;
    assert(EspNativeLevelGate_evaluate(
        arg1, completed, &mask, &warning));
    assert(mask == expectedMask);
    assert(warning == expectedWarning);
}

int main(void) {
    uint16_t mask;
    uint8_t warning;
    const uint32_t junction = 0x00040031UL;
    check(junction, 0U, 0x07f8U, 1U);
    check(junction, 1U << 2U, 0x07f8U, 1U); /* Sector 1 alone */
    check(junction, 1U << 3U, 0x07f8U, 0U); /* Sector 2 */
    check(junction, 1U << 10U, 0x07f8U, 0U); /* last valid */
    check(junction, (1U << 2U) | (1U << 3U), 0x07f8U, 0U);
    check(0x00010031UL, 0U, 0x07ffU, 1U);
    check(0x000b0031UL, (1U << 10U), (1U << 10U), 0U);
    check(0x000b0031UL, 1U << 9U, (1U << 10U), 1U);
    assert(!EspNativeLevelGate_evaluate(0x00000031UL, 0U, &mask, &warning));
    assert(mask == 0U && warning == 0U);
    assert(!EspNativeLevelGate_evaluate(0x000c0031UL, 0U, &mask, &warning));
    assert(!EspNativeLevelGate_evaluate(junction, 0x8000U, &mask, &warning));
    assert(!EspNativeLevelGate_evaluate(junction, 0U, NULL, &warning));
    puts("level_gate_native_host: PASS");
    return 0;
}
