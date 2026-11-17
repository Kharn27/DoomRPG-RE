/* Actual native combat-owner pain settling query, not an abstract mock.
 * Link-time GC drops unrelated combat owners and keeps the real read-only ABI. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/esp_native_gameplay_monster_combat.c"

int main(void) {
    memset(&combatOwner,0,sizeof(combatOwner));
    combatOwner.view.active=1U;
    combatOwner.view.painSpriteIndex=157U;
    combatOwner.painUntilMs=1250U;
    assert(EspNativeGameplayMonsterCombat_painSettling(157U,1000U));
    assert(EspNativeGameplayMonsterCombat_painSettling(157U,1249U));
    assert(!EspNativeGameplayMonsterCombat_painSettling(157U,1250U));
    assert(!EspNativeGameplayMonsterCombat_painSettling(157U,1251U));
    assert(!EspNativeGameplayMonsterCombat_painSettling(158U,1100U));
    combatOwner.view.active=0U;
    assert(!EspNativeGameplayMonsterCombat_painSettling(157U,1100U));
    combatOwner.view.active=1U;
    combatOwner.painUntilMs=0U;
    assert(!EspNativeGameplayMonsterCombat_painSettling(157U,0xfffffff0U));
    /* Signed 32-bit elapsed tests are sound across one bounded time wrap. */
    combatOwner.painUntilMs=0x000000a0U;
    assert(EspNativeGameplayMonsterCombat_painSettling(157U,0xfffffff0U));
    assert(!EspNativeGameplayMonsterCombat_painSettling(157U,0x000000a0U));
    /* The audio lease is anchored at post-impact sound publication, not
     * at the earlier damage roll; the 250ms sprite pose may have expired. */
    combatOwner.painUntilMs=0U;
    combatOwner.view.painSpriteIndex=0xffffU;
    painSoundHoldSprite=157U;
    painSoundHoldUntilMs=1500U;
    assert(EspNativeGameplayMonsterCombat_painSettling(157U,1250U));
    assert(EspNativeGameplayMonsterCombat_painSettling(157U,1499U));
    assert(!EspNativeGameplayMonsterCombat_painSettling(157U,1500U));
    assert(!EspNativeGameplayMonsterCombat_painSettling(158U,1300U));
    puts("[MONSTERSFX-PAIN] PASS real 250ms owner lease, per-sprite, expiry, wrap, no combat mutation");
    return 0;
}
