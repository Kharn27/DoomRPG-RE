#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_COMBAT_MATH_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_COMBAT_MATH_H

#include <stdint.h>

#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_player_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

#define ESP_NATIVE_GAMEPLAY_STANDARD_WEAPONS 9U
#define ESP_NATIVE_GAMEPLAY_MAX_ATTACK_LOOPS 3U

typedef struct EspNativeGameplayWeaponSpec_s {
    uint16_t resourceId;
    uint8_t strMin;
    uint8_t strMax;
    uint8_t rangeMin;
    uint8_t rangeMax;
    uint8_t ammoType;
    uint8_t ammoUsage;
    uint8_t armorSplit;
    uint8_t attackLoops;
    uint8_t radialDamage;
    uint8_t reserved;
} EspNativeGameplayWeaponSpec;

typedef struct EspNativeGameplayAttackRoll_s {
    int32_t totalDamage;
    int32_t totalArmorDamage;
    int32_t calcHit[ESP_NATIVE_GAMEPLAY_MAX_ATTACK_LOOPS];
    int32_t critLimit[ESP_NATIVE_GAMEPLAY_MAX_ATTACK_LOOPS];
    uint32_t rngCalls;
    uint8_t randHit[ESP_NATIVE_GAMEPLAY_MAX_ATTACK_LOOPS];
    uint8_t randDamage[ESP_NATIVE_GAMEPLAY_MAX_ATTACK_LOOPS];
    uint8_t hitType[ESP_NATIVE_GAMEPLAY_MAX_ATTACK_LOOPS];
    uint8_t loops;
    uint8_t hitLoops;
    uint8_t gotCrit;
    uint8_t reserved;
} EspNativeGameplayAttackRoll;

const EspNativeGameplayWeaponSpec* EspNativeGameplayCombatMath_weapon(
    uint8_t weaponIndex);

/* Exact integer hit/damage math from Combat_playerSeq/CombatEntity for one
 * standard player weapon. This mutates only DoomRPG.random; callers own the
 * surrounding Random_t snapshot/rollback and all HP/ammo/world mutation. */
int EspNativeGameplayCombatMath_rollPlayerAttack(
    struct DoomRPG_s* doomRpg,
    uint8_t weaponIndex,
    const EspNativeGameplayPlayerState* player,
    const EspNativeGameplayMonsterRecord* target,
    uint32_t worldDistance,
    EspNativeGameplayAttackRoll* outRoll);

uint32_t EspNativeGameplayCombatMath_monsterExp(
    const EspNativeGameplayMonsterRecord* target);

#ifdef __cplusplus
}
#endif

#endif
