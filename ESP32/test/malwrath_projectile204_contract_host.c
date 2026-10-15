/*
 * Legacy CombatEntity.monsterAttacks + Combat_launchProjectile witness:
 * subtype 3 primary 15 -> sprite 204 once (Imp, already CYD PASS).
 * subtype 6 primary 15 -> sprite 204 three times (Malwrath).
 * Alternate attacks are melee/other weapons, so no sprite 204 is emitted.
 *
 * Resource census happens before the map becomes playable. The same 40-byte
 * sparse catalog record is shared by both kinds of fireball.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_monster_projectile204_contract.h"
#include "esp_native_gameplay_monster_attack_visual.h"

int main(void) {
    uint8_t subtype, alternate;
    for (subtype = 0U; subtype < 14U; ++subtype) {
        for (alternate = 0U; alternate < 2U; ++alternate) {
            const uint8_t shots =
                EspNativeMonsterProjectile204_shots(subtype, alternate);
            if (subtype == 3U || subtype == 6U) {
                assert(EspNativeMonsterProjectile204_needsResource(subtype));
                assert(EspNativeMonsterProjectile204_attackOwned(
                           subtype, alternate) == (alternate == 0U));
                assert(shots == (alternate != 0U ? 0U :
                                 subtype == 3U ? 1U : 3U));
            }
            else {
                assert(!EspNativeMonsterProjectile204_needsResource(subtype));
                assert(!EspNativeMonsterProjectile204_attackOwned(
                           subtype, alternate));
                assert(shots == 0U);
            }
        }
    }
    /* Native impact boundary: all shots visible + no still-live projectile.
     * The old final-idle dwell has no combat/RNG role. */
    for (uint8_t shots = 1U; shots <= 3U; shots += 2U) {
        for (uint8_t completed = 0U; completed < shots; ++completed)
            assert(!EspNativeGameplayMonsterAttackVisual_impactReady(
                completed, shots, 0U));
        assert(EspNativeGameplayMonsterAttackVisual_impactReady(
            shots, shots, 0U));
        assert(!EspNativeGameplayMonsterAttackVisual_impactReady(
            shots, shots, 1U));
        assert(!EspNativeGameplayMonsterAttackVisual_impactReady(
            (uint8_t)(shots + 1U), shots, 0U));
    }
    assert(!EspNativeGameplayMonsterAttackVisual_impactReady(0U, 0U, 0U));

    /* Weapon 15 subtype 10 is special: legacy sets missileAnim=0. */
    assert(!EspNativeMonsterProjectile204_needsResource(10U));
    assert(!EspNativeMonsterProjectile204_attackOwned(6U, 1U));
    puts("MONSTER_PROJECTILE204_HOST PASS Imp=1 Malwrath=3 alternate=no resource=shared impact=last-idle projectile=drain");
    return 0;
}
