#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "DoomRPG.h"
#include "esp_native_root_state.h"
#include "esp_native_process_rng_owner.h"
#include "Render.h"

#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_projection_overlay.h"
#include "esp_native_gameplay_combat_math.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_hit_feedback.h"
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_hub_action_gate.h"
#include "esp_native_gameplay_level_up.h"
#include "esp_native_gameplay_monster_combat.h"
#include "esp_native_monster_death_consequence.h"
#include "esp_native_audio_intent.h"
#include "esp_native_gameplay_monster_pain_tint.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_drop.h"
#include "esp_native_gameplay_monster_trace.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_native_gameplay_player_death.h"
#include "platform_video_c_bridge.h"
#include "esp_native_gameplay_weapon.h"
#include "esp_player_view_state.h"

/* RC20: public composed topology projection is explicitly owned here.
 * ActionEngine and raw map topology remain private read layers. */

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
/* 16 barrels per chain, at most 8 monster hits per blast: 256B static.
 * Resource IDs fit 15 bits; high bit encodes death priority 5 vs pain 2. */
#define MONSTER_SPLASH_AUDIO_CAPACITY (16U * ESP_NATIVE_GAMEPLAY_SPLASH_MAX_HITS)
#define MONSTER_SPLASH_AUDIO_DEATH_FLAG 0x8000U

/* Direct single-target standard weapons currently have complete native combat
 * semantics. Fire Ext is also direct against eType==1 monsters in the legacy
 * player combat path; only its impact presentation differs (gray spray on
 * Phantom subtype 4, no blood spray on other monster subtypes). Multi-loop
 * presentation (chaingun/plasma), radial damage (rocket/BFG) and dog-familiar
 * weapons remain separate mechanical families. */
#define STANDARD_WEAPON_DIRECT_MASK ((1U << 0) | (1U << 1) | (1U << 2) | \
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
    uint8_t deathFinalGib;
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

/*
 * servicePending() nests the native world/sprite renderer below its transaction
 * frame. A full MonsterCombatOwner rollback copy is 324 B on the ESP32 build;
 * keeping it automatic unnecessarily consumes loopTask stack throughout every
 * attack render. The gameplay service is serialized on loopTask, so one bounded
 * non-reentrant BSS rollback owner is sufficient and mirrors the established
 * HUB/save stack-canary recovery pattern.
 */
static MonsterCombatOwner combatRollbackOwner;
/* Post-commit audio presentation only: not in rollback or checkpoint owner. */
static uint32_t painSoundHoldUntilMs;
static uint16_t painSoundHoldSprite;
static EspNativeGameplayPlayerXpResult splashLevelUp;
static uint32_t splashLevelUpSequence;
static uint8_t splashSequenceActive;
static uint32_t splashSequenceNumber;
/* Non-allocating post-commit sound intents, never part of world mutation. */
static uint16_t splashAudioCues[MONSTER_SPLASH_AUDIO_CAPACITY];
static uint32_t splashAudioSequence;
static uint8_t splashAudioCount;
_Static_assert(sizeof(MonsterCombatOwner) <= 384U,
               "monster combat rollback owner must remain small and bounded");

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
    uint8_t finalGib;
    if (combatOwner.deathSpriteIndex == MONSTER_NO_SPRITE) return 0;
    if ((int32_t)(DoomRPG_GetUpTimeMS() - combatOwner.deathUntilMs) < 0) {
        return 0;
    }
    spriteIndex = combatOwner.deathSpriteIndex;
    finalGib = combatOwner.deathFinalGib;
    if (finalGib != 0U) {
        setGibbed(spriteIndex, 1);
        setCorpseReady(spriteIndex, 0);
    }
    else {
        setGibbed(spriteIndex, 0);
        setCorpseReady(spriteIndex, 1);
    }
    combatOwner.deathSpriteIndex = MONSTER_NO_SPRITE;
    combatOwner.deathUntilMs = 0U;
    combatOwner.deathFinalGib = 0U;
    combatOwner.visualRedrawPending = 1U;
    printf("[MONSTERCOMBAT] DEATH-SETTLE sprite=%u visual=%u->%s delayMs=%u gib=%u gibFX=%s immutableSprite=yes\n",
           (unsigned int)spriteIndex,
           (unsigned int)MONSTER_DEATH_VISUAL,
           finalGib != 0U ? "hidden" : "corpse2",
           (unsigned int)MONSTER_DEATH_MS,
           (unsigned int)finalGib,
           finalGib != 0U ? "deferred" : "n/a");
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
        {
            uint32_t i;
            uint32_t restoredDead = 0U;
            /*
             * When a session owner is reconstructed from a checkpoint, logical
             * dead/alive state is authoritative but V8 does not yet persist the
             * transient death-presentation lease (death4/gib bitsets). Normalize
             * every already-dead record to the stable corpse visual immediately.
             * getEntity() below also masks LINKED|ALIVE, so these restored corpses
             * neither block nor reactivate gameplay. Exact corpse-vs-gib replay is
             * a later presentation-only checkpoint family.
             */
            for (i = 0U; i < monsters->count; ++i) {
                if (monsters->records[i].alive == 0U) {
                    const int hidden = monsters->records[i].subtype == 8U;
                    setGibbed(monsters->records[i].spriteIndex, hidden);
                    setCorpseReady(monsters->records[i].spriteIndex, !hidden);
                    ++restoredDead;
                }
            }
            if (restoredDead != 0U) {
                printf("[MONSTERCOMBAT] RESUME-DEAD normalized=%u visual=corpse2-or-subtype8-hidden linked=masked alive=masked exactOrdinaryGibReplay=deferred\n",
                       (unsigned int)restoredDead);
            }
        }
        combatOwner.view.standardWeaponsOwned =
            (uint8_t)STANDARD_WEAPON_DIRECT_MASK;
        combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
        combatOwner.view.active = 1U;
        printf("[MONSTERCOMBAT] READY arena=%08x monsters=%u backend=type1-generic subtypes=0..13 trace=shared combatMath=shared playerState=%uB playerFNV=%08x directWeaponMask=%02x specialDeathMask=%04x deathVisual=4->(corpse2|gibHidden)/%ums gibFX=deferred rollbackOwner=%uB/static renderStats=single-frame legacyEntity=no\n",
               (unsigned int)arena,
               (unsigned int)monsters->count,
               (unsigned int)sizeof(*player),
               (unsigned int)EspNativeGameplayPlayerState_fingerprint(),
               (unsigned int)combatOwner.view.standardWeaponsOwned,
               (unsigned int)SPECIAL_DEATH_MASK,
               (unsigned int)MONSTER_DEATH_MS,
               (unsigned int)sizeof(combatRollbackOwner));
    }
    expirePain();
    if (combatOwner.pending.active == 0U) (void)promoteDeathIfDue();
    return 1;
}

void EspNativeGameplayMonsterCombat_reset(void) {
    EspNativeGameplayMonsterDrop_reset();
    memset(&combatOwner, 0, sizeof(combatOwner));
    memset(&combatRollbackOwner, 0, sizeof(combatRollbackOwner));
    memset(&splashLevelUp, 0, sizeof(splashLevelUp));
    splashLevelUpSequence = 0U;
    splashSequenceActive = 0U;
    splashSequenceNumber = 0U;
    splashAudioSequence = 0U;
    splashAudioCount = 0U;
    painSoundHoldUntilMs=0U;
    painSoundHoldSprite=MONSTER_NO_SPRITE;
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

int EspNativeGameplayMonsterCombat_damageBlend(
    uint16_t spriteIndex, uint32_t nowMs) {
    if (combatOwner.view.active == 0U) return 0;
    return EspNativeMonsterPainTint_active(
        nowMs, combatOwner.painUntilMs,
        combatOwner.view.painSpriteIndex, spriteIndex);
}

int EspNativeGameplayMonsterCombat_painSettling(
    uint16_t spriteIndex, uint32_t nowMs) {
    return combatOwner.view.active == 1U &&
           ((combatOwner.view.painSpriteIndex == spriteIndex &&
             combatOwner.painUntilMs != 0U &&
             (int32_t)(nowMs-combatOwner.painUntilMs) < 0) ||
            (painSoundHoldSprite == spriteIndex &&
             painSoundHoldUntilMs != 0U &&
             (int32_t)(nowMs-painSoundHoldUntilMs) < 0));
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
    (void)DoomRPG_randNextByte(EspNativeProcessRng_live());
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
    pick = DoomRPG_randNextByte(EspNativeProcessRng_live());
    ++(*ioRngCalls);
    return deathSounds[subtype][pick % count];
}

/* MONSTER-DEATH-P1: shared post-XP, pre-drop-commit stage for the direct
 * and barrel-radius pathways. The CALLER has already applied XP (including
 * any level-up RNG), and still owns the complete world/player/RNG rollback.
 * This prepares no persistent drops and publishes no sound: strictly
 * death-sound RNG (or gib 5091 without sound RNG), then one drop RNG word,
 * then a pure drop plan. A failed plan MUST be undone by the caller's
 * existing transaction snapshot. Special deaths are rejected upstream. */
static int prepareDeathAfterXp(
    DoomRPG_t* runtime,
    const EspNativeGameplayMonsterRecord* before,
    uint8_t gib,
    uint32_t* ioRngCalls,
    EspNativeGameplayMonsterDropSpawnPlan* outPlan,
    uint16_t* outSound,
    uint32_t* outDropRoll) {
    if (runtime == NULL || before == NULL || ioRngCalls == NULL ||
        outPlan == NULL || outSound == NULL || outDropRoll == NULL ||
        before->subtype >= MONSTER_SUBTYPE_COUNT || gib > 1U ||
        (specialDeath(before->subtype) && before->subtype != 8U)) return 0;
    *outSound = gib != 0U
        ? 5091U : consumeDeathSound(runtime, before->subtype, ioRngCalls);
    *outDropRoll = (uint32_t)DoomRPG_randNextInt(EspNativeProcessRng_live());
    ++(*ioRngCalls);
    return EspNativeGameplayMonsterDrop_prepare(*outDropRoll, before, outPlan);
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
    if (runtime == NULL || EspNativeRootOwners_live()->render == NULL || view == NULL ||
        view->active != 1U || view->viewAngle != view->destAngle) {
        return 0;
    }
    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(EspNativeRootOwners_live()->render,
                                           (uint8_t)view->viewAngle,
                                           &frame)) {
        printf("[MONSTERCOMBAT] DEATH-REDRAW-FAILED recovery=next-service\n");
        return 1;
    }
    combatOwner.visualRedrawPending = 0U;
    printf("[MONSTERCOMBAT] DEATH-FRAME frame=%08x presented=%u\n",
           (unsigned int)frame.frameAfterFNV,
           (unsigned int)frame.finalPresented);
    return 1;
}

/* Reuse the splash snapshot for direct subtype8 collateral rollback. */
static int
restoreSplashSnapshot(DoomRPG_t* runtime);

static int servicePending(DoomRPG_t* runtime) {
    const EspPlayerViewState* view = EspPlayerView_view();
    const EspNativeGameplayWeaponSpec* weapon;
    const EspNativeGameplayPlayerState* player;
    EspNativeGameplayMonsterRecord* target;
    EspNativeGameplayMonsterRecord targetBefore;
    EspNativeGameplayPlayerState playerBefore;
    EspNativeGameplayPlayerXpResult xpResult;
    EspNativeGameplayMonsterDropSpawnPlan dropPlan;
    EspNativeGameplayAttackRoll roll;
    MonsterCombatPending pending;
    Random_t randomBefore;
    EspNativeGameplayFrameStats frame;
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
    char hitMessage[24];
    int hitMessageQueued = 0;
    int levelUpPopup = 0;
    int dropMaterialized = 0;
    int hitFxArmed = 0;
    int32_t healthBefore;
    int32_t armorBefore;
    int32_t healthAfter;
    int32_t armorAfter;
    int lethal = 0;
    int gib = 0;
    int deathBlast = 0;
    int blastSnapshot = 0;
    EspNativeGameplayDeathBlastPlan blastPlan = {0};
    EspNativeGameplayDeathBlastResult blastResult = {0};

    if (runtime == NULL || EspNativeRootOwners_live()->render == NULL || view == NULL ||
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
    combatRollbackOwner = combatOwner;
    randomBefore = (*EspNativeProcessRng_live());
    if (!EspNativeGameplayPlayerState_snapshot(&playerBefore)) return 0;
    monsterFNVBefore = currentMonsterFNV();
    playerFNVBefore = EspNativeGameplayPlayerState_fingerprint();
    healthBefore = p1Health(target->param1);
    armorBefore = p1Armor(target->param1);
    healthAfter = healthBefore;
    armorAfter = armorBefore;
    memset(&xpResult, 0, sizeof(xpResult));
    memset(&dropPlan, 0, sizeof(dropPlan));
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
        (*EspNativeProcessRng_live()) = randomBefore;
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        EspNativeGameplayWeapon_cancelAttack();
        combatOwner = combatRollbackOwner;
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

        if (lethal && target->subtype == 8U) {
            deathBlast = EspNativeGameplayActionEngine_prepareDeathBlast(
                target->spriteIndex, &blastPlan);
        }
        if (lethal && specialDeath(target->subtype) && !deathBlast) {
            *target = targetBefore;
            (*EspNativeProcessRng_live()) = randomBefore;
            (void)EspNativeGameplayPlayerState_restore(&playerBefore);
            EspNativeGameplayWeapon_cancelAttack();
            combatOwner = combatRollbackOwner;
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
            uint16_t previousDeathSprite;
            uint8_t previousDeathGib;

            gib = prospectiveGib(&targetBefore,
                                 roll.totalDamage,
                                 roll.totalArmorDamage,
                                 pending.distance);
            target->alive = 0U;

            /* One generic presentation state machine owns every ordinary
             * monster death. Even overkill/gib deaths first render the recovered
             * legacy death pose (visual 4). Only after that 250 ms lease do they
             * become either corpse2 or hidden. The eventual native particle/gib
             * effect remains a separate presentation family. */
            setGibbed(target->spriteIndex, 0);
            setCorpseReady(target->spriteIndex, 0);
            previousDeathSprite = combatOwner.deathSpriteIndex;
            previousDeathGib = combatOwner.deathFinalGib;
            if (previousDeathSprite != MONSTER_NO_SPRITE &&
                previousDeathSprite != target->spriteIndex) {
                if (previousDeathGib != 0U) {
                    setGibbed(previousDeathSprite, 1);
                    setCorpseReady(previousDeathSprite, 0);
                }
                else {
                    setGibbed(previousDeathSprite, 0);
                    setCorpseReady(previousDeathSprite, 1);
                }
            }
            if (deathBlast) {
                /* Legacy subtype8 hides immediately, gib or normal death. */
                setGibbed(target->spriteIndex, 1);
                setCorpseReady(target->spriteIndex, 0);
                combatOwner.deathSpriteIndex = MONSTER_NO_SPRITE;
                combatOwner.deathFinalGib = 0U;
                combatOwner.deathUntilMs = 0U;
            } else {
                combatOwner.deathSpriteIndex = target->spriteIndex;
                combatOwner.deathFinalGib = gib ? 1U : 0U;
                combatOwner.deathUntilMs = DoomRPG_GetUpTimeMS() +
                                           MONSTER_DEATH_MS;
            }

            /* Exact Entity_died() ordering: XP/possible level-up RNG first,
             * then death-sound RNG (unless gibbed), then one drop RNG word. */
            xp = EspNativeGameplayCombatMath_monsterExp(&targetBefore);
            if (!EspNativeGameplayPlayerState_applyXp(runtime, xp, &xpResult)) {
                *target = targetBefore;
                (*EspNativeProcessRng_live()) = randomBefore;
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                combatOwner = combatRollbackOwner;
                combatOwner.pending.active = 0U;
                EspNativeGameplayWeapon_cancelAttack();
                printf("[MONSTERCOMBAT] FAILED seq=%u sprite=%u reason=xp-transaction rngRollback=yes playerRollback=yes monsterRollback=yes\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 1;
            }
            rngCalls += xpResult.rngCalls;
            if (xpResult.levelUps != 0U) levelUpPopup = 1;

            if (!prepareDeathAfterXp(
                    runtime, &targetBefore, (uint8_t)gib, &rngCalls,
                    &dropPlan, &consequenceSound, &dropRoll)) {
                *target = targetBefore;
                (*EspNativeProcessRng_live()) = randomBefore;
                (void)EspNativeGameplayPlayerState_restore(&playerBefore);
                combatOwner = combatRollbackOwner;
                combatOwner.pending.active = 0U;
                EspNativeGameplayWeapon_cancelAttack();
                printf("[MONSTERDROP] FAILED seq=%u sprite=%u reason=prepare rngRollback=yes playerRollback=yes monsterRollback=yes\n",
                       (unsigned int)pending.sequence,
                       (unsigned int)pending.spriteIndex);
                return 1;
            }
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

    /*
     * Legacy blood is an impact-time effect, not a post-settle decoration.
     * Provisionally arm it before the attack-frame render so the same physical
     * present that shows pain/death also carries the spray. If render fails,
     * the owner is cancelled before the rollback redraw below.
     */
    if (roll.hitLoops != 0U) {
        hitFxArmed = EspNativeGameplayHitFeedback_arm(
            pending.sequence,
            pending.spriteIndex,
            pending.weapon,
            pending.distance,
            healthBefore,
            armorBefore,
            roll.totalDamage,
            roll.totalArmorDamage);
    }

    memset(&frame, 0, sizeof(frame));
    if (!EspNativeGameplayFrame_renderTurn(EspNativeRootOwners_live()->render,
                                           (uint8_t)view->viewAngle,
                                           &frame)) {
        *target = targetBefore;
        (*EspNativeProcessRng_live()) = randomBefore;
        (void)EspNativeGameplayPlayerState_restore(&playerBefore);
        combatOwner = combatRollbackOwner;
        combatOwner.pending.active = 0U;
        EspNativeGameplayWeapon_cancelAttack();
        if (hitFxArmed) {
            (void)EspNativeGameplayHitFeedback_cancel(pending.sequence);
            hitFxArmed = 0;
        }
        memset(&frame, 0, sizeof(frame));
        if (!EspNativeGameplayFrame_renderTurn(EspNativeRootOwners_live()->render,
                                               (uint8_t)view->viewAngle,
                                               &frame)) {
            printf("[MONSTERCOMBAT] FAILED seq=%u sprite=%u reason=render+rollback-render rngRollback=yes playerRollback=yes monsterRollback=yes\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex);
            return 0;
        }
        printf("[MONSTERCOMBAT] ROLLBACK seq=%u sprite=%u rng=yes player=yes monster=yes frame=%08x\n",
               (unsigned int)pending.sequence,
               (unsigned int)pending.spriteIndex,
               (unsigned int)frame.frameAfterFNV);
        return 1;
    }

    /*
     * Attack-frame render succeeded: gameplay rollback is now closed. The
     * legacy drop pool is materialized only now so a failed attack-frame render
     * never has to roll back a dynamic slot. No other gameplay service can
     * modify this serialized owner during the synchronous frame render.
     */
    if (deathBlast) {
        /* Root HP/XP/RNG remains provisional in the original direct
         * journal. Snapshot all collateral owners BEFORE root drop commit. */
        combatOwner.pending.active = 0U;
        if (!EspNativeGameplayMonsterCombat_beginSplashSequence(
                runtime, pending.sequence)) goto death_blast_failed;
        blastSnapshot = 1;
        if (dropPlan.outcome == ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SPAWN) {
            if (!EspNativeGameplayMonsterDrop_commit(&dropPlan))
                goto death_blast_failed;
            dropMaterialized = 1;
        }
        /* Opening cues describe the already presented attack. Collateral
         * audio remains provisional until the complete transaction commits. */
        if (frame.finalPresented != 0U) {
            const uint16_t shotSound = pending.weapon == 0U
                ? 5136U : weapon->resourceId;
            if (shotSound != 0U)
                (void)EspNativeAudioIntent_publish(shotSound, 0U, 5U);
            if (consequenceSound != 0U)
                (void)EspNativeAudioIntent_publish(consequenceSound, 0U,
                    EspNativeMonsterDeathConsequence_priority(1U, (uint8_t)gib));
        }
        if (!EspNativeGameplayActionEngine_runDeathBlast(
                runtime, pending.sequence, &blastPlan, &blastResult))
            goto death_blast_failed;
        if (blastResult.lethal != 0U) {
            const uint16_t deathTile = (uint16_t)(
                (view->destY >> 6) * 32 + (view->destX >> 6));
            if (!EspNativeGameplayPlayerDeath_armWithReservedRand(
                    runtime, pending.sequence, deathTile, blastResult.deathRand))
                goto death_blast_failed;
            if (!EspNativeGameplayMonsterCombat_commitTerminalSplashSequence(
                    pending.sequence)) return 0; /* death already armed */
        } else if (!EspNativeGameplayMonsterCombat_commitSplashSequence(
                       pending.sequence)) goto death_blast_failed;
        blastSnapshot = 0;
        if (!EspNativeGameplayMonsterCombat_publishSplashAudioAfterCommit(
                pending.sequence)) return 0;
        monsterFNVAfter = currentMonsterFNV();
        playerFNVAfter = EspNativeGameplayPlayerState_fingerprint();
        printf("[MONSTERBLAST] COMMIT seq=%u source=%u hidden=yes gib=%u monsters=%u playerDamage=%u lethal=%u rollback=closed\n",
               (unsigned int)pending.sequence, (unsigned int)pending.spriteIndex,
               (unsigned int)gib, (unsigned int)blastResult.monsterHits,
               (unsigned int)blastResult.playerMessageDamage,
               (unsigned int)blastResult.lethal);
        if (blastResult.lethal != 0U) {
            memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
            combatOwner.view.pending = 0U;
            combatOwner.view.pendingSpriteIndex = MONSTER_NO_SPRITE;
            combatOwner.view.currentMonsterFNV1a = monsterFNVAfter;
            (void)Esp32PlatformVideo_present();
            return 1;
        }
        if (splashLevelUpSequence != 0U) levelUpPopup = 0;
    }

    if (lethal && !deathBlast &&
        dropPlan.outcome == ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SPAWN) {
        if (!EspNativeGameplayMonsterDrop_commit(&dropPlan)) {
            printf("[MONSTERDROP] FAILED seq=%u sprite=%u reason=post-render-commit fatal=yes\n",
                   (unsigned int)pending.sequence,
                   (unsigned int)pending.spriteIndex);
            return 0;
        }
        dropMaterialized = 1;
    }

    /*
     * Queue the result text here (legacy stage-2 timing) while the already-armed
     * blood remains a separate impact-time presentation lease.
     */
    memset(hitMessage, 0, sizeof(hitMessage));
    if (deathBlast && blastResult.playerMessageDamage != 0U) {
        (void)snprintf(hitMessage, sizeof(hitMessage), "%u damage!",
                       (unsigned int)blastResult.playerMessageDamage);
        hitMessageQueued = EspNativeGameplayActionEngine_queueTextFeedback(
            ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE, hitMessage, 500U);
    } else if (roll.hitLoops != 0U) {
        const int32_t visibleDamage =
            roll.totalDamage + roll.totalArmorDamage;
        (void)snprintf(hitMessage, sizeof(hitMessage),
                       roll.gotCrit != 0U ? "Crit! %d damage!" : "%d damage!",
                       (int)visibleDamage);
        hitMessageQueued =
            EspNativeGameplayActionEngine_queueTextFeedback(
                ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PLAYER_HIT,
                hitMessage, 0U);
    }
    else {
        (void)snprintf(hitMessage, sizeof(hitMessage),
                       pending.weapon == 1U ? "No effect!" : "Missed!");
        hitMessageQueued =
            EspNativeGameplayActionEngine_queueTextFeedback(
                ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PLAYER_HIT,
                hitMessage, 0U);
    }
    printf("[MONSTERHITFEEDBACK] ARM seq=%u sprite=%u hit=%u crit=%u message=%s textQueued=%s impact=%s timing=%s deathSuffix=%s mutation=no gameplayRng=untouched\n",
           (unsigned int)pending.sequence,
           (unsigned int)pending.spriteIndex,
           (unsigned int)(roll.hitLoops != 0U),
           (unsigned int)roll.gotCrit,
           hitMessage,
           hitMessageQueued ? "yes" : "deferred-owner-busy",
           roll.hitLoops != 0U
               ? (pending.weapon == 1U
                      ? (targetBefore.subtype == 4U
                             ? (hitFxArmed ? "extinguisher-gray-armed"
                                           : "extinguisher-gray-deferred")
                             : "none-extinguisher")
                      : (hitFxArmed ? "blood-armed" : "blood-deferred"))
               : "none-miss",
           roll.hitLoops != 0U ? "attack-frame" : "n/a",
           lethal ? "deferred-bounded-topbar" : "n/a");

    logFrame(&pending, "attack", &frame);
    /* Ordinary deaths publish here; subtype8 opening cues have already
     * played at the visible attack/blast frame boundary. */
    if (!deathBlast) {
        const uint16_t shotSound=pending.weapon==0U ? 5136U : weapon->resourceId;
        if (shotSound!=0U) (void)EspNativeAudioIntent_publish(shotSound,0U,5U);
        if (consequenceSound!=0U) {
            (void)EspNativeAudioIntent_publish(consequenceSound,0U,
                EspNativeMonsterDeathConsequence_priority(
                    (uint8_t)lethal, (uint8_t)gib));
            /* Visual pain began before the attack frame was rendered. Anchor
             * a separate short audio hold to this actual sound publication. */
            if (!lethal && EspNativeAudioIntent_isSoundEnabled()) {
                painSoundHoldSprite=pending.spriteIndex;
                painSoundHoldUntilMs=DoomRPG_GetUpTimeMS()+MONSTER_PAIN_MS;
            }
        }
        printf("[MONSTERSFX] HIT seq=%u mType=%u shot=%u result=%u lethal=%u rollback=closed\n",
               (unsigned)pending.sequence,(unsigned)targetBefore.mType,
               (unsigned)shotSound,(unsigned)consequenceSound,(unsigned)lethal);
    }
    if (lethal) (void)promoteDeathIfDue();
    printf("[MONSTERCOMBAT] COMMIT seq=%u sprite=%u subtype=%u hp=%d->%d armor=%d->%d alive=%u->%u monsterFNV=%08x->%08x playerFNV=%08x->%08x ammo=%u->%u visual=%s attackSound=%u-deferred consequenceSound=%u-deferred xp=%u-applied level=%u->%u levelUps=%u dropRoll=%s%08x dropMaterialize=%s corpseTrim=deferred turnAdvance=deferred AI=deferred rollback=closed\n",
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
           deathBlast ? "hidden+type1-blast" :
           lethal ? (gib ? "death4->gib-hidden/250ms+unlink,gibFX-deferred"
                         : "death4->corpse2/250ms+unlink")
                  : (roll.hitLoops != 0U ? "pain6/250ms" : "none"),
           pending.weapon == 0U ? 5136U : weapon->resourceId,
           (unsigned int)consequenceSound,
           (unsigned int)xp,
           (unsigned int)xpResult.levelBefore,
           (unsigned int)xpResult.levelAfter,
           (unsigned int)xpResult.levelUps,
           lethal ? "value/" : "unused/",
           (unsigned int)dropRoll,
           lethal
               ? (dropMaterialized != 0
                      ? "live"
                      : EspNativeGameplayMonsterDrop_outcomeName(dropPlan.outcome))
               : "unused");

    memset(&frame, 0, sizeof(frame));
    if (EspNativeGameplayFrame_renderTurn(EspNativeRootOwners_live()->render,
                                          (uint8_t)view->viewAngle,
                                          &frame)) {
        logFrame(&pending, "settle-idle", &frame);
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

    if (deathBlast && splashLevelUpSequence != 0U &&
        !EspNativeGameplayMonsterCombat_finishSplashPresentation(
            pending.sequence)) return 0;
    if (levelUpPopup != 0) {
        const int levelUpOk =
            EspNativeGameplayLevelUp_begin(&xpResult, pending.sequence);
        printf("[LEVELUP] ARM seq=%u level=%u->%u levelUps=%u status=%s owner=dedicated-fullscreen continuation=explicit-tap monsterTurn=legacy-skip-while-levelup-active sound=5043-deferred\n",
               (unsigned int)pending.sequence,
               (unsigned int)xpResult.levelBefore,
               (unsigned int)xpResult.levelAfter,
               (unsigned int)xpResult.levelUps,
               levelUpOk ? "OK" : "FAILED");
    }

    memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
    combatOwner.view.pending = 0U;
    combatOwner.view.pendingSpriteIndex = MONSTER_NO_SPRITE;
    combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
    return 1;

death_blast_failed:
    {
        int exact = 1;
        /* A failed cell may have already restored/closed this snapshot.
         * Re-apply it explicitly to verify every collateral owner. */
        if (blastSnapshot && !restoreSplashSnapshot(runtime)) exact = 0;
        target = EspNativeGameplayMonsterState_findMutable(pending.spriteIndex);
        if (target == NULL) exact = 0;
        else *target = targetBefore;
        (*EspNativeProcessRng_live()) = randomBefore;
        if (!EspNativeGameplayPlayerState_restore(&playerBefore)) exact = 0;
        combatOwner = combatRollbackOwner;
        memset(&combatOwner.pending, 0, sizeof(combatOwner.pending));
        combatOwner.view.pending = 0U;
        combatOwner.view.pendingSpriteIndex = MONSTER_NO_SPRITE;
        EspNativeGameplayWeapon_cancelAttack();
        if (hitFxArmed) (void)EspNativeGameplayHitFeedback_cancel(pending.sequence);
        memset(&frame, 0, sizeof(frame));
        if (!EspNativeGameplayFrame_renderTurn(EspNativeRootOwners_live()->render,
                                               (uint8_t)view->viewAngle, &frame)) exact = 0;
        printf("[MONSTERBLAST] ROLLBACK seq=%u source=%u exact=%s root+collateral+drop+player+rng=restored\n",
               (unsigned int)pending.sequence, (unsigned int)pending.spriteIndex,
               exact ? "yes" : "NO");
        return exact;
    }
}


/*
 * Barrel radius calls Game_hurtEntityAt() in cardinal-then-diagonal order.
 * A single barrel applies its one monster splash after its blast word.
 * A multi-barrel chain applies a splash after EACH explosion so pain/death/
 * drop RNG consumption happens before the next barrel's blast word, exactly
 * as the legacy Game_gsprite_update() sequence. The shared BSS snapshot
 * permits rollback across every wave if later native consequences fail.
 */
typedef struct MonsterSplashRollback_s {
    EspNativeGameplayMonsterStateSnapshot monsters;
    EspNativeGameplayMonsterDropSnapshot drops;
    EspNativeGameplayPlayerState player;
    MonsterCombatOwner combat;
    Random_t random;
} MonsterSplashRollback;

static MonsterSplashRollback splashRollback;

/* The rollback image is BSS-bounded and shared between ordinary single-blast
 * splash and a multi-wave/monster transaction. Do not allocate per monster or
 * per wave. A multi-wave transaction keeps the ORIGINAL snapshot across all
 * subsequent splash calls so a later failure undoes earlier waves as well. */
static int restoreSplashSnapshot(DoomRPG_t* runtime) {
    int exact = 1;
    if (runtime == NULL) return 0;
    if (!EspNativeGameplayMonsterDrop_restore(&splashRollback.drops))
        exact = 0;
    if (!EspNativeGameplayMonsterState_restoreInPlace(
            &splashRollback.monsters)) exact = 0;
    if (!EspNativeGameplayPlayerState_restore(&splashRollback.player))
        exact = 0;
    (*EspNativeProcessRng_live()) = splashRollback.random;
    combatOwner = splashRollback.combat;
    splashSequenceActive = 0U;
    splashSequenceNumber = 0U;
    splashAudioSequence = 0U;
    splashAudioCount = 0U;
    splashLevelUpSequence = 0U;
    memset(&splashLevelUp, 0, sizeof(splashLevelUp));
    return exact;
}

int EspNativeGameplayMonsterCombat_beginSplashSequence(
    DoomRPG_t* runtime, uint32_t sequence) {
    if (runtime == NULL || sequence == 0U ||
        splashSequenceActive != 0U || splashLevelUpSequence != 0U ||
        !syncOwner() || combatOwner.pending.active != 0U ||
        !EspNativeGameplayMonsterState_snapshot(&splashRollback.monsters) ||
        !EspNativeGameplayMonsterDrop_snapshot(&splashRollback.drops) ||
        !EspNativeGameplayPlayerState_snapshot(&splashRollback.player))
        return 0;
    splashRollback.combat = combatOwner;
    splashRollback.random = (*EspNativeProcessRng_live());
    splashSequenceActive = 1U;
    splashSequenceNumber = sequence;
    splashAudioSequence = sequence;
    splashAudioCount = 0U;
    printf("[MONSTERRADIUS] SEQUENCE-BEGIN seq=%u snapshot=monster+drop+player+rng+visual owner=bounded-BSS rollback=armed\n",
           (unsigned int)sequence);
    return 1;
}

int EspNativeGameplayMonsterCombat_cancelSplashSequence(
    DoomRPG_t* runtime) {
    int exact;
    if (splashSequenceActive == 0U) return 1;
    exact = restoreSplashSnapshot(runtime);
    printf("[MONSTERRADIUS] SEQUENCE-CANCEL exact=%s snapshots=restored\n",
           exact ? "yes" : "NO");
    return exact;
}

int EspNativeGameplayMonsterCombat_commitSplashSequence(
    uint32_t sequence) {
    if (splashSequenceActive == 0U ||
        splashSequenceNumber != sequence) return 0;
    splashSequenceActive = 0U;
    splashSequenceNumber = 0U;
    printf("[MONSTERRADIUS] SEQUENCE-COMMIT seq=%u order=blast-then-impact-per-wave rng=live-interleaved rollback=closed\n",
           (unsigned int)sequence);
    return 1;
}

/* Called only after PlayerDeath accepted the terminal transition. Preserve XP
 * in the committed player owner but do not open a level-up modal over death.
 * On failed admission the caller must cancel the still-active sequence. */
int EspNativeGameplayMonsterCombat_splashSequenceArmed(uint32_t sequence) {
    return sequence != 0U && splashSequenceActive != 0U &&
           splashSequenceNumber == sequence;
}

int EspNativeGameplayMonsterCombat_commitTerminalSplashSequence(
    uint32_t sequence) {
    if (sequence == 0U ||
        (splashLevelUpSequence != 0U &&
         splashLevelUpSequence != sequence)) return 0;
    if (!EspNativeGameplayMonsterCombat_commitSplashSequence(sequence)) return 0;
    splashLevelUpSequence = 0U;
    memset(&splashLevelUp, 0, sizeof(splashLevelUp));
    return 1;
}

/* A barrel may roll back later chain waves: never play a monster sound
 * while the splash is speculative. Release the exact already-selected legacy
 * SFX IDs only at the enclosing ActionEngine irreversible BARREL COMMIT.
 * No RNG, renderer, SD, PAK or heap work occurs in this audio publisher. */
int EspNativeGameplayMonsterCombat_publishSplashAudioAfterCommit(
    uint32_t sequence) {
    uint8_t i;
    uint8_t emitted = 0U;
    if (sequence == 0U || splashAudioSequence != sequence ||
        splashSequenceActive != 0U) return 0;
    for (i = 0U; i < splashAudioCount; ++i) {
        const uint16_t packed = splashAudioCues[i];
        const uint16_t resource = (uint16_t)(packed & 0x7fffU);
        const uint8_t priority = EspNativeMonsterDeathConsequence_priority(
            (uint8_t)((packed & MONSTER_SPLASH_AUDIO_DEATH_FLAG) != 0U),
            0U);
        if (EspNativeAudioIntent_publish(resource, 0U, priority)) ++emitted;
        printf("[MONSTERRADIUS] SFX-COMMIT seq=%u ordinal=%u/%u resource=%u priority=%u emitted=%u rollback=closed\n",
               (unsigned int)sequence, (unsigned int)(i + 1U),
               (unsigned int)splashAudioCount,
               (unsigned int)resource, (unsigned int)priority,
               (unsigned int)emitted);
    }
    splashAudioSequence = 0U;
    splashAudioCount = 0U;
    return 1;
}

int EspNativeGameplayMonsterCombat_applySplash(
    DoomRPG_t* runtime,
    const EspNativeGameplaySplashHit* hits,
    uint8_t count,
    uint32_t sequence) {
    uint8_t i;
    uint32_t rngCalls = 0U;
    uint32_t deaths = 0U;
    uint32_t totalXp = 0U;
    EspNativeGameplayPlayerXpResult popup;
    int needsPopup = 0;
    int rollbackOk;

    if (runtime == NULL || hits == NULL || count == 0U ||
        count > ESP_NATIVE_GAMEPLAY_SPLASH_MAX_HITS ||
        (splashSequenceActive != 0U &&
         splashSequenceNumber != sequence) ||
        (splashLevelUpSequence != 0U &&
         (splashSequenceActive == 0U ||
          splashLevelUpSequence != sequence)) ||
        !syncOwner() || combatOwner.pending.active) {
        return 0;
    }
    if (splashSequenceActive == 0U) {
        if (!EspNativeGameplayMonsterState_snapshot(
                &splashRollback.monsters) ||
            !EspNativeGameplayMonsterDrop_snapshot(
                &splashRollback.drops) ||
            !EspNativeGameplayPlayerState_snapshot(
                &splashRollback.player)) return 0;
        splashRollback.combat = combatOwner;
        splashRollback.random = (*EspNativeProcessRng_live());
        splashAudioSequence = sequence;
        splashAudioCount = 0U;
    }
    /* Capacity is checked BEFORE mutating health, drops, or RNG. If more
     * victims than the bounded 16x8 contract, roll back the entire chain. */
    if (splashAudioSequence != sequence ||
        (uint16_t)splashAudioCount + count >
            MONSTER_SPLASH_AUDIO_CAPACITY) {
        i = 0U;
        goto splash_failed;
    }
    memset(&popup, 0, sizeof(popup));

    for (i = 0U; i < count; ++i) {
        EspNativeGameplayMonsterRecord* monster =
            EspNativeGameplayMonsterState_findMutable(hits[i].spriteIndex);
        EspNativeGameplayMonsterRecord before;
        int32_t hpAfter;
        int32_t armorAfter;
        uint16_t sound = 0U;
        if (monster == NULL || monster->alive != 1U ||
            monster->subtype >= MONSTER_SUBTYPE_COUNT ||
            hits[i].healthComponent == 0U ||
            hits[i].armorComponent != hits[i].healthComponent) {
            goto splash_failed;
        }
        before = *monster;
        applyPain(monster, hits[i].healthComponent,
                  hits[i].armorComponent, &hpAfter, &armorAfter);
        if (hpAfter == 0) {
            EspNativeGameplayPlayerXpResult xp;
            EspNativeGameplayMonsterDropSpawnPlan drop;
            uint32_t dropRoll;
            uint32_t gained;
            /* Extra spawning/boss-death scripts need their own native owner. */
            if (specialDeath(before.subtype)) goto splash_failed;
            monster->alive = 0U;
            gained = EspNativeGameplayCombatMath_monsterExp(&before);
            memset(&xp, 0, sizeof(xp));
            if (!EspNativeGameplayPlayerState_applyXp(
                    runtime, gained, &xp)) goto splash_failed;
            rngCalls += xp.rngCalls;
            totalXp += gained;
            if (xp.levelUps != 0U) {
                if (needsPopup || splashLevelUpSequence != 0U)
                    goto splash_failed;
                popup = xp;
                needsPopup = 1;
            }
            memset(&drop, 0, sizeof(drop));
            if (!prepareDeathAfterXp(
                    runtime, &before, 0U, &rngCalls,
                    &drop, &sound, &dropRoll) ||
                (drop.outcome == ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SPAWN &&
                 !EspNativeGameplayMonsterDrop_commit(&drop))) {
                goto splash_failed;
            }
            if (combatOwner.deathSpriteIndex != MONSTER_NO_SPRITE &&
                combatOwner.deathSpriteIndex != before.spriteIndex) {
                setCorpseReady(combatOwner.deathSpriteIndex, 1);
            }
            setGibbed(before.spriteIndex, 0);
            setCorpseReady(before.spriteIndex, 0);
            combatOwner.deathSpriteIndex = before.spriteIndex;
            combatOwner.deathFinalGib = 0U;
            combatOwner.deathUntilMs =
                DoomRPG_GetUpTimeMS() + MONSTER_DEATH_MS;
            combatOwner.view.painSpriteIndex = MONSTER_NO_SPRITE;
            combatOwner.painUntilMs = 0U;
            ++combatOwner.view.kills;
            combatOwner.view.xpApplied += gained;
            ++deaths;
            printf("[MONSTERRADIUS] HIT seq=%u sprite=%u subtype=%u damage=%u+%u hp=%u->0 armor=%u->%d killed=yes xp=%u dropRoll=%08x sound=%u rng=committed\n",
                   (unsigned int)sequence,
                   (unsigned int)before.spriteIndex,
                   (unsigned int)before.subtype,
                   (unsigned int)hits[i].healthComponent,
                   (unsigned int)hits[i].armorComponent,
                   (unsigned int)(before.param1 & 0xffU),
                   (unsigned int)((before.param1 >> 16U) & 0xffU),
                   (int)armorAfter,
                   (unsigned int)gained,
                   (unsigned int)dropRoll,
                   (unsigned int)sound);
        }
        else {
            sound = consumePainSound(runtime, before.subtype, &rngCalls);
            combatOwner.view.painSpriteIndex = before.spriteIndex;
            combatOwner.painUntilMs = DoomRPG_GetUpTimeMS() + MONSTER_PAIN_MS;
            printf("[MONSTERRADIUS] HIT seq=%u sprite=%u subtype=%u damage=%u+%u hp=%u->%d armor=%u->%d killed=no sound=%u rng=committed\n",
                   (unsigned int)sequence,
                   (unsigned int)before.spriteIndex,
                   (unsigned int)before.subtype,
                   (unsigned int)hits[i].healthComponent,
                   (unsigned int)hits[i].armorComponent,
                   (unsigned int)(before.param1 & 0xffU),
                   (int)hpAfter,
                   (unsigned int)((before.param1 >> 16U) & 0xffU),
                   (int)armorAfter,
                   (unsigned int)sound);
        }
        /* Exactly the RNG-selected original cue: no second random draw. */
        if (sound != 0U) {
            splashAudioCues[splashAudioCount++] =
                (uint16_t)(sound | (hpAfter == 0
                    ? MONSTER_SPLASH_AUDIO_DEATH_FLAG : 0U));
        }
    }

    combatOwner.view.currentMonsterFNV1a = currentMonsterFNV();
    combatOwner.visualRedrawPending = 1U;
    printf("[MONSTERRADIUS] COMMIT seq=%u hits=%u deaths=%u xp=%u rngConsequence=%u monsterFNV=%08x playerFNV=%08x rollback=%s owner=esp-native\n",
           (unsigned int)sequence, (unsigned int)count,
           (unsigned int)deaths, (unsigned int)totalXp,
           (unsigned int)rngCalls,
           (unsigned int)combatOwner.view.currentMonsterFNV1a,
           (unsigned int)EspNativeGameplayPlayerState_fingerprint(),
           splashSequenceActive != 0U ? "sequence-armed" : "closed");
    if (needsPopup) {
        splashLevelUp = popup;
        splashLevelUpSequence = sequence;
        printf("[MONSTERRADIUS] LEVELUP-QUEUED seq=%u levels=%u presentation=after-attack-settle\n",
               (unsigned int)sequence, (unsigned int)popup.levelUps);
    }
    return 1;

splash_failed:
    /* A wave-N failure must restore the initial wave-1 snapshot, not just the
     * most recent hit. This also releases any deferred popup from prior waves. */
    rollbackOk = restoreSplashSnapshot(runtime);
    printf("[MONSTERRADIUS] ROLLBACK seq=%u hits=%u failedAt=%u exact=%s rng=yes monster=yes player=yes drops=yes\n",
           (unsigned int)sequence, (unsigned int)count,
           (unsigned int)i, rollbackOk ? "yes" : "NO");
    return 0;
}

int EspNativeGameplayMonsterCombat_finishSplashPresentation(
    uint32_t sequence) {
    int opened;
    if (splashLevelUpSequence == 0U) return 1;
    if (splashLevelUpSequence != sequence) return 0;
    opened = EspNativeGameplayLevelUp_begin(&splashLevelUp, sequence);
    printf("[MONSTERRADIUS] LEVELUP seq=%u levels=%u opened=%u afterAttackSettle=yes\n",
           (unsigned int)sequence, (unsigned int)splashLevelUp.levelUps,
           (unsigned int)opened);
    if (!opened) return 0;
    splashLevelUpSequence = 0U;
    memset(&splashLevelUp, 0, sizeof(splashLevelUp));
    return 1;
}

EspNativeGameplayActionStatus EspNativeGameplayMonsterCombat_executeSelect(
    const EspNativeGameplayInputState* intent,
    EspNativeGameplayActionResult* outResult) {
    EspNativeGameplayActionStatus status;
    EspNativeGameplayMonsterTraceStatus traceStatus;
    EspNativeGameplayMonsterTarget target;
    const EspNativeGameplayMonsterRecord* monster;
    const EspNativeGameplayWeaponSpec* weapon;
    const EspNativeGameplayHudState* hud;
    uint16_t genericSprite = MONSTER_NO_SPRITE;
    uint16_t genericTile = 0U;
    uint8_t genericSubtype = 0U;
    uint8_t weaponIndex;
    int genericEnemy;

    status = EspNativeGameplayActionEngine_executeSelect(intent, outResult);
    genericEnemy = intent != NULL &&
        EspNativeGameplayActionEngine_deferredMonsterTarget(
            intent->sequence, &genericSprite, &genericTile, &genericSubtype);
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
    if (traceStatus != ESP_NATIVE_GAMEPLAY_MONSTER_TRACE_FOUND) {
        if (genericEnemy) {
            const EspNativeGameplayMonsterRecord* genericMonster =
                EspNativeGameplayMonsterState_find(genericSprite);
            printf("[MONSTERCOMBAT] TRACE-MISMATCH seq=%u genericSprite=%u genericTile=%u genericSubtype=%u genericMonster=%u genericAlive=%u genericStateSubtype=%u nativeStatus=%u nativeSprite=%u nativeTile=%u nativeDistance=%u blockerKind=%u reason=trace-disagrees arm=no mutation=no rng=untouched\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)genericSprite, (unsigned int)genericTile,
                   (unsigned int)genericSubtype,
                   genericMonster != NULL ? 1U : 0U,
                   genericMonster != NULL
                       ? (unsigned int)genericMonster->alive : 0U,
                   genericMonster != NULL
                       ? (unsigned int)genericMonster->subtype : 255U,
                   (unsigned int)traceStatus,
                   (unsigned int)target.spriteIndex,
                   (unsigned int)target.tileIndex,
                   (unsigned int)target.distance,
                   (unsigned int)target.reserved);
        }
        return status;
    }

    monster = EspNativeGameplayMonsterState_find(target.spriteIndex);
    if (genericEnemy &&
        (target.spriteIndex != genericSprite ||
         target.tileIndex != genericTile ||
         target.subtype != genericSubtype)) {
        /* Read-only witness. Preserve the existing native combat decision:
         * corpses in the generic leaf can be intentionally bypassed here. */
        printf("[MONSTERCOMBAT] TRACE-DIFFERENCE seq=%u genericSprite=%u genericTile=%u genericSubtype=%u nativeSprite=%u nativeTile=%u nativeSubtype=%u action=preserve-native-trace mutation=no rng=untouched\n",
               (unsigned int)intent->sequence,
               (unsigned int)genericSprite, (unsigned int)genericTile,
               (unsigned int)genericSubtype,
               (unsigned int)target.spriteIndex, (unsigned int)target.tileIndex,
               (unsigned int)target.subtype);
    }
    if (monster == NULL || monster->alive == 0U ||
        monster->subtype != target.subtype) {
        if (genericEnemy) {
            printf("[MONSTERCOMBAT] TARGET-MISMATCH seq=%u sprite=%u subtype=%u found=%u alive=%u actualSubtype=%u reason=monster-state-disagrees arm=no mutation=no rng=untouched\n",
                   (unsigned int)intent->sequence,
                   (unsigned int)target.spriteIndex, (unsigned int)target.subtype,
                   monster != NULL ? 1U : 0U,
                   monster != NULL ? (unsigned int)monster->alive : 0U,
                   monster != NULL ? (unsigned int)monster->subtype : 255U);
        }
        return status;
    }

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

int EspMapSpriteTopology_getVisualState(uint32_t spriteIndex,
                                               uint8_t* outVisualState) {
    const EspNativeGameplayMonsterRecord* monster;
    if (!EspNativeGameplayProjectionOverlay_getVisualState(spriteIndex,
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

int EspMapSpriteTopology_getEntity(uint32_t spriteIndex,
                                          uint8_t* outType,
                                          uint8_t* outSubType,
                                          uint16_t* outLinkState,
                                          uint16_t* outLinkOrder) {
    const EspNativeGameplayMonsterRecord* monster;
    if (!EspNativeGameplayProjectionOverlay_getEntity(spriteIndex, outType, outSubType,
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

int EspNativeGameplayMonsterCombat_actionService(DoomRPG_t* runtime) {
    const EspMapSpriteTopologyView* topology = EspMapSpriteTopology_view();

    if (!EspNativeGameplayHubActionGate_service(runtime)) return 0;

    /* The private chain above has already serviced the generic ActionEngine.
     * With no enemies there is deliberately no MonsterState/MonsterCombat
     * owner to synchronize; zero-enemy hub maps therefore end successfully
     * here instead of turning absence of combat state into a fatal error. */
    if (topology != NULL && EspMapSpriteTopology_isReady() &&
        topology->enemyCount == 0U) {
        return 1;
    }

    if (!syncOwner()) return 0;
    if (combatOwner.pending.active != 0U) {
        return servicePending(runtime);
    }
    return serviceVisualRedraw(runtime);
}

void EspNativeGameplayMonsterCombat_actionReset(void) {
    /* PlayerState is session-permanent and intentionally survives map resets.
     * A later native new-game owner will reset it exactly once per new game. */
    EspNativeGameplayMonsterCombat_reset();
    EspNativeGameplayMonsterState_actionReset();
}
