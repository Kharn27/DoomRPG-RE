#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_native_gameplay_barrel_radius.h"

/*
 * Regression for CYD barrel at sprite 186 / tile 241:
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
    /* Single-barrel monster damage is applied after its one blast word.
     * Multi-barrel without monsters preserves the full consecutive preview.
     * Multi-barrel WITH monsters previews only the first word; subsequent
     * words must be drawn after earlier monster consequence RNG. */
    assert(EspNativeGameplayBarrelRadius_previewWords(1U, 0U) == 1U);
    assert(EspNativeGameplayBarrelRadius_previewWords(1U, 1U) == 1U);
    assert(EspNativeGameplayBarrelRadius_previewWords(2U, 0U) == 2U);
    assert(EspNativeGameplayBarrelRadius_previewWords(16U, 0U) == 16U);
    assert(EspNativeGameplayBarrelRadius_previewWords(2U, 1U) == 1U);
    assert(EspNativeGameplayBarrelRadius_previewWords(16U, 8U) == 1U);
    /* Real incident: chain=1, but it may splash adjacent monsters as well
     * as the player. Death byte must be reserved before monster consequence
     * RNG. A subsequent barrel makes that ordering unsupported for now. */
    assert(EspNativeGameplayBarrelRadius_terminalDeathOwned(1U));
    assert(!EspNativeGameplayBarrelRadius_terminalDeathOwned(0U));
    assert(!EspNativeGameplayBarrelRadius_terminalDeathOwned(2U));
    assert(!EspNativeGameplayBarrelRadius_terminalDeathOwned(16U));
    puts("barrel_radius_inert_line_host: PASS type0=all type12=3,4 only, blast RNG preview modes, lethal single-blast boundary");
    return 0;
}
