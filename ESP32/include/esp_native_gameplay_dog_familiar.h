#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_DOG_FAMILIAR_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_DOG_FAMILIAR_H

#include <stdint.h>
#include "esp_native_gameplay_player_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Source Player_pain() special-case: a selected dog weapon 9..11 and
 * ammo[5]>0 means the familiar absorbs ALL combined health+armor damage.
 * Player HP/armor never decrease during that absorption.
 *
 * When the familiar dies, the source also mutates map/entity sprites and
 * drops weapons 9..11. This has no native transaction owner yet, so
 * DEATH_DEFERRED MUST NOT publish any partial mutation or blood cue. */
typedef enum EspNativeDogFamiliarStatus_e {
    ESP_NATIVE_DOG_FAMILIAR_INVALID = 0,
    ESP_NATIVE_DOG_FAMILIAR_NOT_ACTIVE = 1,
    ESP_NATIVE_DOG_FAMILIAR_DEATH_DEFERRED = 2,
    ESP_NATIVE_DOG_FAMILIAR_ABSORBED = 3
} EspNativeDogFamiliarStatus;

int EspNativeDogFamiliar_active(
    const EspNativeGameplayPlayerState* player);

EspNativeDogFamiliarStatus EspNativeDogFamiliar_prepare(
    const EspNativeGameplayPlayerState* before,
    uint32_t combinedDamage,
    EspNativeGameplayPlayerState* outNext,
    uint8_t* outDogHpAfter);

#ifdef __cplusplus
}
#endif
#endif
