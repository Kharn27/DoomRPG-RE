#include <SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "Render.h"

#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_monster_combat.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_trace.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_weapon.h"
#include "esp_player_view_state.h"

/* This translation unit is the public linker-wrapper owner. The older action
 * and monster-state wrappers are private chain leaves renamed by their headers. */
#undef __wrap_EspMapSpriteTopology_getEntity
#undef __wrap_EspMapSpriteTopology_getVisualState
#undef __wrap_EspNativeGameplayActionEngine_service
#undef __wrap_EspNativeGameplayActionEngine_reset

#define MONSTER_TYPE_ENEMY 1U
#define MONSTER_SUBTYPE_COUNT 14U
#define MONSTER_NO_SPRITE 0xffffU
#define MONSTER_CORPSE_VISUAL 2U
#define MONSTER_DEATH_VISUAL 4U
#define MONSTER_PAIN_VISUAL 6U
#define MONSTER_VISUAL_HIDDEN 0x80U
#define MONSTER_PAIN_MS 250U
#define MONSTER_DEATH_MS 250U
#define MONSTER_MAX_SPRITES 1024U
#define MONSTER_GIB_BITS_BYTES (MONSTER_MAX_SPRITES / 8U)
#define MONSTER_CORPSE_BITS_BYTES (MONSTER_MAX_SPRITES / 8U)

/* Direct single-target standard weapons currently have complete native combat
 * semantics. Multi-loop presentation (chaingun/plasma), radial damage
 * (rocket/BFG), extinguisher entity rules and dog-familiar weapons are separate
 * mechanical families rather than monster-specific exceptions. */
#define STANDARD_WEAPON_DIRECT_MASK ((1U << 0) | (1U << 2) | \
                                     (1U << 3) | (1U << 5))

/* These legacy enemy families have death consequences beyond the generic
 * XP + corpse/gib unlink + drop path (spawn/extra animation/boss scripts).
 * Hit/pain is generic; only a lethal commit fails closed for these families. */
#define SPECIAL_DEATH_MASK ((1U << 7) | (1U << 8) | (1U << 12) | (1U << 13))

typedef struct MonsterCombatPending_s {
    uint32_t sequence;
    uint32_t worldDistance;
    uint16_t spriteIndex;
    uint16_t tileIndex;
    uint8_t subtype;
    uint8_t distance;
    uint8_t weapon;
    uint8_t active;
} MonsterCombatPending;

typedef struct MonsterCombatOwner_s {
    EspNativeGameplayMonsterCombatView view;
    MonsterCombatPending pending;
    uint32_t painUntilMs;
    uint32_t deathUntilMs;
    uint16_t deathSpriteIndex;
    uint8_t visualRedrawPending;
    uint8_t reserved;
    uint8_t gibbedBits[MONSTER_GIB_BITS_BYTES];
    uint8_t corpseBits[MONSTER_CORPSE_BITS_BYTES];
} MonsterCombatOwner;

static const uint16_t painSounds[MONSTER_SUBTYPE_COUNT] = {
    5085U, 5089U, 5085U, 5085U, 5099U, 5099U, 5099U,
    5110U, 5085U, 5117U, 5122U, 5099U, 5099U, 5137U
};

static const uint16_t deathSounds[MONSTER_SUBTYPE_COUNT][3] = {
    {5082U, 5084U, 5107U},
    {5090U,    0U,    0U},
    {5082U, 5084U, 5107U},
    {5076U, 5077U,    0U},
    {5101U,    0U,    0U},
    {5095U,    0U,    0U},
    {5102U,    0U,    0U},
    {5111U,    0U,    0U},
    {5113U,    0U,    0U},
    {5118U,    0U,    0U},
    {5123U,    0U,    0U},
    {5126U,    0U,    0U},
    {5129U,    0U,    0U},
    {5138U,    0U,    0U}
};

static MonsterCombatOwner combatOwner;

EspNativeGameplayActionStatus __real_EspNativeGameplayActionEngine_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult);

static uint8_t p1Health(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p1MaxHealth(uint32_t p) { return (uint8_t)((p >> 8) & 0xffU); }
static uint8_t p1Armor(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }
static uint8_t p1MaxArmor(uint32_t p) { return (uint8_t)((p >> 24) & 0xffU); }
static uint8_t p2Defense(uint32_t p) { return (uint8_t)(p & 0xffU); }
static uint8_t p2Agility(uint32_t p) { return (uint8_t)((p >> 16) & 0xffU); }

static uint32_t fnvByte(uint32_t hash, uint8_t value) {
    hash ^= value;
    return hash * 16777619U;
}

static uint32_t fnv16(uint32_t hash, uint16_t value) {
    hash = fnvByte(hash, (uint8_t)(value & 0xffU));
    return fnvByte(hash, (uint8_t)((value >> 8) & 0xffU));
}

static uint32_t fnv32(uint32_t hash, uint32_t value) {
    hash = fnvByte(hash, (uint8_t)(value & 0xffU));
    hash = fnvByte(hash, (uint8_t)((value >> 8) & 0xffU));
    hash = fnvByte(hash, (uint8_t)((value >> 16) & 0xffU));
    return fnvByte(hash, (uint8_t)((value >> 24) & 0xffU));
}

static uint32_t currentMonsterFNV(void) {
    const EspNativeGameplayMonsterView* monsters =
        EspNativeGameplayMonsterState_view();
    uint32_t hash = 2166136261U;
    uint32_t i;

    if (monsters == NULL || monsters->records == NULL) return 0U;
    for (i = 0U; i < monsters->count; ++i) {
        const EspNativeGameplayMonsterRecord* r = &monsters->records[i];
        hash = fnv32(hash, r->param1);
        hash = fnv32(hash, r->param2);
        hash = fnv16(hash, r->spriteIndex);
        hash = fnv16(hash, r->defTile);
        hash = fnvByte(hash, r->subtype);
        hash = fnvByte(hash, r->mType);
        hash = fnvByte(hash, r->alternateAttack);
        hash = fnvByte(hash, r->alive);
    }
    return hash;
}

static int gibbed(uint16_t spriteIndex) {
    return spriteIndex < MONSTER_MAX_SPRITES &&
           ((combatOwner.gibbedBits[spriteIndex >> 3] >>
             (spriteIndex & 7U)) & 1U) != 0U;
}

static void setGibbed(uint16_t spriteIndex, int value) {
    uint8_t mask;
    if (spriteIndex >= MONSTER_MAX_SPRITES) return;
    mask = (uint8_t)(1U << (spriteIndex & 7U));
    if (value) combatOwner.gibbedBits[spriteIndex >> 3] |= mask;
    else combatOwner.gibbedBits[spriteIndex >> 3] &= (uint8_t)~mask;
}

static int corpseReady(uint16_t spriteIndex) {
    return spriteIndex < MONSTER_MAX_SPRITES &&
           ((combatOwner.corpseBits[spriteIndex >> 3] >>
             (spriteIndex & 7U)) & 1U) != 0U;
}

static void setCorpseReady(uint16_t spriteIndex, int value) {
    uint8_t mask;
    if (spriteIndex >= MONSTER_MAX_SPRITES) return;
    mask = (uint8_t)(1U << (spriteIndex & 7U));
    if (value) combatOwner.corpseBits[spriteIndex >> 3] |= mask;
    else combatOwner.corpseBits[spriteIndex >> 3] &= (uint8_t)~mask;
}

static void expirePain(void) {
    if (combatOwner.view.painSpriteIndex == MONSTER_NO_SPRITE) return;
    if ((int32_t)(DoomRPG_GetUpTimeMS() - combatOwner.painUntilMs) >= 0) {
        combatOwner.view.painSpriteIndex = MONSTER_NO_SPRITE;
        combatOwner.painUntilMs = 0U;
    }
}

static int promoteDeathIfDue(void) {
    uint16_t spriteIndex;
    if (combatOwner.deathSpriteIndex == MONSTER_NO_SPRITE) return 0;
    if ((int32_t)(DoomRPG_GetUpTimeMS() - combatOwner.deathUntilMs) < 0) {
        return 0;
    }
    spriteIndex = combatOwner.deathSpriteIndex;
    setCorpseReady(spriteIndex, 1);
    combatOwner.deathSpriteIndex = MONSTER_NO_SPRITE;
    combatOwner.deathUntilMs = 0U;
    combatOwner.visualRedrawPending = 1U;
    printf("[MONSTERCOMBAT] CORPSE sprite=%u visual=%u->%u delayMs=%u immutableSprite=yes\n",
           (unsigned int)spriteIndex,
           (unsigned int)MONSTER_DEATH_VISUAL,
           (unsigned int)MONSTER_CORPSE_VISUAL,
           (unsigned int)MONSTER_DEATH_MS);
    return 1;
}

static int syncOwner(void) {
    const EspNativeGameplayMonsterView* monsters =
        EspNativeGameplayMonsterState_view();
    const EspNativeGameplayPlayerState* player;
    uint32_t arena;

    if (monsters == NULL || monsters->records == NULL || monsters->count == 0U ||
        monsters->count > ESP_NATIVE_GAMEPLAY_MONSTER_MAX_COUNT ||
        !EspNativeGameplayPlayerState_ensure()) {
        return 0;
    }
    player = EspNativeGameplayPlayerState_view();
    if (player == NULL) return 0;

    arena = monsters->sourceArenaFNV1a;
    if (combatOwner.view.active == 0U ||
        combatOwner.view.sourceArenaFNV1a != arena) {
        memset(&combatOwner, 0, sizeof(combatOwner));
        combatOwner.view.sourceArenaFNV1a = arena;
        combatOwner.view.pendingSpriteIndex = MONSTER_NO_SPRITE;
        combatOwner.view.painSpriteIndex = MONSTER_NO_SPRITE;
        combatOwner.deathSpriteIndex = MONSTER_NO_SPRITE;
        combatOwner.view.standardWeaponsOwned =
            (uint8_t)STANDARD_WEAPON_DIRECT_MASK;
        combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
        combatOwner.view.active = 1U;
        printf("[MONSTERCOMBAT] READY arena=%08x monsters=%u backend=type1-generic subtypes=0..13 trace=shared combatMath=shared playerState=%uB playerFNV=%08x directWeaponMask=%02x specialDeathMask=%04x deathVisual=4->2/%ums legacyEntity=no\n",
               (unsigned int)arena,
               (unsigned int)monsters->count,
               (unsigned int)sizeof(*player),
               (unsigned int)EspNativeGameplayPlayerState_fingerprint(),
               (unsigned int)combatOwner.view.standardWeaponsOwned,
               (unsigned int)SPECIAL_DEATH_MASK,
               (unsigned int)MONSTER_DEATH_MS);
    }
    expirePain();
    if (combatOwner.pending.active == 0U) (void)promoteDeathIfDue();
    return 1;
}

void EspNativeGameplayMonsterCombat_reset(void) {
    memset(&combatOwner, 0, sizeof(combatOwner));
    combatOwner.view.pendingSpriteIndex = MONSTER_NO_SPRITE;
    combatOwner.view.painSpriteIndex = MONSTER_NO_SPRITE;
    combatOwner.deathSpriteIndex = MONSTER_NO_SPRITE;
}

int EspNativeGameplayMonsterCombat_isReady(void) {
    return syncOwner();
}

const EspNativeGameplayMonsterCombatView* EspNativeGameplayMonsterCombat_view(void) {
    if (!syncOwner()) return NULL;
    combatOwner.view.pending = combatOwner.pending.active;
    combatOwner.view.pendingSpriteIndex = combatOwner.pending.active != 0U
                                              ? combatOwner.pending.spriteIndex
                                              : MONSTER_NO_SPRITE;
    combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
    return &combatOwner.view;
}

static void applyPain(EspNativeGameplayMonsterRecord* record,
                      int32_t damage,
                      int32_t armorDamage,
                      int32_t* outHealthAfter,
                      int32_t* outArmorAfter) {
    int32_t armor;
    int32_t health;
    int32_t healthDamage;

    armor = p1Armor(record->param1);
    health = p1Health(record->param1);
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

    record->param1 = (record->param1 & 0xff00ff00U) |
                     (uint32_t)(uint8_t)health |
                     ((uint32_t)(uint8_t)armor << 16);
    if (outHealthAfter != NULL) *outHealthAfter = health;
    if (outArmorAfter != NULL) *outArmorAfter = armor;
}

static int specialDeath(uint8_t subtype) {
    return subtype < 16U && (SPECIAL_DEATH_MASK & (1U << subtype)) != 0U;
}

static int prospectiveGib(const EspNativeGameplayMonsterRecord* before,
                          int32_t damage,
                          int32_t armorDamage,
                          uint8_t tileDistance) {
    int32_t remaining;
    int32_t overkillThreshold;

    if (before == NULL || tileDistance != 1U) return 0;
    remaining = (int32_t)p1Health(before->param1) +
                (int32_t)p1Armor(before->param1) - damage - armorDamage;
    if (remaining > 0) return 0;
    overkillThreshold =
        -(((((int32_t)p1MaxHealth(before->param1)) << 16) / 637) >> 8);
    return remaining <= overkillThreshold || before->subtype == 13U;
}

static uint16_t consumePainSound(DoomRPG_t* runtime,
                                 uint8_t subtype,
                                 uint32_t* ioRngCalls) {
    if (runtime == NULL || ioRngCalls == NULL ||
        subtype >= MONSTER_SUBTYPE_COUNT || painSounds[subtype] == 0U) {
        return 0U;
    }
    /* Legacy EntityMonster_getSoundRnd() still consumes one RNG byte even when
     * there is exactly one non-zero pain sound candidate. */
    (void)DoomRPG_randNextByte(&runtime->random);
    ++(*ioRngCalls);
    return painSounds[subtype];
}

static uint16_t consumeDeathSound(DoomRPG_t* runtime,
                                  uint8_t subtype,
                                  uint32_t* ioRngCalls) {
    uint32_t count = 0U;
    uint8_t pick;
    if (runtime == NULL || ioRngCalls == NULL || subtype >= MONSTER_SUBTYPE_COUNT) {
        return 0U;
    }
    while (count < 3U && deathSounds[subtype][count] != 0U) ++count;
    if (count == 0U) return 0U;
    pick = DoomRPG_randNextByte(&runtime->random);
    ++(*ioRngCalls);
    return deathSounds[subtype][pick % count];
}

static void logFrame(const MonsterCombatPending* pending,
                     const char* phase,
                     const EspNativeGameplayFrameStats* frame) {
    if (pending == NULL || phase == NULL || frame == NULL) return;
    printf("[MONSTERCOMBAT] FRAME seq=%u sprite=%u subtype=%u weapon=%u phase=%s frame=%08x worldUs=%u spriteUs=%u hudUs=%u presentUs=%u totalUs=%u presented=%u\n",
           (unsigned int)pending->sequence,
           (unsigned int)pending->spriteIndex,
           (unsigned int)pending->subtype,
           (unsigned int)pending->weapon,
           phase,
           (unsigned int)frame->frameAfterFNV,
           (unsigned int)frame->worldMicros,
           (unsigned int)frame->spriteMicros,
           (unsigned int)frame->hudMicros,
           (unsigned int)frame->presentMicros,
           (unsigned int)frame->totalMicros,
           (unsigned int)frame->finalPresented);
}

static int serviceVisualRedraw(DoomRPG_t* runtime) {
    const EspPlayerViewState* view = EspPlayerView_view();
    EspNativeGameplayFrameStats frame;

    if (combatOwner.visualRedrawPending == 0U) return 1;
    if (runtime == NULL || runtime->render == NULL || view == NULL ||
        view->active != 1U || view->viewAngle != view->destAngle) {
        return 0;
    }
    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(runtime->render,
                                           (uint8_t)view->viewAngle,
                                           &frame)) {
        printf("[MONSTERCOMBAT] CORPSE-REDRAW-FAILED recovery=next-service\n");
        return 1;
    }
    combatOwner.visualRedrawPending = 0U;
    printf("[MONSTERCOMBAT] CORPSE-FRAME frame=%08x presented=%u\n",
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
    return 1;
}

static int servicePending(DoomRPG_t* runtime) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayWeaponSpec* weapon;
    const EspNativeGameplayPlayerState* player;
    EspNativeGameplayMonsterRecord* target;
    EspNativeGameplayMonsterRecord targetBefore;
    EspNativeGameplayPlayerState playerBefore;
    EspNativeGameplayPlayerXpResult xpResult;
    EspNativeGameplayAttackRoll roll;
    MonsterCombatOwner ownerBefore;
    MonsterCombatPending pending;
    Random_t randomBefore;
    EspNativeGameplayFrameStats attackFrame;
    EspNativeGameplayFrameStats rollbackFrame;
    EspNativeGameplayFrameStats settleFrame;
    uint32_t monsterFNVBefore;
    uint32_t monsterFNVAfter;
    uint32_t playerFNVBefore;
    uint32_t playerFNVAfter;
    uint32_t rngCalls;
    uint32_t dropRoll = 0U;
    uint32_t xp = 0U;
    uint16_t consequenceSound = 0U;
    uint8_t ammoBefore = 0U;
    uint8_t ammoAfter = 0U;
    int32_t healthBefore;
    int32_t armorBefore;
    int32_t healthAfter;
    int32_t armorAfter;
    int lethal = 0;
    int gib = 0;

    if (runtime == NULL || runtime->render == NULL || view == NULL ||
        view->active != 1U || view->viewAngle != view->destAngle ||
        combatOwner.pending.active == 0U || !syncOwner()) {
        return 0;
    }

    pending = combatOwner.pending;
    weapon = EspNativeGameplayCombatMath_weapon(pending.weapon);
    target = EspNativeGameplayMonsterState_findMutable(pending.spriteIndex);
    player = EspNativeGameplayPlayerState_view();
    if (weapon == NULL || target == NULL || player == NULL || target->alive == 0U ||
        target->subtype != pending.subtype || target->subtype >= MONSTER_SUBTYPE_COUNT) {
        printf("[MONSTERCOMBAT] DEFER seq=%u sprite=%u reason=stale-transaction mutation=no\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex);
        memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
        return 1;
    }

    targetBefore = *target;
    ownerBefore = combatOwner;
    randomBefore = runtime->random;
    if (!EspNativeGameplayPlayerState_snapshot(&playerBefore)) return 0;
    monsterFNVBefore = currentMonsterFNV();
    playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();
    healthBefore = p1Health(target->param1);
    armorBefore = p1Armor(target->param1);
    healthAfter = healthBefore;
    armorAfter = armorBefore;
    memset(&xpResult, 0, sizeof(xpResult));
    memset(&roll, 0, sizeof(roll));

    if (!EspNativeGameplayPlayerState_consumeAmmo(weapon->ammoType,
                                                   weapon->ammoUsage,
                                                   &ammoBefore,
                                                   &ammoAfter)) {
        printf("[MONSTERCOMBAT] NOAMMO seq=%u sprite=%u weapon=%u ammoType=%u need=%u have=%u mutation=no\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex,
               (unsigned int)pending.weapon,
               (unsigned int)weapon->ammoType,
               (unsigned int)weapon->ammoUsage,
               (unsigned int)ammoBefore);
        memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
        return 1;
    }

    if (!EspNativeGameplayWeapon_armAttack(pending.weapon) ||
        !EspNativeGameplayCombatMath_rollPlayerAttack(runtime,
                                                      pending.weapon,
                                                      EspNativeGameplayPlayerState_view(),
                                                      target,
                                                      pending.worldDistance,
                                                      &roll)) {
        runtime->random = randomBefore;
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        EspNativeGameplayWeapon_cancelAttack();
        combatOwner = ownerBefore;
        combatOwner.pending.active = 0U;
        printf("[MONSTERCOMBAT] FAILED seq=%u sprite=%u reason=attack-contract rngRollback=yes playerRollback=yes monsterMutation=no\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex);
        return 1;
    }

    rngCalls = roll.rngCalls;
    if (roll.hitLoops != 0U) {
        applyPain(target, roll.totalDamage, roll.totalArmorDamage,
                  &healthAfter, &armorAfter);
        lethal = healthAfter <= 0;

        if (lethal && specialDeath(target->subtype)) {
            *target = targetBefore;
            runtime->random = randomBefore;
            (void)EspNativeGameplayPlayerState_restore(&playerBefore);
            EspNativeGameplayWeapon_cancelAttack();
            combatOwner = ownerBefore;
            combatOwner.pending.active = 0U;
            printf("[MONSTERCOMBAT] DEFER seq=%u sprite=%u subtype=%u reason=special-death-family prospectiveDamage=%d+%d hp=%d->0 rngRollback=yes playerRollback=yes monsterRollback=yes mutation=no\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex,
                   (unsigned int)targetBefore.subtype,
                   (int)roll.totalDamage,
                   (int)roll.totalArmorDamage,
                   (int)healthBefore);
            return 1;
        }

        if (lethal) {
            gib = prospectiveGib(&targetBefore,
                                 roll.totalDamage,
                                 roll.totalArmorDamage,
                                 pending.distance);
            target->alive = 0U;
            setGibbed(target->spriteIndex, gib);
            setCorpseReady(target->spriteIndex, 0);
            if (gib) {
                if (combatOwner.deathSpriteIndex == target->spriteIndex) {
                    combatOwner.deathSpriteIndex = MONSTER_NO_SPRITE;
                    combatOwner.deathUntilMs = 0U;
                }
            }
            else {
                if (combatOwner.deathSpriteIndex != MONSTER_NO_SPRITE &&
                    combatOwner.deathSpriteIndex != target->spriteIndex) {
                    setCorpseReady(combatOwner.deathSpriteIndex, 1);
                }
                combatOwner.deathSpriteIndex = target->spriteIndex;
                combatOwner.deathUntilMs = DoomRPG_GetUpTimeMS() +
                                           MONSTER_DEATH_MS;
            }

            /* Exact Entity_died() ordering: XP/possible level-up RNG first,
             * then death-sound RNG (unless gibbed), then one drop RNG word. */
            xp = EspNativeGameplayCombatMath_monsterExp(&targetBefore);
            if (!EspNativeGameplayPlayerState_applyXp(runtime, xp, &xpResult)) {
                *target = targetBefore;
                runtime->random = randomBefore;
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                combatOwner = ownerBefore;
                combatOwner.pending.active = 0U;
                EspNativeGameplayWeapon_cancelAttack();
                printf("[MONSTERCOMBAT] FAILED seq=%u sprite=%u reason=xp-transaction rngRollback=yes playerRollback=yes monsterRollback=yes\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 1;
            }
            rngCalls += xpResult.rngCalls;

            if (gib) {
                consequenceSound = 5091U;
            }
            else {
                consequenceSound = consumeDeathSound(runtime,
                                                      target->subtype,
                                                      &rngCalls);
            }
            dropRoll = (uint32_t)DoomRPG_randNextInt(&runtime->random);
            ++rngCalls;
            combatOwner.view.xpApplied += xp;
            ++combatOwner.view.kills;
            combatOwner.view.painSpriteIndex = MONSTER_NO_SPRITE;
            combatOwner.painUntilMs = 0U;
        }
        else {
            consequenceSound = consumePainSound(runtime,
                                                target->subtype,
                                                &rngCalls);
            combatOwner.view.painSpriteIndex = pending.spriteIndex;
            combatOwner.painUntilMs = DoomRPG_GetUpTimeMS() + MONSTER_PAIN_MS;
        }
        ++combatOwner.view.hits;
        if (roll.gotCrit != 0U) ++combatOwner.view.crits;
    }
    else {
        ++combatOwner.view.misses;
    }
    ++combatOwner.view.attacks;
    combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
    monsterFNVAfter = combatOwner.view.currentMonsterFNV1a;
    playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();

    printf("[MONSTERCOMBAT] ROLL seq=%u sprite=%u subtype=%u mType=%u weapon=%u distance=%u worldDist=%u loops=%u hitLoops=%u firstRandHit=%u firstCalcHit=%d firstCritLimit=%d firstRandDamage=%u totalDamage=%d armorDamage=%d crit=%u rngCalls=%u\n",
           (unsigned int)pending.sequence,
           (unsigned int)pending.spriteIndex,
           (unsigned int)targetBefore.subtype,
           (unsigned int)targetBefore.mType,
           (unsigned int)pending.weapon,
           (unsigned int)pending.distance,
           (unsigned int)pending.worldDistance,
           (unsigned int)roll.loops,
           (unsigned int)roll.hitLoops,
           (unsigned int)roll.randHit[0],
           (int)roll.calcHit[0],
           (int)roll.critLimit[0],
           (unsigned int)roll.randDamage[0],
           (int)roll.totalDamage,
           (int)roll.totalArmorDamage,
           (unsigned int)roll.gotCrit,
           (unsigned int)rngCalls);

    memset(&attackFrame, 0, sizeof(attackFrame));
    if (!EspNativeGameplayFrame_renderTurn(runtime->render,
                                           (uint8_t)view->viewAngle,
                                           &attackFrame)) {
        *target = targetBefore;
        runtime->random = randomBefore;
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        combatOwner = ownerBefore;
        combatOwner.pending.active = 0U;
        EspNativeGameplayWeapon_cancelAttack();
        memset(&rollbackFrame, 0, sizeof(rollbackFrame));
        if (!EspNativeGameplayFrame_renderTurn(runtime->render,
                                               (uint8_t)view->viewAngle,
                                               &rollbackFrame)) {
            printf("[MONSTERCOMBAT] FAILED seq=%u sprite=%u reason=render+rollback-render rngRollback=yes playerRollback=yes monsterRollback=yes\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex);
            return 0;
        }
        printf("[MONSTERCOMBAT] ROLLBACK seq=%u sprite=%u rng=yes player=yes monster=yes frame=%08x\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex,
               (unsigned int)rollbackFrame.frameAfterFNV);
        return 1;
    }

    logFrame(&pending, "attack", &attackFrame);
    if (lethal && !gib) (void)promoteDeathIfDue();
    printf("[MONSTERCOMBAT] COMMIT seq=%u sprite=%u subtype=%u hp=%d->%d armor=%d->%d alive=%u->%u monsterFNV=%08x->%08x playerFNV=%08x->%08x ammo=%u->%u visual=%s attackSound=%u-deferred consequenceSound=%u-deferred xp=%u-applied level=%u->%u levelUps=%u dropRoll=%s%08x dropMaterialize=deferred corpseTrim=deferred turnAdvance=deferred AI=deferred rollback=closed\n",
           (unsigned int)pending.sequence,
           (unsigned int)pending.spriteIndex,
           (unsigned int)targetBefore.subtype,
           (int)healthBefore,
           (int)healthAfter,
           (int)armorBefore,
           (int)armorAfter,
           (unsigned int)targetBefore.alive,
           (unsigned int)target->alive,
           (unsigned int)monsterFNVBefore,
           (unsigned int)monsterFNVAfter,
           (unsigned int)playerFNVBefore,
           (unsigned int)playerFNVAfter,
           (unsigned int)ammoBefore,
           (unsigned int)ammoAfter,
           lethal ? (gib ? "gib-hidden+unlink" : "death4->corpse2/250ms+unlink")
                  : (roll.hitLoops != 0U ? "pain6/250ms" : "none"),
           pending.weapon == 0U ? 5136U : weapon->resourceId,
           (unsigned int)consequenceSound,
           (unsigned int)xp,
           (unsigned int)xpResult.levelBefore,
           (unsigned int)xpResult.levelAfter,
           (unsigned int)xpResult.levelUps,
           lethal ? "value/" : "unused/",
           (unsigned int)dropRoll);

    memset(&settleFrame, 0, sizeof(settleFrame));
    if (EspNativeGameplayFrame_renderTurn(runtime->render,
                                          (uint8_t)view->viewAngle,
                                          &settleFrame)) {
        logFrame(&pending, "settle-idle", &settleFrame);
        if (combatOwner.visualRedrawPending != 0U) {
            combatOwner.visualRedrawPending = 0U;
        }
        printf("[MONSTERCOMBAT] ATTACK seq=%u weapon=%u genericMonster=yes worldCommitted=yes\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.weapon);
    }
    else {
        EspNativeGameplayWeapon_cancelAttack();
        printf("[MONSTERCOMBAT] SETTLE-FAILED seq=%u sprite=%u worldCommitted=yes recovery=next-full-redraw\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex);
    }

    memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
    combatOwner.view.pending = 0U;
    combatOwner.view.pendingSpriteIndex = MONSTER_NO_SPRITE;
    combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
    return 1;
}

EspNativeGameplayActionStatus __wrap_EspNativeGameplayActionEngine_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult) {
    EspNativeGameplayActionStatus status;
    EspNativeGameplayMonsterTraceStatus traceStatus;
    EspNativeGameplayMonsterTarget target;
    const EspNativeGameplayMonsterRecord* monster;
    const EspNativeGameplayWeaponSpec* weapon;
    const EspNativeGameplayHudState* hud;
    uint8_t weaponIndex;

    status = __real_EspNativeGameplayActionEngine_executeSelect(intent, outResult);
    if ((status != ESP_NATIVE_GAMEPLAY_ACTION_NO_EVENT &&
         status != ESP_NATIVE_GAMEPLAY_ACTION_NO_ELIGIBLE) ||
        intent == NULL || intent->action != ESP_NATIVE_GAMEPLAY_ACTION_SELECT ||
        intent->pending != 1U || intent->active != 1U || !syncOwner() ||
        combatOwner.pending.active != 0U) {
        return status;
    }

    hud = EspNativeGameplayHud_view();
    if (hud == NULL || hud->active != 1U || hud->painted != 1U) return status;
    weaponIndex = hud->model.weapon;
    weapon = EspNativeGameplayCombatMath_weapon(weaponIndex);
    if (weapon == NULL) return status;

    memset(&target, 0, sizeof(target));
    traceStatus = EspNativeGameplayMonsterTrace_forward(&target);
    if (traceStatus != ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_FOUND) return status;

    monster = EspNativeGameplayMonsterState_find(target.spriteIndex);
    if (monster == NULL || monster->alive == 0U ||
        monster->subtype != target.subtype) return status;

    if ((STANDARD_WEAPON_DIRECT_MASK & (1U << weaponIndex)) == 0U) {
        printf("[MONSTERCOMBAT] DEFER seq=%u sprite=%u subtype=%u weapon=%u reason=%s backend=generic monsterFamilyOwned=yes mutation=no\n",
               (unsigned int)intent->sequence,
               (unsigned int)target.spriteIndex,
               (unsigned int)target.subtype,
               (unsigned int)weaponIndex,
               weapon->radialDamage != 0U ? "radial-damage-family" :
               (weapon->attackLoops > 1U ? "multi-loop-presentation-family" :
                                           "weapon-entity-rule-family"));
        return status;
    }

    /* The HUD/weapon-pickup owner is still the current presentation source of
     * weapon selection. Mirror that already-proven selection into the shared
     * player gameplay owner; future pickup consolidation removes this bridge. */
    if (!EspNativeGameplayPlayerState_adoptWeapon(weaponIndex)) return status;

    if (weapon->ammoUsage > EspNativeGameplayPlayerState_ammo(weapon->ammoType)) {
        printf("[MONSTERCOMBAT] NOAMMO seq=%u sprite=%u subtype=%u weapon=%u ammoType=%u need=%u have=%u mutation=no uiMessage=deferred\n",
               (unsigned int)intent->sequence,
               (unsigned int)target.spriteIndex,
               (unsigned int)target.subtype,
               (unsigned int)weaponIndex,
               (unsigned int)weapon->ammoType,
               (unsigned int)weapon->ammoUsage,
               (unsigned int)EspNativeGameplayPlayerState_ammo(weapon->ammoType));
        return status;
    }

    memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
    combatOwner.pending.sequence = intent->sequence;
    combatOwner.pending.spriteIndex = target.spriteIndex;
    combatOwner.pending.tileIndex = target.tileIndex;
    combatOwner.pending.worldDistance = target.worldDistance;
    combatOwner.pending.subtype = target.subtype;
    combatOwner.pending.distance = target.distance;
    combatOwner.pending.weapon = weaponIndex;
    combatOwner.pending.active = 1U;
    combatOwner.view.pending = 1U;
    combatOwner.view.pendingSpriteIndex = target.spriteIndex;

    printf("[MONSTERCOMBAT] ARM seq=%u sprite=%u tile=%u subtype=%u mType=%u weapon=%u distance=%u hp=%u/%u armor=%u/%u def=%u agi=%u playerAcc=%u playerStr=%u backend=generic-type1 rng=pending mutation=no rollback=armed\n",
           (unsigned int)intent->sequence,
           (unsigned int)target.spriteIndex,
           (unsigned int)target.tileIndex,
           (unsigned int)monster->subtype,
           (unsigned int)monster->mType,
           (unsigned int)weaponIndex,
           (unsigned int)target.distance,
           (unsigned int)p1Health(monster->param1),
           (unsigned int)p1MaxHealth(monster->param1),
           (unsigned int)p1Armor(monster->param1),
           (unsigned int)p1MaxArmor(monster->param1),
           (unsigned int)p2Defense(monster->param2),
           (unsigned int)p2Agility(monster->param2),
           (unsigned int)EspNativeGameplayPlayerState_accuracy(),
           (unsigned int)EspNativeGameplayPlayerState_strength());
    return status;
}

int __wrap_EspMapSpriteTopology_getVisualState(uint32_t spriteIndex,
                                               uint8_t* outVisualState) {
    const EspNativeGameplayMonsterRecord* monster;
    if (!EspNativeGameplayActionEngine_getVisualState(spriteIndex,
                                                       outVisualState)) {
        return 0;
    }
    if (outVisualState == NULL || !syncOwner()) return 1;
    monster = EspNativeGameplayMonsterState_find((uint16_t)spriteIndex);
    if (monster == NULL) return 1;

    if (monster->alive == 0U) {
        if (gibbed((uint16_t)spriteIndex)) {
            *outVisualState |= MONSTER_VISUAL_HIDDEN;
        }
        else {
            *outVisualState = (uint8_t)((*outVisualState & 0xf0U) |
                                        (corpseReady((uint16_t)spriteIndex)
                                             ? MONSTER_CORPSE_VISUAL
                                             : MONSTER_DEATH_VISUAL));
        }
    }
    else if (combatOwner.view.painSpriteIndex == spriteIndex) {
        *outVisualState = (uint8_t)((*outVisualState & 0xf0U) |
                                    MONSTER_PAIN_VISUAL);
    }
    return 1;
}

int __wrap_EspMapSpriteTopology_getEntity(uint32_t spriteIndex,
                                          uint8_t* outType,
                                          uint8_t* outSubType,
                                          uint16_t* outLinkState,
                                          uint16_t* outLinkOrder) {
    const EspNativeGameplayMonsterRecord* monster;
    if (!EspNativeGameplayActionEngine_getEntity(spriteIndex, outType, outSubType,
                                                  outLinkState, outLinkOrder)) {
        return 0;
    }
    if (outType == NULL || *outType != MONSTER_TYPE_ENEMY || !syncOwner()) {
        return 1;
    }
    monster = EspNativeGameplayMonsterState_find((uint16_t)spriteIndex);
    if (monster != NULL && monster->alive == 0U) {
        if (outLinkState != NULL) {
            *outLinkState &= (uint16_t)~(ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                                         ESP_MAP_SPRITE_TOPOLOGY_ALIVE);
        }
        if (outLinkOrder != NULL) *outLinkOrder = 0U;
    }
    return 1;
}

int __wrap_EspNativeGameplayActionEngine_service(DoomRPG_t* runtime) {
    if (!EspNativeGameplayMonsterState_actionService(runtime)) return 0;
    if (!syncOwner()) return 0;
    if (combatOwner.pending.active != 0U) {
        return servicePending(runtime);
    }
    return serviceVisualRedraw(runtime);
}

void __wrap_EspNativeGameplayActionEngine_reset(void) {
    /* PlayerState is session-permanent and intentionally survives map resets.
     * A later native new-game owner will reset it exactly once per new game. */
    EspNativeGameplayMonsterCombat_reset();
    EspNativeGameplayMonsterState_actionReset();
}
