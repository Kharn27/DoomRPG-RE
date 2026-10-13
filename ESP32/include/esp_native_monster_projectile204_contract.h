#ifndef DOOMRPG_ESP32_NATIVE_MONSTER_PROJECTILE204_CONTRACT_H
#define DOOMRPG_ESP32_NATIVE_MONSTER_PROJECTILE204_CONTRACT_H

#include <stdint.h>

/*
 * Legacy CombatEntity.monsterAttacks:
 *  subtype 3 primary -> weapon 15, one shot (Imp)
 *  subtype 6 primary -> weapon 15, three shots (Malwrath/Cacodemon)
 * Legacy Combat_launchProjectile(weapon 15) emits logical sprite 204 for
 * every subtype except 10. This bounded milestone explicitly owns 3 and 6;
 * other weapon-15 families remain deferred until validated independently.
 *
 * Map topology has subtype but not an individual monster attack-state flag:
 * prime this immutable runtime-generated resource if either family is in
 * the BSP, then use alternateAttack==0 for per-probe authorization.
 */
static inline int EspNativeMonsterProjectile204_needsResource(
    uint8_t subtype) {
    return subtype == 3U || subtype == 6U;
}

static inline int EspNativeMonsterProjectile204_attackOwned(
    uint8_t subtype, uint8_t alternateAttack) {
    return alternateAttack == 0U &&
           EspNativeMonsterProjectile204_needsResource(subtype);
}

static inline uint8_t EspNativeMonsterProjectile204_shots(
    uint8_t subtype, uint8_t alternateAttack) {
    if (!EspNativeMonsterProjectile204_attackOwned(
            subtype, alternateAttack)) return 0U;
    return subtype == 6U ? 3U : 1U;
}

#endif
