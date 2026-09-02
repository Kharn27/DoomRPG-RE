#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_player_state.h"

#define PLAYER_WEAPON_LIMIT 12U
#define PLAYER_STAT_MAX 99U

static EspNativeGameplayPlayerState playerState;

_Static_assert(sizeof(EspNativeGameplayPlayerState) == 52U,
               "native player gameplay state must remain 52 bytes");

static uint8_t p1Health(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p1MaxHealth(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p1Armor(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p1MaxArmor(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }
static uint8_t p2Defense(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p2Strength(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p2Agility(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p2Accuracy(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }

static uint32_t packParam1(uint8_t health,
                           uint8_t maxHealth,
                           uint8_t armor,
                           uint8_t maxArmor) {
    return (uint32_t)health |
           ((uint32_t)maxHealth << 8) |
           ((uint32_t)armor << 16) |
           ((uint32_t)maxArmor << 24);
}

static uint32_t packParam2(uint8_t defense,
                           uint8_t strength,
                           uint8_t agility,
                           uint8_t accuracy) {
    return (uint32_t)defense |
           ((uint32_t)strength << 8) |
           ((uint32_t)agility << 16) |
           ((uint32_t)accuracy << 24);
}

static uint8_t cappedAdd(uint8_t value, uint8_t amount) {
    uint16_t sum = (uint16_t)value + amount;
    return (uint8_t)(sum > PLAYER_STAT_MAX ? PLAYER_STAT_MAX : sum);
}

static uint32_t hashByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t hash16(uint32_t hash, uint16_t value) {
    hash = hashByte(hash, (uint8_t)(value & 0xffU));
    return hashByte(hash, (uint8_t)((value >> 8) & 0xffU));
}

static uint32_t hash32(uint32_t hash, uint32_t value) {
    hash = hashByte(hash, (uint8_t)(value & 0xffU));
    hash = hashByte(hash, (uint8_t)((value >> 8) & 0xffU));
    hash = hashByte(hash, (uint8_t)((value >> 16) & 0xffU));
    return hashByte(hash, (uint8_t)((value >> 24) & 0xffU));
}

uint32_t EspNativeGameplayPlayerState_fingerprint(void) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (playerState.active == 0U) return 0U;
    hash = hash32(hash, playerState.param1);
    hash = hash32(hash, playerState.param2);
    hash = hash32(hash, playerState.currentXP);
    hash = hash32(hash, playerState.nextLevelXP);
    hash = hash32(hash, playerState.keys);
    hash = hash32(hash, playerState.credits);
    hash = hash32(hash, playerState.xpGained);
    hash = hash16(hash, playerState.weapons);
    hash = hash16(hash, playerState.disabledWeapons);
    hash = hash16(hash, playerState.berserkerTics);
    for (i = 0U; i < 6U; ++i) hash = hashByte(hash, playerState.ammo[i]);
    for (i = 0U; i < 5U; ++i) hash = hashByte(hash, playerState.inventory[i]);
    hash = hashByte(hash, playerState.level);
    hash = hashByte(hash, playerState.weapon);
    return hashByte(hash, playerState.active);
}

void EspNativeGameplayPlayerState_resetFresh(void) {
    memset(&playerState, 0, sizeof(playerState));
    playerState.param1 = packParam1(30U, 30U, 0U, 20U);
    playerState.param2 = packParam2(16U, 12U, 14U, 16U);
    playerState.currentXP = 0U;
    playerState.nextLevelXP = 80U;
    playerState.level = 1U;
    playerState.keys = 0U;
    playerState.credits = 0U;
    playerState.ammo[1] = 8U;
    playerState.weapon = 2U;
    playerState.weapons = (uint16_t)(1U << 2);
    playerState.active = 1U;
    printf("[PLAYERSTATE] READY bytes=%u level=1 xp=0/80 hp=30/30 armor=0/20 def=16 str=12 agi=14 acc=16 ammo1=8 weapon=2 weapons=0004 stateFNV=%08x legacyPlayer=no\n",
           (unsigned int)sizeof(playerState),
           (unsigned int)EspNativeGameplayPlayerState_fingerprint());
}

int EspNativeGameplayPlayerState_ensure(void) {
    if (playerState.active == 0U) EspNativeGameplayPlayerState_resetFresh();
    return playerState.active == 1U;
}

const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void) {
    return EspNativeGameplayPlayerState_ensure() ? &playerState : NULL;
}

int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState* outState) {
    if (outState == NULL || !EspNativeGameplayPlayerState_ensure()) return 0;
    *outState = playerState;
    return 1;
}

int EspNativeGameplayPlayerState_restore(
    const EspNativeGameplayPlayerState* snapshot) {
    if (snapshot == NULL || snapshot->active != 1U) return 0;
    playerState = *snapshot;
    return 1;
}

int EspNativeGameplayPlayerState_adoptWeapon(uint8_t weapon) {
    if (!EspNativeGameplayPlayerState_ensure() || weapon >= PLAYER_WEAPON_LIMIT) {
        return 0;
    }
    playerState.weapon = weapon;
    playerState.weapons |= (uint16_t)(1U << weapon);
    return 1;
}

static uint8_t levelRoll(DoomRPG_t* doomRpg,
                         uint8_t base,
                         uint8_t span,
                         uint32_t* rngCalls) {
    uint32_t value;
    value = (uint32_t)DoomRPG_randNextInt(&doomRpg->random) & 255U;
    ++(*rngCalls);
    return (uint8_t)(base + (value % span));
}

static void nextLevel(DoomRPG_t* doomRpg, uint32_t* rngCalls) {
    uint8_t maxHealth;
    uint8_t maxArmor;
    uint8_t defense;
    uint8_t strength;
    uint8_t agility;
    uint8_t accuracy;
    uint8_t gainHealth;
    uint8_t gainArmor;
    uint8_t gainDefense;
    uint8_t gainStrength;
    uint8_t gainAgility;
    uint8_t gainAccuracy;

    ++playerState.level;
    playerState.nextLevelXP = ((uint32_t)playerState.level * 20U) + 60U;

    gainHealth = levelRoll(doomRpg, 3U, 3U, rngCalls);
    maxHealth = cappedAdd(p1MaxHealth(playerState.param1), gainHealth);

    gainArmor = levelRoll(doomRpg, 3U, 3U, rngCalls);
    maxArmor = cappedAdd(p1MaxArmor(playerState.param1), gainArmor);

    gainDefense = levelRoll(doomRpg, 1U, 2U, rngCalls);
    defense = cappedAdd(p2Defense(playerState.param2), gainDefense);

    gainStrength = levelRoll(doomRpg, 1U, 2U, rngCalls);
    strength = cappedAdd(p2Strength(playerState.param2), gainStrength);

    gainAgility = levelRoll(doomRpg, 1U, 2U, rngCalls);
    agility = cappedAdd(p2Agility(playerState.param2), gainAgility);

    gainAccuracy = levelRoll(doomRpg, 1U, 2U, rngCalls);
    accuracy = cappedAdd(p2Accuracy(playerState.param2), gainAccuracy);

    playerState.param1 = packParam1(maxHealth,
                                    maxHealth,
                                    p1Armor(playerState.param1),
                                    maxArmor);
    playerState.param2 = packParam2(defense, strength, agility, accuracy);
}

int EspNativeGameplayPlayerState_applyXp(
    DoomRPG_t* doomRpg,
    uint32_t xp,
    EspNativeGameplayPlayerXpResult* outResult) {
    uint32_t rngCalls = 0U;
    uint8_t levelBefore;
    uint32_t nextBefore;
    uint32_t fnvBefore;
    uint8_t levelUps = 0U;

    if (outResult != NULL) memset(outResult, 0, sizeof(*outResult));
    if (doomRpg == NULL || outResult == NULL ||
        !EspNativeGameplayPlayerState_ensure()) return 0;

    levelBefore = playerState.level;
    nextBefore = playerState.nextLevelXP;
    fnvBefore = EspNativeGameplayPlayerState_fingerprint();
    playerState.currentXP += xp;
    playerState.xpGained += xp;

    while (playerState.currentXP >= playerState.nextLevelXP) {
        playerState.currentXP -= playerState.nextLevelXP;
        nextLevel(doomRpg, &rngCalls);
        if (levelUps != 0xffU) ++levelUps;
    }

    outResult->xpApplied = xp;
    outResult->rngCalls = rngCalls;
    outResult->stateFNVBefore = fnvBefore;
    outResult->stateFNVAfter = EspNativeGameplayPlayerState_fingerprint();
    outResult->nextLevelXPBefore = nextBefore;
    outResult->nextLevelXPAfter = playerState.nextLevelXP;
    outResult->levelBefore = levelBefore;
    outResult->levelAfter = playerState.level;
    outResult->levelUps = levelUps;
    return 1;
}

uint8_t EspNativeGameplayPlayerState_health(void) {
    return EspNativeGameplayPlayerState_ensure() ? p1Health(playerState.param1) : 0U;
}

uint8_t EspNativeGameplayPlayerState_maxHealth(void) {
    return EspNativeGameplayPlayerState_ensure() ? p1MaxHealth(playerState.param1) : 0U;
}

uint8_t EspNativeGameplayPlayerState_armor(void) {
    return EspNativeGameplayPlayerState_ensure() ? p1Armor(playerState.param1) : 0U;
}

uint8_t EspNativeGameplayPlayerState_maxArmor(void) {
    return EspNativeGameplayPlayerState_ensure() ? p1MaxArmor(playerState.param1) : 0U;
}

uint8_t EspNativeGameplayPlayerState_defense(void) {
    return EspNativeGameplayPlayerState_ensure() ? p2Defense(playerState.param2) : 0U;
}

uint8_t EspNativeGameplayPlayerState_strength(void) {
    return EspNativeGameplayPlayerState_ensure() ? p2Strength(playerState.param2) : 0U;
}

uint8_t EspNativeGameplayPlayerState_agility(void) {
    return EspNativeGameplayPlayerState_ensure() ? p2Agility(playerState.param2) : 0U;
}

uint8_t EspNativeGameplayPlayerState_accuracy(void) {
    return EspNativeGameplayPlayerState_ensure() ? p2Accuracy(playerState.param2) : 0U;
}
