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
