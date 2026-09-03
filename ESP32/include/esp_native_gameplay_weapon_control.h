#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_WEAPON_CONTROL_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_WEAPON_CONTROL_H

#include <stdint.h>

#include "esp_native_gameplay_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EspNativeGameplayWeaponControlStatus_e {
    ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_PREPARED = 0,
    ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_UNCHANGED = 1,
    ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_INVALID = 2,
    ESP_NATIVE_GAMEPLAY_WEAPON_CONTROL_NOT_READY = 3
} EspNativeGameplayWeaponControlStatus;

typedef struct EspNativeGameplayWeaponControlResult_s {
    uint32_t playerFNVBefore;
    uint16_t weapons;
    uint8_t weaponBefore;
    uint8_t weaponAfter;
    uint8_t ammoTypeAfter;
    uint8_t ammoAfter;
    uint8_t direction;
    uint8_t inspected;
} EspNativeGameplayWeaponControlResult;

/* Recover legacy Player_selectNextWeapon / Player_selectPrevWeapon without
 * mutating PlayerState. Selection wraps across the 12 legacy player slots and
 * accepts only an owned weapon that has ammo or consumes no ammo. The caller
 * owns the subsequent PlayerState commit/redraw/rollback transaction. */
EspNativeGameplayWeaponControlStatus EspNativeGameplayWeaponControl_prepare(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayWeaponControlResult* outResult);

const char* EspNativeGameplayWeaponControl_statusName(
    EspNativeGameplayWeaponControlStatus status);

#ifdef __cplusplus
}
#endif

#endif
