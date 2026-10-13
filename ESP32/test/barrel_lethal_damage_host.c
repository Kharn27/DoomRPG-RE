/* Regression for a barrel explosion that exceeds remaining HP and armor.
 * Use the real 52-byte native player owner. Death-arm itself is a distinct
 * render/HUD transaction and remains gated by the firmware build + CYD test.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_player_state.h"

/* New-game-only save hook: the test restores an already-active checkpoint. */
void EspNativeGameplaySave_clearTransitionRoute(void) {}

static EspNativeGameplayPlayerState playerWith(uint8_t hp, uint8_t armor) {
    EspNativeGameplayPlayerState p;
    memset(&p, 0, sizeof(p));
    p.param1 = (uint32_t)hp | (30U << 8) |
               ((uint32_t)armor << 16) | (20U << 24);
    p.param2 = 16U | (12U << 8) | (14U << 16) | (16U << 24);
    p.active = 1U;
    p.level = 1U;
    p.weapon = 3U;
    p.weapons = (uint16_t)(1U << 3);
    p.ammo[2] = 3U;
    return p;
}

int main(void) {
    EspNativeGameplayPlayerDamageResult result;
    EspNativeGameplayPlayerState before = playerWith(18U, 3U);
    EspNativeGameplayPlayerState after;
    uint32_t fnvBefore;
    assert(EspNativeGameplayPlayerState_restore(&before));
    fnvBefore = EspNativeGameplayPlayerState_fingerprint();

    /* Exact CYD incident: blast=22, hp=18, armor=3. Ordinary callers must
     * NOT mutate/accept lethal pain; only explicit death-bound callers can. */
    assert(EspNativeGameplayPlayerState_applyDamageNonlethal(
        22U, 22U, &result) ==
        ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_LETHAL_DEFERRED);
    assert(result.healthBefore == 18U && result.healthAfter == 0U);
    assert(result.armorBefore == 3U && result.armorAfter == 0U);
    assert(result.stateFNVBefore == fnvBefore);
    assert(result.stateFNVAfter == fnvBefore);
    assert(EspNativeGameplayPlayerState_fingerprint() == fnvBefore);

    assert(EspNativeGameplayPlayerState_applyDamageForDeath(
        22U, 22U, &result) ==
        ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_LETHAL_COMMITTED);
    assert(result.healthAfter == 0U && result.armorAfter == 0U);
    assert(EspNativeGameplayPlayerState_health() == 0U);
    assert(EspNativeGameplayPlayerState_armor() == 0U);
    assert(result.stateFNVAfter == EspNativeGameplayPlayerState_fingerprint());
    assert(result.stateFNVBefore == fnvBefore);
    assert(result.stateFNVAfter != fnvBefore);
    assert(EspNativeGameplayPlayerState_snapshot(&after));
    assert((after.param1 & 0xffU) == 0U);
    assert(after.ammo[2] == before.ammo[2]); /* blast never spends ammo */

    /* Death_arm failure: the action journal must be able to restore the
     * original player state without leaking HP-zero into the next turn. */
    assert(EspNativeGameplayPlayerState_restore(&before));
    assert(EspNativeGameplayPlayerState_fingerprint() == fnvBefore);
    assert(memcmp(EspNativeGameplayPlayerState_view(),
                  &before, sizeof(before)) == 0);

    before = playerWith(26U, 11U);
    assert(EspNativeGameplayPlayerState_restore(&before));
    assert(EspNativeGameplayPlayerState_applyDamageForDeath(
        3U, 3U, &result) == ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_OK);
    assert(result.healthAfter == 23U && result.armorAfter == 8U);
    assert(EspNativeGameplayPlayerState_health() == 23U);
    assert(EspNativeGameplayPlayerState_armor() == 8U);
    puts("BARRELLETHAL: PASS nonlethal-fail-closed terminal-hp-zero rollback exact regular-damage");
    return 0;
}
