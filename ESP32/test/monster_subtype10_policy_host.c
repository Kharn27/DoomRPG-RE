/* Pure original-branch oracle; no graphics, heap, SDL or fake RNG draws. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_gameplay_monster_subtype10_policy.h"

int main(void) {
    unsigned int i;
    unsigned int raises = 0U;
    for (i = 0; i < 256U; ++i) {
        const int got = EspNativeSubtype10_wantsRaise((uint8_t)i);
        assert(got == (i < 84U));
        raises += (unsigned int)got;
    }
    assert(raises == 84U);
    assert(ESP_NATIVE_SUBTYPE10_RAISE_OCCUPANCY_MASK == 16263U);
    assert(!EspNativeSubtype10_candidateEligible(0, 0, 1, 0, 0));
    assert(EspNativeSubtype10_candidateEligible(64, 0, 1, 0, 0));
    assert(EspNativeSubtype10_candidateEligible(-128, 0, 1, 0, 0));
    assert(EspNativeSubtype10_candidateEligible(0, 128, 1, 0, 0));
    assert(EspNativeSubtype10_candidateEligible(64, 64, 1, 0, 0));
    assert(!EspNativeSubtype10_candidateEligible(129, 0, 1, 0, 0));
    assert(!EspNativeSubtype10_candidateEligible(128, 1, 1, 0, 0));
    assert(!EspNativeSubtype10_candidateEligible(64, 0, 0, 0, 0));
    assert(!EspNativeSubtype10_candidateEligible(64, 0, 1, 1, 0));
    assert(!EspNativeSubtype10_candidateEligible(64, 0, 1, 0, 1));
    assert(!EspNativeSubtype10_candidateEligible(40000, 40000, 1, 0, 0));
    puts("monster_subtype10_policy_host: PASS 84/256 RNG gate, radius, corpse/target/occupancy");
    return 0;
}
