#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_monster_retaliation.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_player_view_state.h"

#define RETALIATION_NO_SPRITE 0xffffU
#define RETALIATION_MONSTER_WEAPON_COUNT 19U
#define RETALIATION_HIT_MISS 0U
#define RETALIATION_HIT_NORMAL 1U
#define RETALIATION_HIT_CRIT 2U
#define RETALIATION_DOG_WEAPON_FIRST 9U
#define RETALIATION_DOG_WEAPON_LAST 11U
#define RETALIATION_DOG_AMMO_TYPE 5U

typedef struct MonsterWeaponSpec_s {
    uint8_t strMin;
    uint8_t strMax;
    uint8_t rangeMin;
    uint8_t rangeMax;
    uint8_t armorSplit;
    uint8_t valid;
} MonsterWeaponSpec;

typedef struct MonsterRetaliationRoll_s {
    int32_t totalDamage;
    int32_t totalArmorDamage;
    int32_t firstCalcHit;
    int32_t firstCritLimit;
    uint32_t rngCalls;
    uint32_t missProjectileRngCalls;
    uint8_t firstRandHit;
    uint8_t firstRandDamage;
    uint8_t loops;
    uint8_t hitLoops;
    uint8_t gotCrit;
} MonsterRetaliationRoll;

/* Exact CombatEntity.c subtype -> primary/alternate attack table. */
static const uint8_t monsterAttacks[28] = {
    2U, 3U, 12U, 13U, 4U, 4U, 15U, 12U, 13U, 14U, 13U, 12U, 15U, 13U,
    15U, 14U, 7U, 12U, 7U, 3U, 15U, 15U, 16U, 17U, 7U, 17U, 12U, 13U
};

/* Exact monsterWpInfo NUMSHOTS field. */
static const uint8_t monsterShots[14] = {
    1U, 1U, 3U, 1U, 3U, 1U, 3U, 1U, 1U, 1U, 1U, 1U, 1U, 3U
};

/* Values copied from Combat_init(). Monster -> player uses the neutral 256
 * weapon multiplier because legacy Player CombatEntity.mType is -1. */
static const MonsterWeaponSpec monsterWeapons[RETALIATION_MONSTER_WEAPON_COUNT] = {
    {0U, 0U, 0U, 0U, 0U, 0U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {6U, 7U, 5U, 80U, 102U, 1U},
    {6U, 10U, 2U, 80U, 128U, 1U},
    {3U, 6U, 3U, 90U, 102U, 1U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {15U, 36U, 8U, 70U, 128U, 1U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {0U, 0U, 0U, 0U, 0U, 0U},
    {3U, 5U, 0U, 90U, 128U, 1U},
    {4U, 7U, 0U, 80U, 128U, 1U},
    {5U, 15U, 0U, 70U, 128U, 1U},
    {4U, 10U, 3U, 85U, 128U, 1U},
    {10U, 20U, 3U, 75U, 128U, 1U},
    {15U, 30U, 2U, 80U, 128U, 1U},
    {0U, 0U, 0U, 0U, 0U, 0U}
};

static EspNativeGameplayMonsterRetaliationView retaliationView;

static uint8_t p1Health(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p1Armor(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p2Defense(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p2Strength(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p2Agility(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p2Accuracy(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }

static uint32_t fnvByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t fnv32(uint32_t hash, uint32_t value) {
    hash = fnvByte(hash, (uint8_t)(value & 0xffU));
    hash = fnvByte(hash, (uint8_t)((value >> 8) & 0xffU));
    hash = fnvByte(hash, (uint8_t)((value >> 16) & 0xffU));
    return fnvByte(hash, (uint8_t)((value >> 24) & 0xffU));
}

static uint32_t randomFNV(const Random_t* random) {
    uint32_t hash = 2166136261U;
    uint32_t i;
    if (random == NULL) return 0U;
    for (i = 0U; i < RANDTABLESIZE; ++i) {
        hash = fnvByte(hash, random->randTable[i]);
    }
    return fnv32(hash, (uint32_t)random->nextRand);
}

static const char* reasonName(uint8_t reason) {
    switch ((EspNativeGameplayMonsterTurnReason)reason) {
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_MOVE: return "MOVE";
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_ROTATE: return "ROTATE";
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PLAYER_ATTACK: return "PLAYER_ATTACK";
    default: return "NONE";
    }
}

static int projectileMissConsumesRng(uint8_t weaponId) {
    return weaponId == 6U || weaponId == 7U || weaponId == 8U ||
           (weaponId >= 12U && weaponId <= 17U);
}

static int rollMonsterAttack(DoomRPG_t* doomRpg,
                             const EspNativeGameplayMonsterRecord* monster,
                             const EspNativeGameplayPlayerState* player,
                             uint8_t weaponId,
                             uint8_t loops,
                             MonsterRetaliationRoll* outRoll) {
    const MonsterWeaponSpec* weapon;
    uint8_t loop;
    int playerDefense;
    int playerAgility;

    if (outRoll != NULL) memset(outRoll, 0, sizeof(*outRoll));
    if (doomRpg == NULL || monster == NULL || player == NULL || outRoll == NULL ||
        weaponId >= RETALIATION_MONSTER_WEAPON_COUNT || loops == 0U) return 0;
    weapon = &monsterWeapons[weaponId];
    if (weapon->valid == 0U) return 0;
    playerDefense = (int)p2Defense(player->param2) << 8;
    playerAgility = (int)p2Agility(player->param2) << 8;
    if (playerDefense <= 0 || playerAgility <= 0) return 0;

    outRoll->loops = loops;
    for (loop = 0U; loop < loops; ++loop) {
        int calcHit;
        int critLimit;
        int randHit;
        int hitType;

        calcHit = (((((int)p2Accuracy(monster->param2) << 16) /
                      playerAgility) * 128) >> 8) +
                  (((int)weapon->rangeMax << 16) / 51200);
        critLimit = (calcHit << 8) / 5120;
        randHit = DoomRPG_randNextByte(&doomRpg->random);
        ++outRoll->rngCalls;
        if (loop == 0U) {
            outRoll->firstRandHit = (uint8_t)randHit;
            outRoll->firstCalcHit = calcHit;
            outRoll->firstCritLimit = critLimit;
        }
        hitType = randHit < calcHit
                      ? (randHit < critLimit ? RETALIATION_HIT_CRIT
                                             : RETALIATION_HIT_NORMAL)
                      : RETALIATION_HIT_MISS;

        if (hitType == RETALIATION_HIT_MISS) {
            if (projectileMissConsumesRng(weaponId)) {
                (void)DoomRPG_randNextByte(&doomRpg->random);
                ++outRoll->rngCalls;
                ++outRoll->missProjectileRngCalls;
            }
            continue;
        }

        ++outRoll->hitLoops;
        if (loop == 0U && hitType == RETALIATION_HIT_CRIT) outRoll->gotCrit = 1U;
        {
            int randStr = DoomRPG_randNextByte(&doomRpg->random);
            int strength = (int)p2Strength(monster->param2) << 16;
            int weaponRoll;
            int calDmg;
            int calDmgArm;
            int attackScale = outRoll->gotCrit != 0U ? 512 : 256;

            ++outRoll->rngCalls;
            if (loop == 0U) outRoll->firstRandDamage = (uint8_t)randStr;
            weaponRoll = ((int)weapon->strMin << 8) +
                         ((randStr * (((int)weapon->strMax -
                                      (int)weapon->strMin) << 8)) >> 8);
            calDmg = (((((((weaponRoll * (strength / playerDefense)) >> 8) *
                           attackScale) >> 8) * 256) >> 8));
            if (calDmg < 256) calDmg = 256;
            else if (calDmg > 255744) calDmg = 255744;
            calDmgArm = (calDmg * (int)weapon->armorSplit) >> 8;
            outRoll->totalArmorDamage += (calDmgArm + 128) >> 8;
            outRoll->totalDamage += ((calDmg - calDmgArm) + 128) >> 8;
        }
    }
    return 1;
}

static void prospectivePlayerPain(const EspNativeGameplayPlayerState* player,
                                  int32_t damage,
                                  int32_t armorDamage,
                                  uint8_t* outHealth,
                                  uint8_t* outArmor) {
    int32_t health;
    int32_t armor;
    int32_t healthDamage;
    if (player == NULL || outHealth == NULL || outArmor == NULL) return;
    health = p1Health(player->param1);
    armor = p1Armor(player->param1);
    healthDamage = damage;
    if (armor < armorDamage) {
        healthDamage += armorDamage - armor;
        armor = 0;
    }
    else {
        armor -= armorDamage;
    }
    health -= healthDamage;
    if (health < 0) health = 0;
    *outHealth = (uint8_t)health;
    *outArmor = (uint8_t)armor;
}

static int commitPlayerPain(const EspNativeGameplayPlayerState* before,
                            uint8_t healthAfter,
                            uint8_t armorAfter) {
    EspNativeGameplayPlayerState next;
    if (before == NULL || before->active != 1U) return 0;
    next = *before;
    next.param1 = (next.param1 & 0xff00ff00U) |
                  (uint32_t)healthAfter |
                  ((uint32_t)armorAfter << 16);
    return EspNativeGameplayPlayerState_restore(&next);
}

static int syncOwner(void) {
    const EspNativeGameplayMonsterTurnView* turn =
        EspNativeGameplayMonsterTurn_view();
    const EspNativeGameplayMonsterView* monsters =
        EspNativeGameplayMonsterState_view();

    if (turn == NULL || turn->active != 1U || turn->sourceArenaFNV1a == 0U ||
        monsters == NULL || monsters->records == NULL ||
        monsters->sourceArenaFNV1a != turn->sourceArenaFNV1a) {
        return 0;
    }

    if (retaliationView.active == 0U ||
        retaliationView.sourceArenaFNV1a != turn->sourceArenaFNV1a) {
        memset(&retaliationView, 0, sizeof(retaliationView));
        retaliationView.sourceArenaFNV1a = turn->sourceArenaFNV1a;
        retaliationView.observedAttackProbes = turn->attackProbes;
        retaliationView.lastAttackerSpriteIndex = RETALIATION_NO_SPRITE;
        retaliationView.active = 1U;
        printf("[MONSTERRETAL] READY arena=%08x ownerBytes=%u source=hardware-proven-turn-probe commit=nonlethal-playerstate render=transactional miss=commit-rng lethal=fail-closed dogFamiliar=fail-closed movement=deferred attackVisual=deferred painFX=deferred sound=deferred\n",
               (unsigned int)retaliationView.sourceArenaFNV1a,
               (unsigned int)sizeof(retaliationView));
    }
    return 1;
}

void EspNativeGameplayMonsterRetaliation_reset(void) {
    memset(&retaliationView, 0, sizeof(retaliationView));
    retaliationView.lastAttackerSpriteIndex = RETALIATION_NO_SPRITE;
}

const EspNativeGameplayMonsterRetaliationView*
EspNativeGameplayMonsterRetaliation_view(void) {
    return syncOwner() ? &retaliationView : NULL;
}

void EspNativeGameplayMonsterRetaliation_service(struct DoomRPG_s* doomRpgBase) {
    DoomRPG_t* doomRpg = (DoomRPG_t*)doomRpgBase;
    const EspNativeGameplayMonsterTurnView* turn;
    const EspNativeGameplayMonsterRecord* monster;
    const EspPlayerViewState* playerView;
    EspNativeGameplayPlayerState playerBefore;
    MonsterRetaliationRoll roll;
    EspNativeGameplayFrameStats frame;
    EspNativeGameplayFrameStats rollbackFrame;
    Random_t randomBefore;
    Random_t randomAfterRoll;
    uint32_t randomFNVBefore;
    uint32_t randomFNVAfter;
    uint32_t playerFNVBefore;
    uint32_t playerFNVAfter;
    uint8_t weaponId;
    uint8_t loops;
    uint8_t aiDecision = 0U;
    uint32_t aiRngCalls = 0U;
    uint8_t healthAfter;
    uint8_t armorAfter;
    int rollbackRendered;

    if (!syncOwner()) return;
    turn = EspNativeGameplayMonsterTurn_view();
    if (turn == NULL || turn->attackProbes == retaliationView.observedAttackProbes) {
        return;
    }

    /* One native input action is serviced at a time. If this ever jumps by more
     * than one, fail closed instead of guessing which attacker/order was lost. */
    if (turn->attackProbes != retaliationView.observedAttackProbes + 1U) {
        printf("[MONSTERRETAL] DEFER probes=%u->%u cause=probe-sequence-gap mutation=no rngConsumed=0\n",
               (unsigned int)retaliationView.observedAttackProbes,
               (unsigned int)turn->attackProbes);
        retaliationView.observedAttackProbes = turn->attackProbes;
        return;
    }
    retaliationView.observedAttackProbes = turn->attackProbes;
    retaliationView.lastAttackerSpriteIndex = turn->lastAttackerSpriteIndex;

    if (doomRpg == NULL || doomRpg->render == NULL ||
        turn->lastAttackerSpriteIndex == RETALIATION_NO_SPRITE ||
        !EspNativeGameplayPlayerState_snapshot(&playerBefore)) {
        printf("[MONSTERRETAL] DEFER probe=%u reason=%s cause=not-ready mutation=no rngConsumed=0\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason));
        return;
    }

    playerView = EspPlayerView_view();
    if (playerView == NULL || playerView->active != 1U ||
        playerView->viewX != playerView->destX ||
        playerView->viewY != playerView->destY ||
        playerView->viewAngle != playerView->destAngle) {
        printf("[MONSTERRETAL] DEFER probe=%u reason=%s cause=unsettled-player-view mutation=no rngConsumed=0\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason));
        return;
    }

    monster = EspNativeGameplayMonsterState_find(turn->lastAttackerSpriteIndex);
    if (monster == NULL || monster->alive == 0U || monster->subtype >= 14U) {
        printf("[MONSTERRETAL] DEFER probe=%u reason=%s sprite=%u cause=attacker-not-live mutation=no rngConsumed=0\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)turn->lastAttackerSpriteIndex);
        return;
    }

    if (playerBefore.weapon >= RETALIATION_DOG_WEAPON_FIRST &&
        playerBefore.weapon <= RETALIATION_DOG_WEAPON_LAST &&
        playerBefore.ammo[RETALIATION_DOG_AMMO_TYPE] != 0U) {
        ++retaliationView.dogFamiliarDeferred;
        printf("[MONSTERRETAL] DOG-DEFER probe=%u reason=%s sprite=%u playerWeapon=%u dogAmmo=%u legacyDamageTarget=dog-familiar owner=not-yet-native mutation=no rngConsumed=0\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               (unsigned int)playerBefore.weapon,
               (unsigned int)playerBefore.ammo[RETALIATION_DOG_AMMO_TYPE]);
        return;
    }

    weaponId = monsterAttacks[(uint32_t)monster->subtype * 2U +
                              (monster->alternateAttack != 0U ? 1U : 0U)];
    loops = monsterShots[monster->subtype];
    if (weaponId >= RETALIATION_MONSTER_WEAPON_COUNT ||
        monsterWeapons[weaponId].valid == 0U || loops == 0U) {
        printf("[MONSTERRETAL] DEFER probe=%u reason=%s sprite=%u weapon=%u cause=weapon-not-owned mutation=no rngConsumed=0\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               (unsigned int)weaponId);
        return;
    }

    randomBefore = doomRpg->random;
    randomFNVBefore = randomFNV(&randomBefore);
    playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();

    /* Replay the exact AI byte consumed by the proven probe. The probe restored
     * Random_t before returning, so the live replay must obtain the same value.
     * A >=217 value here would mean the replay diverged and therefore rolls back. */
    if (((1U + (uint32_t)monsterWeapons[weaponId].rangeMin) / 2U) != 0U) {
        aiDecision = DoomRPG_randNextByte(&doomRpg->random);
        ++aiRngCalls;
        if (aiDecision >= 217U) {
            doomRpg->random = randomBefore;
            printf("[MONSTERRETAL] REPLAY-DIVERGED probe=%u reason=%s sprite=%u weapon=%u aiRand=%u expected=<217 rngRollback=yes mutation=no\n",
                   (unsigned int)turn->attackProbes,
                   reasonName(turn->lastReason),
                   (unsigned int)monster->spriteIndex,
                   (unsigned int)weaponId,
                   (unsigned int)aiDecision);
            return;
        }
    }

    memset(&roll, 0, sizeof(roll));
    if (!rollMonsterAttack(doomRpg, monster, &playerBefore,
                           weaponId, loops, &roll)) {
        doomRpg->random = randomBefore;
        printf("[MONSTERRETAL] DEFER probe=%u reason=%s sprite=%u cause=roll-failed rngRollback=yes mutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex);
        return;
    }

    prospectivePlayerPain(&playerBefore,
                          roll.totalDamage,
                          roll.totalArmorDamage,
                          &healthAfter,
                          &armorAfter);
    randomAfterRoll = doomRpg->random;

    if (healthAfter == 0U && roll.hitLoops != 0U) {
        doomRpg->random = randomBefore;
        ++retaliationView.lethalDeferred;
        printf("[MONSTERRETAL] LETHAL-DEFER probe=%u reason=%s sprite=%u subtype=%u weapon=%u loops=%u hitLoops=%u totalDamage=%d armorDamage=%d playerHP=%u->0 armor=%u->%u playerDeathState=not-owned rngCalls=%u rng=%08x->%08x rollback=yes mutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               (unsigned int)monster->subtype,
               (unsigned int)weaponId,
               (unsigned int)roll.loops,
               (unsigned int)roll.hitLoops,
               (int)roll.totalDamage,
               (int)roll.totalArmorDamage,
               (unsigned int)p1Health(playerBefore.param1),
               (unsigned int)p1Armor(playerBefore.param1),
               (unsigned int)armorAfter,
               (unsigned int)(aiRngCalls + roll.rngCalls),
               (unsigned int)randomFNVBefore,
               (unsigned int)randomFNV(&doomRpg->random));
        return;
    }

    if (roll.hitLoops == 0U) {
        ++retaliationView.committedAttacks;
        ++retaliationView.committedMisses;
        randomFNVAfter = randomFNV(&doomRpg->random);
        printf("[MONSTERRETAL] MISS-COMMIT probe=%u reason=%s sprite=%u subtype=%u mType=%u weapon=%u alt=%u loops=%u firstRandHit=%u firstCalcHit=%d firstCritLimit=%d aiRand=%s%u rngCalls=%u combatRngCalls=%u missProjectileRng=%u playerHP=%u armor=%u playerFNV=%08x rng=%08x->%08x gameplayRngCommitted=yes playerMutation=no attackVisual=deferred sound=deferred turn=closed\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               (unsigned int)monster->subtype,
               (unsigned int)monster->mType,
               (unsigned int)weaponId,
               (unsigned int)monster->alternateAttack,
               (unsigned int)roll.loops,
               (unsigned int)roll.firstRandHit,
               (int)roll.firstCalcHit,
               (int)roll.firstCritLimit,
               aiRngCalls != 0U ? "value/" : "unused/",
               (unsigned int)aiDecision,
               (unsigned int)(aiRngCalls + roll.rngCalls),
               (unsigned int)roll.rngCalls,
               (unsigned int)roll.missProjectileRngCalls,
               (unsigned int)p1Health(playerBefore.param1),
               (unsigned int)p1Armor(playerBefore.param1),
               (unsigned int)playerFNVBefore,
               (unsigned int)randomFNVBefore,
               (unsigned int)randomFNVAfter);
        return;
    }

    if (!commitPlayerPain(&playerBefore, healthAfter, armorAfter)) {
        doomRpg->random = randomBefore;
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        printf("[MONSTERRETAL] ROLLBACK probe=%u reason=%s sprite=%u cause=playerstate-commit rngRollback=yes playerRollback=yes mutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex);
        return;
    }

    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(doomRpg->render,
                                           (uint8_t)playerView->viewAngle,
                                           &frame) ||
        memcmp(&doomRpg->random, &randomAfterRoll, sizeof(randomAfterRoll)) != 0) {
        int renderChangedRng =
            memcmp(&doomRpg->random, &randomAfterRoll, sizeof(randomAfterRoll)) != 0;
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        doomRpg->random = randomBefore;
        memset(&rollbackFrame, 0, sizeof(rollbackFrame));
        rollbackRendered = EspNativeGameplayFrame_renderTurn(
            doomRpg->render, (uint8_t)playerView->viewAngle, &rollbackFrame);
        ++retaliationView.renderRollbacks;
        printf("[MONSTERRETAL] ROLLBACK probe=%u reason=%s sprite=%u cause=%s playerExact=%s rngExact=%s rollbackRender=%s frame=%08x mutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               renderChangedRng ? "render-touched-gameplay-rng" : "render-failed",
               memcmp(EspNativeGameplayPlayerState_view(),
                      &playerBefore, sizeof(playerBefore)) == 0 ? "yes" : "NO",
               memcmp(&doomRpg->random,
                      &randomBefore, sizeof(randomBefore)) == 0 ? "yes" : "NO",
               rollbackRendered ? "yes" : "NO",
               rollbackRendered ? (unsigned int)rollbackFrame.frameAfterFNV : 0U);
        return;
    }

    ++retaliationView.committedAttacks;
    playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();
    randomFNVAfter = randomFNV(&doomRpg->random);
    printf("[MONSTERRETAL] COMMIT probe=%u reason=%s sprite=%u subtype=%u mType=%u weapon=%u alt=%u loops=%u hitLoops=%u firstRandHit=%u firstCalcHit=%d firstCritLimit=%d firstRandDamage=%u totalDamage=%d armorDamage=%d crit=%u aiRand=%s%u rngCalls=%u combatRngCalls=%u missProjectileRng=%u playerHP=%u->%u armor=%u->%u playerFNV=%08x->%08x rng=%08x->%08x frame=%08x presented=%u rollback=closed attackVisual=deferred painFX=deferred damageText=deferred sound=deferred playerDeath=fail-closed turn=closed\n",
           (unsigned int)turn->attackProbes,
           reasonName(turn->lastReason),
           (unsigned int)monster->spriteIndex,
           (unsigned int)monster->subtype,
           (unsigned int)monster->mType,
           (unsigned int)weaponId,
           (unsigned int)monster->alternateAttack,
           (unsigned int)roll.loops,
           (unsigned int)roll.hitLoops,
           (unsigned int)roll.firstRandHit,
           (int)roll.firstCalcHit,
           (int)roll.firstCritLimit,
           (unsigned int)roll.firstRandDamage,
           (int)roll.totalDamage,
           (int)roll.totalArmorDamage,
           (unsigned int)roll.gotCrit,
           aiRngCalls != 0U ? "value/" : "unused/",
           (unsigned int)aiDecision,
           (unsigned int)(aiRngCalls + roll.rngCalls),
           (unsigned int)roll.rngCalls,
           (unsigned int)roll.missProjectileRngCalls,
           (unsigned int)p1Health(playerBefore.param1),
           (unsigned int)healthAfter,
           (unsigned int)p1Armor(playerBefore.param1),
           (unsigned int)armorAfter,
           (unsigned int)playerFNVBefore,
           (unsigned int)playerFNVAfter,
           (unsigned int)randomFNVBefore,
           (unsigned int)randomFNVAfter,
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
}
