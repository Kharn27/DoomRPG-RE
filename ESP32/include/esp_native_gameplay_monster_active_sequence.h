#ifndef ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVE_SEQUENCE_H
#define ESP_NATIVE_GAMEPLAY_MONSTER_ACTIVE_SEQUENCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

/* The service processes at most one living member's complete
 * attack-or-movement transaction per tick. All other ordered members
 * remain in the current turn without a synthetic timer. */
#define ESP_NATIVE_GAMEPLAY_ACTIVESEQ_MEMBER_BUDGET 1U

static inline int EspNativeGameplayMonsterActiveSequence_shouldYield(
    uint32_t handled, uint32_t remaining) {
    return remaining != 0U &&
           handled >= ESP_NATIVE_GAMEPLAY_ACTIVESEQ_MEMBER_BUDGET;
}

/* The pending ordered prefix owns WORLD input even if no attack animation
 * is active. This does not alter the turn, RNG or SAVE model. */
int EspNativeGameplayMonsterActiveSequence_isBusy(void);

/* Permanent movement-domain composition boundary. Expands one producer turn
 * across the ordered active-monster set, while each member still executes the
 * unchanged Movement planner/probe/publish transaction to completion before the
 * next member is selected. */
void EspNativeGameplayMonsterActiveSequence_service(struct DoomRPG_s* doomRpg);

#ifdef __cplusplus
}
#endif

#endif
