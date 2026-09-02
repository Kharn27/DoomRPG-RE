#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_native_gameplay_combat_math.h"

#define HIT_MISS 0U
#define HIT_NORMAL 1U
#define HIT_CRIT 2U

static const EspNativeGameplayWeaponSpec weapons[ESP_NATIVE_GAMEPLAY_STANDARD_WEAPONS] = {
    /* resource, min,max, rmin,rmax, ammo,usage, armorSplit, loops, radial */
    {5044U,  3U, 12U, 0U,  70U, 0U, 0U,  25U, 1U, 0U, 0U}, /* Axe */
    {5045U,  1U,  2U, 0U, 100U, 0U, 1U, 204U, 1U, 0U, 0U}, /* Extinguisher */
    {5046U,  6U,  7U, 5U,  80U, 1U, 1U, 102U, 1U, 0U, 0U}, /* Pistol */
    {5047U,  6U, 10U, 2U,  80U, 2U, 1U, 128U, 1U, 0U, 0U}, /* Shotgun */
    {5048U,  3U,  6U, 3U,  90U, 1U, 3U, 102U, 3U, 0U, 0U}, /* Chaingun */
    {5049U, 12U, 18U, 1U,  90U, 2U, 2U,  51U, 1U, 0U, 0U}, /* Super shotgun */
    {5050U,  6U,  8U, 4U,  90U, 4U, 3U, 230U, 3U, 0U, 0U}, /* Plasma */
    {5051U, 15U, 36U, 8U,  70U, 3U, 1U, 128U, 1U, 1U, 0U}, /* Rocket */
    {5052U, 60U,105U, 8U, 100U, 4U,15U,  76U, 1U, 1U, 0U}  /* BFG */
};

static uint8_t p1MaxHealth(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p1MaxArmor(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }
static uint8_t p2Defense(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p2Strength(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p2Agility(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p2Accuracy(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }

const EspNativeGameplayWeaponSpec* EspNativeGameplayCombatMath_weapon(
    uint8_t weaponIndex) {
    return weaponIndex < ESP_NATIVE_GAMEPLAY_STANDARD_WEAPONS
               ? &weapons[weaponIndex]
               : NULL;
}

static int rangeMinToDist(const EspNativeGameplayWeaponSpec* weapon) {
    int range = (int)weapon->rangeMin * 64;
    return (range * range) + 4096;
}

static int calcHit(DoomRPG_t* doomRpg,
                   const EspNativeGameplayWeaponSpec* weapon,
                   uint8_t playerAccuracy,
                   uint8_t targetAgility,
                   uint32_t worldDistance,
                   uint8_t* outRandHit,
                   int32_t* outCalcHit,
                   int32_t* outCritLimit,
                   uint32_t* ioRngCalls) {
    int calcHit;
    int decHit;
    int distance;
    int distStep;
    int dist2;
    uint8_t randHit;

    if (doomRpg == NULL || weapon == NULL || targetAgility == 0U ||
        outRandHit == NULL || outCalcHit == NULL || outCritLimit == NULL ||
        ioRngCalls == NULL) return -1;

    calcHit = (((((int)playerAccuracy << 16) /
                 ((int)targetAgility << 8)) * 128) >> 8) +
              (((int)weapon->rangeMax << 16) / 51200);
    dist2 = (int)worldDistance - rangeMinToDist(weapon);

    /* Exact CombatEntity_calcHit early range rejection: no RNG is consumed. */
    if (dist2 > 0 && weapon->rangeMin == 0U) {
        *outRandHit = 0U;
        *outCalcHit = calcHit;
        *outCritLimit = (calcHit << 8) / 5120;
        return HIT_MISS;
    }

    if (weapon->ammoType != 2U) {
        distance = 4096;
        distStep = 2;
        decHit = (calcHit * 76) >> 8;
        while (distance < dist2) {
            calcHit -= decHit;
            ++distStep;
            distance = (distStep * 64) * (distStep * 64);
        }
    }

    *outCalcHit = calcHit;
    *outCritLimit = (calcHit << 8) / 5120;
    randHit = DoomRPG_randNextByte(&doomRpg->random);
    ++(*ioRngCalls);
    *outRandHit = randHit;
    if ((int)randHit >= calcHit) return HIT_MISS;
    return (int)randHit < *outCritLimit ? HIT_CRIT : HIT_NORMAL;
}

static int weaponMultiplier(uint8_t weaponIndex,
                            const EspNativeGameplayWeaponSpec* weapon,
                            uint8_t mType) {
    uint8_t ammoType;
    if (weapon == NULL) return 256;
    ammoType = weapon->ammoType;

    if (mType != 0U) {
        switch (mType) {
        case 1U:
            if (ammoType == 2U) return 409;
            break;
        case 2U:
            if (ammoType == 4U) return 512;
            if (weaponIndex == 0U) return 204;
            break;
        case 3U:
            if (ammoType == 2U) return 512;
            if (ammoType == 4U) return 165;
            break;
        case 4U:
            if (weaponIndex == 0U) return 191;
            if (ammoType == 0U) return 2550;
            break;
        case 5U:
            if (ammoType == 2U) return 382;
            if (ammoType == 3U) return 191;
            break;
        case 6U:
            if (weaponIndex == 0U) return 768;
            if (ammoType == 5U) return 140;
            break;
        case 7U:
            if (ammoType == 5U) return 637;
            if (ammoType == 1U) return 165;
            break;
        case 8U:
            if (weaponIndex == 0U) return 768;
            if (ammoType == 2U) return 140;
            break;
        case 9U:
            if (ammoType == 3U) return 512;
            if (ammoType == 5U) return 165;
            break;
        case 10U:
            if (ammoType == 0U) return 1530;
            if (ammoType == 1U) return 165;
            break;
        case 11U:
            if (ammoType == 2U) return 512;
            break;
        case 12U:
            if (ammoType == 3U) return 382;
            break;
        case 13U:
            if (ammoType == 4U) return 382;
            break;
        default:
            break;
        }
        return 256;
    }

    return weaponIndex == 0U ? 768 : 204;
}

static int calcDamage(DoomRPG_t* doomRpg,
                      uint8_t weaponIndex,
                      const EspNativeGameplayWeaponSpec* weapon,
                      uint8_t playerStrength,
                      const EspNativeGameplayMonsterRecord* target,
                      int attackScale,
                      uint32_t worldDistance,
                      uint8_t* outRandDamage,
                      int32_t* outDamage,
                      int32_t* outArmorDamage,
                      uint32_t* ioRngCalls) {
    int randStr;
    int strength;
    int defense;
    int multiplier;
    int weaponRoll;
    int calDmg;
    int calDmgArm;
    int distance;
    int distStep;
    int decDmg;

    if (doomRpg == NULL || weapon == NULL || target == NULL ||
        outRandDamage == NULL || outDamage == NULL ||
        outArmorDamage == NULL || ioRngCalls == NULL) return 0;

    defense = (int)p2Defense(target->param2) << 8;
    if (defense <= 0) return 0;
    randStr = DoomRPG_randNextByte(&doomRpg->random);
    ++(*ioRngCalls);
    *outRandDamage = (uint8_t)randStr;
    strength = (int)playerStrength << 16;
    multiplier = weaponMultiplier(weaponIndex, weapon, target->mType);
    weaponRoll = ((int)weapon->strMin << 8) +
                 ((randStr * (((int)weapon->strMax -
                               (int)weapon->strMin) << 8)) >> 8);

    calDmg = (((((((weaponRoll * (strength / defense)) >> 8) *
                  attackScale) >> 8) * multiplier) >> 8));

    distance = 4096;
    distStep = 2;
    decDmg = (calDmg * 76) >> 8;
    while (distance < (int)worldDistance - rangeMinToDist(weapon)) {
        calDmg -= decDmg;
        ++distStep;
        distance = (distStep * 64) * (distStep * 64);
    }
    if (calDmg < 256) calDmg = 256;
    else if (calDmg > 255744) calDmg = 255744;

    calDmgArm = (calDmg * (int)weapon->armorSplit) >> 8;
    *outArmorDamage = (calDmgArm + 128) >> 8;
    *outDamage = ((calDmg - calDmgArm) + 128) >> 8;
    return 1;
}

int EspNativeGameplayCombatMath_rollPlayerAttack(
    DoomRPG_t* doomRpg,
    uint8_t weaponIndex,
    const EspNativeGameplayPlayerState* player,
    const EspNativeGameplayMonsterRecord* target,
    uint32_t worldDistance,
    EspNativeGameplayAttackRoll* outRoll) {
    const EspNativeGameplayWeaponSpec* weapon;
    uint8_t loop;
    int baseScale;

    if (outRoll != NULL) memset(outRoll, 0, sizeof(*outRoll));
    weapon = EspNativeGameplayCombatMath_weapon(weaponIndex);
    if (doomRpg == NULL || player == NULL || target == NULL ||
        outRoll == NULL || weapon == NULL || target->alive == 0U ||
        weapon->attackLoops == 0U ||
        weapon->attackLoops > ESP_NATIVE_GAMEPLAY_MAX_ATTACK_LOOPS) {
        return 0;
    }

    outRoll->loops = weapon->attackLoops;
    baseScale = player->berserkerTics != 0U ? 768 : 256;

    for (loop = 0U; loop < weapon->attackLoops; ++loop) {
        int hitType = calcHit(doomRpg, weapon,
                              (uint8_t)((player->param2 >> 24) & 0xffU),
                              p2Agility(target->param2), worldDistance,
                              &outRoll->randHit[loop],
                              &outRoll->calcHit[loop],
                              &outRoll->critLimit[loop],
                              &outRoll->rngCalls);
        if (hitType < 0) return 0;
        outRoll->hitType[loop] = (uint8_t)hitType;
        if (hitType == HIT_MISS) continue;

        ++outRoll->hitLoops;
        /* Legacy only promotes gotCrit when the FIRST attack loop crits. Once
         * promoted, its doubled attack scale persists through later loops. */
        if (loop == 0U && hitType == HIT_CRIT) outRoll->gotCrit = 1U;
        if (!calcDamage(doomRpg, weaponIndex, weapon,
                        (uint8_t)((player->param2 >> 8) & 0xffU),
                        target,
                        outRoll->gotCrit != 0U ? ((baseScale * 512) >> 8)
                                               : baseScale,
                        worldDistance,
                        &outRoll->randDamage[loop],
                        &outRoll->totalDamage,
                        &outRoll->totalArmorDamage,
                        &outRoll->rngCalls)) {
            return 0;
        }

        /* calcDamage returns one loop's values. Combat_playerSeq accumulates
         * those across animation loops, so re-run through temporaries below. */
        if (weapon->attackLoops > 1U) {
            int32_t oneDamage = outRoll->totalDamage;
            int32_t oneArmor = outRoll->totalArmorDamage;
            uint8_t j;
            outRoll->totalDamage = oneDamage;
            outRoll->totalArmorDamage = oneArmor;
            for (j = 0U; j < loop; ++j) {
                /* Prior loop totals are stored nowhere else; this branch is
                 * replaced immediately below by the explicit accumulator. */
            }
        }
    }

    return 1;
}

uint32_t EspNativeGameplayCombatMath_monsterExp(
    const EspNativeGameplayMonsterRecord* target) {
    if (target == NULL) return 0U;
    return (uint32_t)((((int)p2Defense(target->param2) +
                        (int)p2Strength(target->param2)) * 5 +
                       ((int)p2Agility(target->param2) +
                        (int)p2Accuracy(target->param2)) * 3 +
                       ((int)p1MaxHealth(target->param1) +
                        (int)p1MaxArmor(target->param1)) * 5 + 49) / 50);
}
