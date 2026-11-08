#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_automap.h"
#include "esp_native_resident_gameplay.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_retaliation.h"
#include "esp_native_gameplay_monster_attack_visual.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_player_death.h"
#include "esp_native_gameplay_player_pain_visual.h"
#include "esp_native_gameplay_dog_familiar.h"
#include "esp_native_gameplay_monster_drop.h"
#include "esp_native_audio_intent.h"
#include "esp_player_view_state.h"
#include "platform_video_c_bridge.h"

#define RETALIATION_NO_SPRITE 0xffffU
#define RETALIATION_MONSTER_WEAPON_COUNT 19U
#define RETALIATION_HIT_MISS 0U
#define RETALIATION_HIT_NORMAL 1U
#define RETALIATION_HIT_CRIT 2U
#define RETALIATION_DAMAGE_TEXT_BYTES 64U
#define RETALIATION_DAMAGE_FLASH_MS 500U

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
static uint32_t waitingVisualProbe;

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
    case ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PASS_TURN: return "PASS_TURN";
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
        EspNativeGameplayMonsterActivation_turnView();
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
        waitingVisualProbe = 0U;
        retaliationView.sourceArenaFNV1a = turn->sourceArenaFNV1a;
        retaliationView.observedAttackProbes = turn->attackProbes;
        retaliationView.lastAttackerSpriteIndex = RETALIATION_NO_SPRITE;
        retaliationView.active = 1U;
        printf("[MONSTERRETAL] READY arena=%08x ownerBytes=%u source=hardware-proven-turn-probe commit=playerstate+native-death render=transactional playerPainFeedback=damage-text+red-500ms passMessage=legacy-superseded-on-hit miss=commit-rng lethal=playerdeath-live resolvedProbe=published dogFamiliar=fail-closed movement=ordered-sequencer attackVisual=deferred attackMessage=deferred painFace=native-500ms shake=native-world-weapon-500ms sound=deferred\n",
               (unsigned int)retaliationView.sourceArenaFNV1a,
               (unsigned int)sizeof(retaliationView));
    }
    return 1;
}

void EspNativeGameplayMonsterRetaliation_reset(void) {
    memset(&retaliationView, 0, sizeof(retaliationView));
    waitingVisualProbe = 0U;
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
    EspNativeGameplayPlayerState dogAfterState;
    EspNativeDogFamiliarDeathPlan dogDeathPlan;
    EspNativeGameplayMonsterDropSpawnPlan dogCorpsePlan;
    EspNativeGameplayMonsterDropSnapshot dogCorpseBefore;
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
    EspNativeGameplayDamageSide damageSide = ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_NONE;
    char damageText[RETALIATION_DAMAGE_TEXT_BYTES];
    int damageTextLen;
    int rollbackRendered;
    int feedbackRollback;
    uint8_t dogCandidate = 0U;
    uint8_t dogAbsorbed = 0U;
    uint8_t dogDied = 0U;
    uint8_t dogCorpseCommitted = 0U;
    uint8_t dogHpAfter = 0U;

    if (!syncOwner()) return;
    turn = EspNativeGameplayMonsterActivation_turnView();
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
    /*
     * Legacy Combat_monsterSeq does not apply Player_pain until stage 2, after
     * every attack/idle loop has completed.  Keep the turn transaction open:
     * no live RNG or player mutation is committed while the visual owner is
     * still presenting this exact probe.
     */
    if (!EspNativeGameplayMonsterAttackVisual_isProbeComplete(
            turn->attackProbes)) {
        if (waitingVisualProbe != turn->attackProbes) {
            waitingVisualProbe = turn->attackProbes;
            printf("[MONSTERRETAL] WAIT probe=%u reason=%s sprite=%u phase=attack-animation resolution=after-animation playerMutation=no rngConsumed=0 worldInput=blocked\n",
                   (unsigned int)turn->attackProbes,
                   reasonName(turn->lastReason),
                   (unsigned int)turn->lastAttackerSpriteIndex);
        }
        return;
    }

    waitingVisualProbe = 0U;
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

    /* Player_pain(): dog weapons 9..11 with dog HP in ammo[5] intercept.
     * Don't replay RNG before the proven attack animation is complete. */
    dogCandidate = (uint8_t)EspNativeDogFamiliar_active(&playerBefore);

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

    randomAfterRoll = doomRpg->random;
    if (dogCandidate != 0U && roll.hitLoops != 0U) {
        const uint32_t total =
            (uint32_t)roll.totalDamage + (uint32_t)roll.totalArmorDamage;
        const EspNativeDogFamiliarStatus result =
            EspNativeDogFamiliar_prepare(
                &playerBefore,total,&dogAfterState,&dogHpAfter);
        if (result == ESP_NATIVE_DOG_FAMILIAR_ABSORBED) {
            dogAbsorbed = 1U;
        }
        else if (result == ESP_NATIVE_DOG_FAMILIAR_DEATH_DEFERRED &&
                 EspNativeDogFamiliar_planDeath(
                     &playerBefore,total,&dogDeathPlan) &&
                 playerView->destX >= 32 && playerView->destX <= 2016 &&
                 playerView->destY >= 32 && playerView->destY <= 2016 &&
                 EspNativeGameplayMonsterDrop_prepareDogCorpse(
                     dogDeathPlan.corpseGSpriteKind,
                     (uint16_t)playerView->destX,
                     (uint16_t)playerView->destY,&dogCorpsePlan) &&
                 EspNativeGameplayMonsterDrop_snapshot(&dogCorpseBefore)) {
            /* The engine has no captured-Entity_t owner: faithfully use
             * Player_pain's other branch, custom gSprite 20/21/22 frame2
             * at the settled player tile. Its existing V11 dynamic sprite
             * pool owns this corpse through checkpoint and render rollback. */
            dogDied = 1U;
            printf("[DOGDEATH] PREPARE probe=%u corpse=%u slot=%u kind=legacy-gsprite-fallback frame=2 nextWeapon=%u playerHP=preserved world=not-yet-committed snapshot=V11\n",
                   (unsigned int)turn->attackProbes,
                   (unsigned int)dogDeathPlan.corpseGSpriteKind,
                   (unsigned int)dogCorpsePlan.slot,
                   (unsigned int)dogDeathPlan.nextWeapon);
        }
        else {
            doomRpg->random = randomBefore;
            ++retaliationView.dogFamiliarDeferred;
            printf("[MONSTERRETAL] DOG-DEFER probe=%u sprite=%u reason=%s dogHP=%u damage=%u cause=dog-death-preflight-failed rngRollback=yes playerMutation=no worldMutation=no blood=no\n",
                   (unsigned int)turn->attackProbes,
                   (unsigned int)monster->spriteIndex,
                   reasonName(turn->lastReason),
                   (unsigned int)playerBefore.ammo[5],
                   (unsigned int)total);
            return;
        }
        healthAfter = p1Health(playerBefore.param1);
        armorAfter = p1Armor(playerBefore.param1);
    } else {
        prospectivePlayerPain(&playerBefore,
                              roll.totalDamage,
                              roll.totalArmorDamage,
                              &healthAfter,&armorAfter);
    }

    /* Attribute the impact to THIS resolved attacker, including its final
     * post-movement position. Never infer the source from a rendered sprite.
     * The 500 ms red border now applies to EVERY hit. A resolved direction
     * adds a sequential 20-physical-px cue after the border's lease ends. */
    if (roll.hitLoops != 0U) {
        const EspNativeGameplayMonsterPositionRecord* attackerPosition =
            EspNativeGameplayMonsterPosition_find(monster->spriteIndex);
        int32_t stepX;
        int32_t stepY;
        if (attackerPosition != NULL &&
            EspPlayerView_cardinalBasis((uint8_t)playerView->viewAngle,
                                        NULL, NULL, &stepX, &stepY, NULL)) {
            damageSide = EspNativeGameplayDamageSide_fromWorld(
                playerView->viewX, playerView->viewY, stepX, stepY,
                attackerPosition->worldX, attackerPosition->worldY);
        }
    }

    if (healthAfter == 0U && roll.hitLoops != 0U) {
        uint16_t deathTile;
        int feedbackQueued;
        int deathArmed;
        int presented;

        if (playerView->destX < 0 || playerView->destY < 0 ||
            ((uint32_t)playerView->destX >> 6) >= 32U ||
            ((uint32_t)playerView->destY >> 6) >= 32U) {
            doomRpg->random = randomBefore;
            ++retaliationView.lethalDeferred;
            printf("[MONSTERRETAL] LETHAL-DEFER probe=%u reason=%s sprite=%u cause=player-tile-out-of-range rngRollback=yes mutation=no\n",
                   (unsigned int)turn->attackProbes,
                   reasonName(turn->lastReason),
                   (unsigned int)monster->spriteIndex);
            return;
        }
        deathTile = (uint16_t)((((uint32_t)playerView->destY >> 6) * 32U) +
                               ((uint32_t)playerView->destX >> 6));

        if (!commitPlayerPain(&playerBefore, healthAfter, armorAfter)) {
            doomRpg->random = randomBefore;
            (void)EspNativeGameplayPlayerState_restore(&playerBefore);
            ++retaliationView.lethalDeferred;
            printf("[MONSTERRETAL] LETHAL-DEFER probe=%u reason=%s sprite=%u cause=playerstate-commit rngRollback=yes playerRollback=yes mutation=no\n",
                   (unsigned int)turn->attackProbes,
                   reasonName(turn->lastReason),
                   (unsigned int)monster->spriteIndex);
            return;
        }

        damageTextLen = snprintf(damageText, sizeof(damageText), "%s%d damage!",
                                 roll.gotCrit != 0U ? "Crit! " : "",
                                 (int)(roll.totalDamage +
                                       roll.totalArmorDamage));
        feedbackQueued =
            damageTextLen > 0 &&
            (size_t)damageTextLen < sizeof(damageText) &&
            EspNativeGameplayActionEngine_queueTextFeedback(
                ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE,
                damageText, RETALIATION_DAMAGE_FLASH_MS);
        if (!feedbackQueued) {
            (void)EspNativeGameplayPlayerState_restore(&playerBefore);
            doomRpg->random = randomBefore;
            ++retaliationView.lethalDeferred;
            printf("[MONSTERRETAL] LETHAL-DEFER probe=%u reason=%s sprite=%u cause=damage-feedback playerRollback=yes rngRollback=yes mutation=no\n",
                   (unsigned int)turn->attackProbes,
                   reasonName(turn->lastReason),
                   (unsigned int)monster->spriteIndex);
            return;
        }

        deathArmed = EspNativeGameplayPlayerDeath_arm(
            doomRpgBase, turn->attackProbes, deathTile);
        if (!deathArmed) {
            feedbackRollback =
                EspNativeGameplayActionEngine_cancelQueuedFeedback(
                    ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE);
            (void)EspNativeGameplayPlayerState_restore(&playerBefore);
            doomRpg->random = randomBefore;
            ++retaliationView.lethalDeferred;
            printf("[MONSTERRETAL] LETHAL-DEFER probe=%u reason=%s sprite=%u cause=death-arm playerRollback=yes rngRollback=yes feedbackRollback=%s mutation=no\n",
                   (unsigned int)turn->attackProbes,
                   reasonName(turn->lastReason),
                   (unsigned int)monster->spriteIndex,
                   feedbackRollback ? "yes" : "NO");
            return;
        }

        presented = Esp32PlatformVideo_present();
        ++retaliationView.committedAttacks;
        ++retaliationView.lethalCommitted;
        retaliationView.lastResolvedProbe = turn->attackProbes;
        playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();
        randomFNVAfter = randomFNV(&doomRpg->random);
        printf("[MONSTERRETAL] LETHAL-COMMIT probe=%u reason=%s sprite=%u subtype=%u mType=%u weapon=%u alt=%u loops=%u hitLoops=%u firstRandHit=%u firstCalcHit=%d firstCritLimit=%d firstRandDamage=%u totalDamage=%d armorDamage=%d crit=%u aiRand=%s%u rngCalls=%u combatRngCalls=%u missProjectileRng=%u playerHP=%u->0 armor=%u->%u playerFNV=%08x->%08x rng=%08x->%08x attackRngCommitted=yes deathRngCommitted=yes deathTile=%u deathOwner=armed damageFeedback=%s presented=%s turn=terminal\n",
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
               (unsigned int)p1Armor(playerBefore.param1),
               (unsigned int)armorAfter,
               (unsigned int)playerFNVBefore,
               (unsigned int)playerFNVAfter,
               (unsigned int)randomFNVBefore,
               (unsigned int)randomFNVAfter,
               (unsigned int)deathTile,
               damageText,
               presented ? "yes" : "deferred");
        return;
    }

    if (roll.hitLoops == 0U) {
        int dodgeQueued;
        int dodgeRendered = 0;

        /*
         * Legacy Combat_monsterSeq() closes a monster miss with "Dodged!".
         * The gameplay result here is already fully known and RNG-owned; this
         * bounded top-bar message is presentation-only and must not mutate the
         * player or consume additional gameplay RNG.
         */
        dodgeQueued = EspNativeGameplayActionEngine_queueTextFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_COMBAT_TEXT,
            "Dodged!", 0U);
        if (dodgeQueued) {
            memset(&frame, 0, sizeof(frame));
            dodgeRendered = EspNativeGameplayFrame_renderTurn(
                doomRpg->render, (uint8_t)playerView->viewAngle, &frame);
            if (memcmp(&doomRpg->random,
                       &randomAfterRoll, sizeof(randomAfterRoll)) != 0) {
                doomRpg->random = randomAfterRoll;
                printf("[MONSTERRETAL] MISS-PRESENT-RNG-GUARD probe=%u sprite=%u restored=post-roll\n",
                       (unsigned int)turn->attackProbes,
                       (unsigned int)monster->spriteIndex);
            }
        }

        ++retaliationView.committedAttacks;
        ++retaliationView.committedMisses;
        retaliationView.lastResolvedProbe = turn->attackProbes;
        randomFNVAfter = randomFNV(&doomRpg->random);
        printf("[MONSTERRETAL] MISS-COMMIT probe=%u reason=%s sprite=%u subtype=%u mType=%u weapon=%u alt=%u loops=%u firstRandHit=%u firstCalcHit=%d firstCritLimit=%d aiRand=%s%u rngCalls=%u combatRngCalls=%u missProjectileRng=%u playerHP=%u armor=%u playerFNV=%08x rng=%08x->%08x message=\"Dodged!\" textQueued=%s rendered=%s frame=%08x gameplayRngCommitted=yes playerMutation=no attackVisual=complete-before-resolution sound=deferred turn=closed\n",
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
               (unsigned int)randomFNVAfter,
               dodgeQueued ? "yes" : "owner-busy",
               dodgeRendered ? "yes" : "deferred",
               dodgeRendered ? (unsigned int)frame.frameAfterFNV : 0U);
        return;
    }

    if (!(dogDied != 0U
              ? EspNativeGameplayPlayerState_restore(
                    &dogDeathPlan.playerAfter)
              : (dogAbsorbed != 0U
                     ? EspNativeGameplayPlayerState_restore(&dogAfterState)
                     : commitPlayerPain(&playerBefore,healthAfter,armorAfter)))) {
        doomRpg->random = randomBefore;
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        printf("[MONSTERRETAL] ROLLBACK probe=%u reason=%s sprite=%u cause=playerstate-commit rngRollback=yes playerRollback=yes mutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex);
        return;
    }

    damageTextLen = dogDied != 0U
        ? snprintf(damageText,sizeof damageText,
                   "%sDog took %d damage! Dog died!",
                   roll.gotCrit != 0U ? "Crit! " : "",
                   (int)(roll.totalDamage+roll.totalArmorDamage))
        : (dogAbsorbed != 0U
               ? snprintf(damageText,sizeof damageText,
                          "%sDog took %d damage!",
                          roll.gotCrit != 0U ? "Crit! " : "",
                          (int)(roll.totalDamage+roll.totalArmorDamage))
               : snprintf(damageText,sizeof damageText,"%s%d damage!",
                          roll.gotCrit != 0U ? "Crit! " : "",
                          (int)(roll.totalDamage+roll.totalArmorDamage)));
    if (damageTextLen <= 0 || (size_t)damageTextLen >= sizeof(damageText) ||
        !EspNativeGameplayActionEngine_queueTextFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE,
            damageText, RETALIATION_DAMAGE_FLASH_MS)) {
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        doomRpg->random = randomBefore;
        printf("[MONSTERRETAL] ROLLBACK probe=%u reason=%s sprite=%u cause=player-pain-feedback playerExact=%s rngExact=%s damageTotal=%d crit=%u mutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               memcmp(EspNativeGameplayPlayerState_view(),
                      &playerBefore, sizeof(playerBefore)) == 0 ? "yes" : "NO",
               memcmp(&doomRpg->random,
                      &randomBefore, sizeof(randomBefore)) == 0 ? "yes" : "NO",
               (int)(roll.totalDamage + roll.totalArmorDamage),
               (unsigned int)roll.gotCrit);
        return;
    }

    /* Render/commit the validated hit with its text as before, but hold the
     * nonlethal 2px red border until ordered member/FX work returns. The
     * action compositor then presents it together with the directional band.
     * Lethal retirement and early rollback branches never enter this opt-in. */
    /* The legacy pain palette and face belong to the same reversible world
     * transaction as the hit. They must NOT survive render/RNG rollback. */
    if (!((dogAbsorbed != 0U || dogDied != 0U)
          ? EspNativePlayerPainVisual_stageDog(
                turn->attackProbes,(EspNativePlayerPainSide)damageSide,
                DoomRPG_GetUpTimeMS())
          : EspNativePlayerPainVisual_stage(
                turn->attackProbes,(EspNativePlayerPainSide)damageSide,
                DoomRPG_GetUpTimeMS()))) {
        feedbackRollback = EspNativeGameplayActionEngine_cancelQueuedFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE);
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        doomRpg->random = randomBefore;
        printf("[PLAYERPAINFX] DEFER probe=%u cause=stage-busy feedbackRollback=%s playerRollback=yes rngRollback=yes\n",
               (unsigned int)turn->attackProbes,
               feedbackRollback ? "yes" : "NO");
        return;
    }
    /* Combat_updateProjectile() only spawns an impact animation for
     * a successful projectile whose aggregate player damage is nonzero.
     * This optional art choice is part of the already-staged face lease:
     * cancel() restores it on any failed player/world/raster transaction.
     * Source weapon12/13/14 => scratch sprites188/189/190, the other
     * projectile families => source180..187. No gameplay RNG reads. */
    if (roll.totalDamage + roll.totalArmorDamage > 0) {
        (void)EspNativePlayerPainVisual_stageImpact(
            turn->attackProbes,weaponId,monster->subtype);
    }
    /* The player, feedback, visual lease and corpse must be a single
     * rollback unit. Commit the V11 dynamic corpse only after every
     * cheap preflight above; any renderer or RNG failure restores its
     * exact 144-byte pool snapshot BEFORE drawing the rollback frame. */
    if (dogDied != 0U &&
        !EspNativeGameplayMonsterDrop_commit(&dogCorpsePlan)) {
        (void)EspNativePlayerPainVisual_cancel(turn->attackProbes);
        feedbackRollback = EspNativeGameplayActionEngine_cancelQueuedFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE);
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        doomRpg->random = randomBefore;
        printf("[DOGDEATH] ROLLBACK probe=%u cause=corpse-commit failed=fail-closed rngRollback=yes playerRollback=yes feedbackRollback=%s corpseRollback=not-written\n",
               (unsigned int)turn->attackProbes,
               feedbackRollback?"yes":"NO");
        return;
    }
    if (dogDied != 0U) dogCorpseCommitted = 1U;
    EspNativeGameplayActionEngine_stageNonlethalMonsterFlash();
    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(doomRpg->render,
                                           (uint8_t)playerView->viewAngle,
                                           &frame) ||
        memcmp(&doomRpg->random, &randomAfterRoll, sizeof(randomAfterRoll)) != 0) {
        int renderChangedRng =
            memcmp(&doomRpg->random, &randomAfterRoll, sizeof(randomAfterRoll)) != 0;
        int corpseRollbackExact = 1;
        (void)EspNativePlayerPainVisual_cancel(turn->attackProbes);
        feedbackRollback = EspNativeGameplayActionEngine_cancelQueuedFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE);
        if (dogCorpseCommitted != 0U) {
            corpseRollbackExact =
                EspNativeGameplayMonsterDrop_restore(&dogCorpseBefore) &&
                EspNativeGameplayMonsterDrop_fingerprint() ==
                    dogCorpseBefore.stateFNV1a;
        }
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        doomRpg->random = randomBefore;
        if (dogDied != 0U) {
            printf("[DOGDEATH] ROLLBACK probe=%u corpseExact=%s playerExact=%s rngExact=%s feedbackRollback=%s\n",
                   (unsigned int)turn->attackProbes,
                   corpseRollbackExact?"yes":"NO",
                   memcmp(EspNativeGameplayPlayerState_view(),
                          &playerBefore,sizeof playerBefore)==0?"yes":"NO",
                   memcmp(&doomRpg->random,&randomBefore,sizeof randomBefore)==0
                       ?"yes":"NO",
                   feedbackRollback?"yes":"already-consumed");
        }
        memset(&rollbackFrame, 0, sizeof(rollbackFrame));
        rollbackRendered = EspNativeGameplayFrame_renderTurn(
            doomRpg->render, (uint8_t)playerView->viewAngle, &rollbackFrame);
        ++retaliationView.renderRollbacks;
        printf("[MONSTERRETAL] ROLLBACK probe=%u reason=%s sprite=%u cause=%s playerExact=%s rngExact=%s feedbackRollback=%s rollbackRender=%s frame=%08x mutation=no\n",
               (unsigned int)turn->attackProbes,
               reasonName(turn->lastReason),
               (unsigned int)monster->spriteIndex,
               renderChangedRng ? "render-touched-gameplay-rng" : "render-failed",
               memcmp(EspNativeGameplayPlayerState_view(),
                      &playerBefore, sizeof(playerBefore)) == 0 ? "yes" : "NO",
               memcmp(&doomRpg->random,
                      &randomBefore, sizeof(randomBefore)) == 0 ? "yes" : "NO",
               feedbackRollback ? "yes" : "already-consumed",
               rollbackRendered ? "yes" : "NO",
               rollbackRendered ? (unsigned int)rollbackFrame.frameAfterFNV : 0U);
        return;
    }

    /* renderTurn already presented the staged native face+weapon palette;
     * publication is an owner-state change only and consumes no RNG. */
    (void)EspNativePlayerPainVisual_commit(turn->attackProbes);
    /* Player_painEvent() emits 5089 for dog interception, 5081 for
     * ordinary hurt, then Player_pain() emits 5090 when the dog dies.
     * Publish ONLY after the player/world/frame transaction commits:
     * no orphaned audio intents or altered gameplay RNG on rollback.
     * The existing backend intentionally remains silent. */
    (void)EspNativeAudioIntent_publish(
        dogCandidate != 0U ? 5089U : 5081U, 0U,
        dogCandidate != 0U ? 3U : 2U);
    if (dogDied != 0U) {
        (void)EspNativeAudioIntent_publish(5090U,0U,3U);
    }
    ++retaliationView.committedAttacks;
    retaliationView.lastResolvedProbe = turn->attackProbes;
    /* AUTOMAP has its own 900ms visual lease, only AFTER an actual committed
     * hit. Avoid mapping the pre-COMMIT DAMAGE topbar intent, which is
     * intentionally suppressed with the 3D feedback owner. */
    if (EspNativeResidentGameplay_isAutomapActive()) {
        EspNativeGameplayAutomap_noteDamage(
            (uint16_t)(roll.totalDamage + roll.totalArmorDamage),
            (uint8_t)(roll.gotCrit != 0U));
    }
    /* UI intent is published only after player+RNG+frame COMMIT. Overflow
     * never rolls back gameplay or aliases another attacker's direction. */
    if (damageSide != ESP_NATIVE_GAMEPLAY_DAMAGE_SIDE_NONE) {
        (void)EspNativeGameplayActionEngine_queueDirectionalDamage(
            turn->attackProbes, monster->spriteIndex, damageSide);
    }
    playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();
    randomFNVAfter = randomFNV(&doomRpg->random);
    if (dogDied != 0U) {
        printf("[DOGDEATH] COMMIT probe=%u sprite=%u dogHP=%u->0 playerHP=%u->%u armor=%u->%u weapon=%u->%u weapons=%04x->%04x corpseGSprite=%u frame=2 slot=%u world=%u,%u V11=persistent damage=%u sound5090=audio-intent-silent capturedEntity=no nativeFallback=yes rng=%08x->%08x rollback=closed\n",
               (unsigned int)turn->attackProbes,
               (unsigned int)monster->spriteIndex,
               (unsigned int)playerBefore.ammo[5],
               (unsigned int)p1Health(playerBefore.param1),
               (unsigned int)healthAfter,
               (unsigned int)p1Armor(playerBefore.param1),
               (unsigned int)armorAfter,
               (unsigned int)playerBefore.weapon,
               (unsigned int)dogDeathPlan.nextWeapon,
               (unsigned int)playerBefore.weapons,
               (unsigned int)dogDeathPlan.playerAfter.weapons,
               (unsigned int)dogDeathPlan.corpseGSpriteKind,
               (unsigned int)dogCorpsePlan.slot,
               (unsigned int)dogCorpsePlan.record.worldX,
               (unsigned int)dogCorpsePlan.record.worldY,
               (unsigned int)(roll.totalDamage+roll.totalArmorDamage),
               (unsigned int)randomFNVBefore,
               (unsigned int)randomFNVAfter);
    }
    else if (dogAbsorbed != 0U) {
        printf("[MONSTERRETAL] DOG-ABSORB-COMMIT probe=%u sprite=%u dogHP=%u->%u damage=%u playerHP=%u->%u armor=%u->%u dogBlood=15-particles source=Combat_spawnDogBloodParticles rng=%08x->%08x rollback=closed\n",
               (unsigned int)turn->attackProbes,
               (unsigned int)monster->spriteIndex,
               (unsigned int)playerBefore.ammo[5],
               (unsigned int)dogHpAfter,
               (unsigned int)(roll.totalDamage+roll.totalArmorDamage),
               (unsigned int)p1Health(playerBefore.param1),
               (unsigned int)healthAfter,
               (unsigned int)p1Armor(playerBefore.param1),
               (unsigned int)armorAfter,
               (unsigned int)randomFNVBefore,
               (unsigned int)randomFNVAfter);
    }
    printf("[MONSTERRETAL] COMMIT probe=%u reason=%s sprite=%u subtype=%u mType=%u weapon=%u alt=%u loops=%u hitLoops=%u firstRandHit=%u firstCalcHit=%d firstCritLimit=%d firstRandDamage=%u totalDamage=%d armorDamage=%d crit=%u aiRand=%s%u rngCalls=%u combatRngCalls=%u missProjectileRng=%u playerHP=%u->%u armor=%u->%u playerFNV=%08x->%08x rng=%08x->%08x frame=%08x presented=%u message=\"%s\" damageTotal=%d redFlash=b800/%ums passMessage=%s rollback=closed attackVisual=complete-before-resolution attackMessage=deferred painFace=native-500ms weaponBlend=native-500ms shake=native-world-weapon-500ms sound=5081-or-5089-5090-intent-silent statusWarnings=deferred playerDeath=nonlethal turn=closed\n",
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
           (unsigned int)frame.finalPresented,
           damageText,
           (int)(roll.totalDamage + roll.totalArmorDamage),
           (unsigned int)RETALIATION_DAMAGE_FLASH_MS,
           turn->lastReason == ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PASS_TURN
               ? "legacy-superseded" : "n/a");
}
