#ifndef DOOMRPG_ESP32_NATIVE_MONSTER_DEATH_CONSEQUENCE_H
#define DOOMRPG_ESP32_NATIVE_MONSTER_DEATH_CONSEQUENCE_H

#include <stdint.h>

/* MONSTER-DEATH-P1: exactly the legacy Entity_died()/Game_hurtEntityAt()
 * audio priorities. Sound IDs have ALREADY been chosen by the monster RNG
 * owner: this pure policy must not draw again or publish speculative sound.
 * Gib is a distinct death branch (5091/P3), not a normal death (P5).
 * An impossible pain+gib combination fails closed.
 */
static inline uint8_t EspNativeMonsterDeathConsequence_priority(
    uint8_t lethal, uint8_t gib) {
    if (gib != 0U && lethal == 0U) return 0U;
    return lethal != 0U ? (gib != 0U ? 3U : 5U) : 2U;
}

#endif
