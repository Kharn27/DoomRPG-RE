/* Cooperative ordinal budget must not replay any already-consumed member.
 * This pure model is secondary to the full firmware/ELF test; the real
 * sequencer's source uses precisely this inline scheduling predicate. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_gameplay_monster_active_sequence.h"

int main(void) {
    uint32_t ordinal = 0U;
    uint32_t ticks = 0U;
    uint32_t visits = 0U;
    const uint32_t limit = 10U;
    assert(ESP_NATIVE_GAMEPLAY_ACTIVESEQ_MEMBER_BUDGET == 1U);
    assert(!EspNativeGameplayMonsterActiveSequence_shouldYield(0U, 9U));
    assert(EspNativeGameplayMonsterActiveSequence_shouldYield(1U, 9U));
    assert(!EspNativeGameplayMonsterActiveSequence_shouldYield(1U, 0U));
    assert(EspNativeGameplayMonsterActiveSequence_shouldYield(2U, 1U));
    while (ordinal < limit) {
        uint32_t handled = 0U;
        ++ticks;
        while (ordinal < limit) {
            ++visits;
            ++ordinal;
            ++handled;
            if (EspNativeGameplayMonsterActiveSequence_shouldYield(
                    handled, limit - ordinal)) break;
        }
    }
    assert(ticks == limit && visits == limit && ordinal == limit);
    puts("monster_active_sequence_budget_host: PASS ordered resume, one member per tick, no replay, no final yield");
    return 0;
}
