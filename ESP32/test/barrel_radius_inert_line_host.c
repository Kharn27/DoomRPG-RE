#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_native_gameplay_barrel_radius.h"

/*
 * Regression for CYD Sector-1(?) barrel at sprite 186 / tile 241:
 * radius neighbor tile 177 contains linked line 288, type=0/subtype=1.
 * The legacy leaves that line non-hurtable. The previous native preflight
 * handled only type=0/subtype=255 and rejected the entire explosion.
 */
int main(void) {
    unsigned int subtype;
    for (subtype = 0U; subtype <= UINT8_MAX; ++subtype) {
        assert(EspNativeGameplayBarrelRadius_lineNoop(
                   0U, (uint8_t)subtype));
    }
    assert(EspNativeGameplayBarrelRadius_lineNoop(12U, 3U));
    assert(EspNativeGameplayBarrelRadius_lineNoop(12U, 4U));

    /* Other destructive and gameplay-bearing families remain fail-closed. */
    assert(!EspNativeGameplayBarrelRadius_lineNoop(12U, 1U));
    assert(!EspNativeGameplayBarrelRadius_lineNoop(12U, 2U));
    assert(!EspNativeGameplayBarrelRadius_lineNoop(12U, 0U));
    assert(!EspNativeGameplayBarrelRadius_lineNoop(1U, 4U));
    assert(!EspNativeGameplayBarrelRadius_lineNoop(2U, 0U));
    assert(!EspNativeGameplayBarrelRadius_lineNoop(7U, 1U));
    assert(!EspNativeGameplayBarrelRadius_lineNoop(10U, 0U));
    puts("barrel_radius_inert_line_host: PASS type0=all type12=3,4 only");
    return 0;
}
