#include <stdint.h>
#include <string.h>
#include "esp_native_gameplay_dog_familiar.h"

int EspNativeDogFamiliar_active(
    const EspNativeGameplayPlayerState* player) {
    return player != NULL && player->active == 1U &&
           player->weapon >= 9U && player->weapon <= 11U &&
           player->ammo[5] != 0U;
}

EspNativeDogFamiliarStatus EspNativeDogFamiliar_prepare(
    const EspNativeGameplayPlayerState* before,
    uint32_t combinedDamage,
    EspNativeGameplayPlayerState* outNext,
    uint8_t* outDogHpAfter) {
    if (outNext != NULL) memset(outNext, 0, sizeof *outNext);
    if (outDogHpAfter != NULL) *outDogHpAfter = 0U;
    if (before == NULL || outNext == NULL || outDogHpAfter == NULL ||
        before->active != 1U || combinedDamage == 0U)
        return ESP_NATIVE_DOG_FAMILIAR_INVALID;
    if (!EspNativeDogFamiliar_active(before))
        return ESP_NATIVE_DOG_FAMILIAR_NOT_ACTIVE;
    if (combinedDamage >= before->ammo[5])
        return ESP_NATIVE_DOG_FAMILIAR_DEATH_DEFERRED;
    *outNext = *before;
    outNext->ammo[5] = (uint8_t)(before->ammo[5] - combinedDamage);
    *outDogHpAfter = outNext->ammo[5];
    return ESP_NATIVE_DOG_FAMILIAR_ABSORBED;
}

/* Exact Combat_init() weapon ammoType / ammoUsage for weapon 0..8.
 * Player_selectNextWeapon() searches after the removed dog weapon first;
 * 9..11 are ALL removed by 0xfffff1ff, so it then wraps 0..8 and
 * accepts owned weapons with ammo[ammoType]>0 OR ammoUsage==0. */
static const uint8_t nextAmmoType[9] = {0U,0U,1U,2U,1U,2U,4U,3U,4U};
static const uint8_t nextAmmoUsage[9] = {0U,1U,1U,1U,3U,2U,3U,1U,15U};

int EspNativeDogFamiliar_planDeath(
    const EspNativeGameplayPlayerState* before,
    uint32_t incomingCombinedDamage,
    EspNativeDogFamiliarDeathPlan* outPlan) {
    EspNativeGameplayPlayerState next;
    uint8_t candidate;
    if (outPlan != NULL) memset(outPlan,0,sizeof *outPlan);
    if (outPlan == NULL || before == NULL ||
        !EspNativeDogFamiliar_active(before) ||
        incomingCombinedDamage < before->ammo[5]) return 0;
    next=*before;
    next.ammo[5]=0U;
    next.weapons&=(uint16_t)~0x0e00U;
    for(candidate=0U;candidate<9U;++candidate) {
        const uint16_t mask=(uint16_t)(1U<<candidate);
        if ((next.weapons&mask)!=0U &&
            (nextAmmoUsage[candidate]==0U ||
             next.ammo[nextAmmoType[candidate]]!=0U)) {
            next.weapon=candidate;
            outPlan->playerAfter=next;
            outPlan->corpseGSpriteKind=(uint8_t)(20U+before->weapon-9U);
            outPlan->nextWeapon=candidate;
            outPlan->previousWeapon=before->weapon;
            return 1;
        }
    }
    /* Source may retain a now-unowned selected weapon in pathological
     * inventory states. Never publish that dangling native selection. */
    return 0;
}
