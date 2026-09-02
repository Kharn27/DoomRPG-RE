#ifndef DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_STATE_H
#define DOOMRPG_ESP32_NATIVE_GAMEPLAY_PLAYER_STATE_H

#include <stdint.h>

#include "DoomRPG.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    uint8_t ammo[6];
    uint8_t inventory[5];
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
    uint16_t nextLevelXPBefore;
    uint16_t nextLevelXPAfter;
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
    const EspNativeGameplayPlayerState* expectedSnapshot);
int EspNativeGameplayPlayerState_adoptWeapon(uint8_t weapon);
int EspNativeGameplayPlayerState_applyXp(
    DoomRPG_t* doomRpg,
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
uint32_t EspNativeGameplayPlayerState_fingerprint(void);

#ifdef __cplusplus
}
#endif

#endif
