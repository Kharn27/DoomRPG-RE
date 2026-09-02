#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_STATE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DoomRPG_s;

#define ESP_NATIVE_GAMEPLAY_PLAYER_AMMO_TYPES 6U
#define ESP_NATIVE_GAMEPLAY_PLAYER_INVENTORY_SLOTS 5U
#define ESP_NATIVE_GAMEPLAY_PLAYER_WEAPON_LIMIT 12U

typedef struct EspNativeGameplayPlayerState_s {
    uint32_t param1;
    uint32_t param2;
    uint32_t currentXP;
    uint32_t nextLevelXP;
    uint32_t keys;
    uint32_t credits;
    uint32_t xpGained;
    uint16_t weapons;
    uint16_t disabledWeapons;
    uint16_t berserkerTics;
    uint8_t ammo[ESP_NATIVE_GAMEPLAY_PLAYER_AMMO_TYPES];
    uint8_t inventory[ESP_NATIVE_GAMEPLAY_PLAYER_INVENTORY_SLOTS];
    uint8_t level;
    uint8_t weapon;
    uint8_t active;
    uint8_t reserved;
} EspNativeGameplayPlayerState;

typedef struct EspNativeGameplayPlayerXpResult_s {
    uint32_t xpApplied;
    uint32_t rngCalls;
    uint32_t stateFNVBefore;
    uint32_t stateFNVAfter;
    uint32_t nextLevelXPBefore;
    uint32_t nextLevelXPAfter;
    uint8_t levelBefore;
    uint8_t levelAfter;
    uint8_t levelUps;
    uint8_t reserved;
} EspNativeGameplayPlayerXpResult;

void EspNativeGameplayPlayerState_resetFresh(void);
int EspNativeGameplayPlayerState_ensure(void);
const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void);
int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState* outState);
int EspNativeGameplayPlayerState_restore(
    const EspNativeGameplayPlayerState* snapshot);

/* Shared player-facing primitives. Combat, pickups and key/script families all
 * mutate this one compact owner rather than creating per-feature mini owners. */
int EspNativeGameplayPlayerState_adoptWeapon(uint8_t weapon);
int EspNativeGameplayPlayerState_consumeAmmo(uint8_t ammoType,
                                             uint8_t ammoUsage,
                                             uint8_t* outBefore,
                                             uint8_t* outAfter);
int EspNativeGameplayPlayerState_addAmmo(uint8_t ammoType,
                                         uint8_t amount,
                                         uint8_t* outAdded);
int EspNativeGameplayPlayerState_addInventory(uint8_t slot,
                                              uint8_t amount,
                                              uint8_t* outAdded);
int EspNativeGameplayPlayerState_addHealth(uint8_t amount,
                                           uint8_t* outAdded);
int EspNativeGameplayPlayerState_addArmor(uint8_t amount,
                                          uint8_t* outAdded);
int EspNativeGameplayPlayerState_addCredits(uint32_t amount);
int EspNativeGameplayPlayerState_addKeys(uint32_t keyMask);
int EspNativeGameplayPlayerState_applyXp(
    struct DoomRPG_s* doomRpg,
    uint32_t xp,
    EspNativeGameplayPlayerXpResult* outResult);

uint8_t EspNativeGameplayPlayerState_health(void);
uint8_t EspNativeGameplayPlayerState_maxHealth(void);
uint8_t EspNativeGameplayPlayerState_armor(void);
uint8_t EspNativeGameplayPlayerState_maxArmor(void);
uint8_t EspNativeGameplayPlayerState_defense(void);
uint8_t EspNativeGameplayPlayerState_strength(void);
uint8_t EspNativeGameplayPlayerState_agility(void);
uint8_t EspNativeGameplayPlayerState_accuracy(void);
uint8_t EspNativeGameplayPlayerState_ammo(uint8_t ammoType);
uint16_t EspNativeGameplayPlayerState_weapons(void);
uint32_t EspNativeGameplayPlayerState_fingerprint(void);

#ifdef __cplusplus
}
#endif

#endif
