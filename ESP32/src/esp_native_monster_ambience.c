#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_audio_intent.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_monster_ambience.h"
#include "esp_native_monster_sfx_table.h"

/* A render can activate several enemies at once, but the original game emits
 * one highest-subtype sight cry for the whole newly visible group. Every member
 * of that group must settle before its first attack pose, not just the winner.
 * Keep independent short-lived ordinal ranges: no heap, sprite-wide bitset,
 * world mutation or gameplay RNG. A fifth overlapping group coalesces with
 * the newest guard instead of silently releasing any still-live group. */
#define MONSTER_SIGHT_HOLD_MS 350U
#define MONSTER_SIGHT_GUARD_SLOTS 4U

typedef struct MonsterSightGuard_s {
    uint32_t firstOrdinal;
    uint32_t endOrdinal;
    uint32_t untilMs;
} MonsterSightGuard;

typedef struct MonsterAmbientOwner_s {
    uint32_t arena;
    uint32_t seenCount;
    uint32_t nextIdleMs;
    uint32_t rng;
    MonsterSightGuard sightGuards[MONSTER_SIGHT_GUARD_SLOTS];
    uint8_t sightGuardCount;
    uint8_t active;
} MonsterAmbientOwner;

static MonsterAmbientOwner ambient;

/* Sound selection is deliberately private to the audio presentation owner.
 * The legacy draws from its shared RNG, but doing so in a real-time native
 * audio tick would perturb combat rolls and checkpoint determinism. Only the
 * same variants and sparse interval distribution are retained, not RNG ABI. */
static uint8_t nextAudioRandom(void) {
    uint32_t x=ambient.rng;
    if (x==0U) x=0x6d2b79f5U;
    x^=x<<13U;
    x^=x>>17U;
    x^=x<<5U;
    ambient.rng=x;
    return (uint8_t)(x>>16U);
}

static uint16_t randomizedSound(uint8_t mType, uint8_t field) {
    uint8_t count=0U;
    while (count<3U &&
           EspNativeMonsterSfx_lookup(mType,(uint8_t)(field+count))!=0U)
        ++count;
    if (count==0U) return 0U;
    /* Legacy EntityMonster_getSoundRnd consumes one random even if count=1. */
    return EspNativeMonsterSfx_lookup(
        mType,(uint8_t)(field+(nextAudioRandom()%count)));
}

static void resetOwner(void) {
    memset(&ambient,0,sizeof(ambient));
}

static void retainSightGuard(uint32_t firstOrdinal, uint32_t endOrdinal,
                             uint32_t nowMs) {
    MonsterSightGuard* guard = NULL;
    uint32_t i;
    if (firstOrdinal >= endOrdinal) return;
    for (i=0U;i<ambient.sightGuardCount;++i) {
        if ((int32_t)(nowMs-ambient.sightGuards[i].untilMs)>=0) {
            guard=&ambient.sightGuards[i];
            break;
        }
    }
    if (guard==NULL && ambient.sightGuardCount<MONSTER_SIGHT_GUARD_SLOTS) {
        guard=&ambient.sightGuards[ambient.sightGuardCount++];
    }
    if (guard==NULL) {
        /* Every slot is still settling. Extend the most recent activation
         * range; older guards retain their original deadline. This may
         * conservatively hold the merged cohort longer, never release it
         * early or create another attack/gameplay turn. */
        uint32_t newest=0U;
        for (i=1U;i<ambient.sightGuardCount;++i) {
            if (ambient.sightGuards[i].endOrdinal >
                ambient.sightGuards[newest].endOrdinal)
                newest=i;
        }
        guard=&ambient.sightGuards[newest];
        if (firstOrdinal<guard->firstOrdinal)
            guard->firstOrdinal=firstOrdinal;
    } else {
        guard->firstOrdinal=firstOrdinal;
    }
    guard->endOrdinal=endOrdinal;
    guard->untilMs=nowMs+MONSTER_SIGHT_HOLD_MS;
}

int EspNativeMonsterAmbience_sightSettling(uint16_t spriteIndex,
                                          uint32_t nowMs) {
    if (!ambient.active) return 0;
    for (uint32_t i=0U;i<ambient.sightGuardCount;++i) {
        const MonsterSightGuard* guard=&ambient.sightGuards[i];
        if ((int32_t)(nowMs-guard->untilMs)>=0) continue;
        for (uint32_t ordinal=guard->firstOrdinal;
             ordinal<guard->endOrdinal;++ordinal) {
            uint16_t member=0xffffU;
            if (EspNativeGameplayMonsterActivation_getOrdered(
                    ordinal,&member) && member==spriteIndex)
                return 1;
        }
    }
    return 0;
}

void EspNativeMonsterAmbience_service(uint32_t nowMs,
                                     int gameplayActive,
                                     int idleAllowed) {
    const EspNativeGameplayMonsterView* view;
    uint32_t count;
    if (!gameplayActive) {
        if (ambient.active) resetOwner();
        return;
    }
    view=EspNativeGameplayMonsterState_view();
    if (view==NULL || view->sourceArenaFNV1a==0U ||
        view->records==NULL) return;

    count=EspNativeGameplayMonsterActivation_count();
    if (!ambient.active || ambient.arena!=view->sourceArenaFNV1a ||
        count<ambient.seenCount) {
        const uint32_t baseline=
            EspNativeGameplayMonsterActivation_soundRestoreBaseline();
        resetOwner();
        ambient.active=1U;
        ambient.arena=view->sourceArenaFNV1a;
        ambient.seenCount=baseline<=count?baseline:count;
        ambient.nextIdleMs=nowMs+8000U;
        ambient.rng=view->sourceArenaFNV1a ^ nowMs ^ 0x9e3779b9U;
        if (ambient.rng==0U) ambient.rng=0x6d2b79f5U;
        printf("[MONSTERAMBIENT] READY arena=%08x restoredPrefix=%u active=%u "
               "firstIdleMs=8000 rng=private-audio gameplayRng=untouched\n",
               (unsigned)ambient.arena,(unsigned)ambient.seenCount,
               (unsigned)count);
    }

    /* Game_activate collects the highest subtype detected during a view
     * update, then plays one randomized sight sample. No sight replay on
     * checkpoint adoption or on later render of the same live sprite. */
    if (count>ambient.seenCount) {
        const uint32_t previous=ambient.seenCount;
        const EspNativeGameplayMonsterRecord* winner=NULL;
        uint16_t winningSprite=0xffffU;
        for (uint32_t i=previous;i<count;++i) {
            uint16_t sprite=0xffffU;
            if (!EspNativeGameplayMonsterActivation_getOrdered(i,&sprite))
                continue;
            const EspNativeGameplayMonsterRecord* m=
                EspNativeGameplayMonsterState_find(sprite);
            if (m==NULL || !m->alive || m->mType>=14U ||
                EspNativeMonsterSfx_lookup(m->mType,1U)==0U) continue;
            if (winner==NULL || m->subtype>winner->subtype) {
                winner=m;
                winningSprite=sprite;
            }
        }
        ambient.seenCount=count;
        if (winner!=NULL && EspNativeAudioIntent_isSoundEnabled()) {
            const uint16_t sound=randomizedSound(winner->mType,1U);
            if (sound!=0U) {
                (void)EspNativeAudioIntent_publish(sound,0U,3U);
                retainSightGuard(previous,count,nowMs);
                printf("[MONSTERAMBIENT] SIGHT sprite=%u subtype=%u mType=%u "
                       "id=%u newlyActive=%u activation=once "
                       "guardOrdinals=%u..%u guardMs=%u "
                       "gameplayRng=untouched\n",
                       (unsigned)winningSprite,(unsigned)winner->subtype,
                       (unsigned)winner->mType,(unsigned)sound,
                       (unsigned)(count-previous),
                       (unsigned)previous,(unsigned)(count-1U),
                       (unsigned)MONSTER_SIGHT_HOLD_MS);
            }
        }
    }

    /* No autonomous world redraw needed: one audio tick after 8s initial
     * wait and then the legacy's 6s + max(0, 64*rand-1000*activeCount).
     * Only living, activated monsters with a non-zero idle sound qualify.
     * Pause during dialogue/HUB, monster attack or SOUND OFF. */
    if (!idleAllowed || !EspNativeAudioIntent_isSoundEnabled() ||
        (int32_t)(nowMs-ambient.nextIdleMs)<0) return;
    uint32_t eligible=0U;
    for (uint32_t i=0U;i<count;++i) {
        uint16_t sprite=0xffffU;
        if (!EspNativeGameplayMonsterActivation_getOrdered(i,&sprite))
            continue;
        const EspNativeGameplayMonsterRecord* m=
            EspNativeGameplayMonsterState_find(sprite);
        if (m!=NULL && m->alive && m->mType<14U &&
            EspNativeMonsterSfx_lookup(m->mType,0U)!=0U) ++eligible;
    }
    if (eligible==0U) {
        ambient.nextIdleMs=nowMs+1000U;
        return;
    }
    const uint32_t chosen=(uint32_t)nextAudioRandom()%eligible;
    uint32_t ordinal=0U;
    for (uint32_t i=0U;i<count;++i) {
        uint16_t sprite=0xffffU;
        if (!EspNativeGameplayMonsterActivation_getOrdered(i,&sprite))
            continue;
        const EspNativeGameplayMonsterRecord* m=
            EspNativeGameplayMonsterState_find(sprite);
        if (m==NULL || !m->alive || m->mType>=14U ||
            EspNativeMonsterSfx_lookup(m->mType,0U)==0U) continue;
        if (ordinal++!=chosen) continue;
        const uint16_t sound=EspNativeMonsterSfx_lookup(m->mType,0U);
        (void)EspNativeAudioIntent_publish(sound,0U,1U);
        printf("[MONSTERAMBIENT] IDLE sprite=%u subtype=%u mType=%u id=%u "
               "active=%u gameplayRng=untouched\n",
               (unsigned)sprite,(unsigned)m->subtype,
               (unsigned)m->mType,(unsigned)sound,
               (unsigned)eligible);
        break;
    }
    /* Legacy idleTime: 6000 + clamp(rand*64 - 1000*activeCount, 0..).
     * Use living eligible count to avoid very frequent calls on dead rows. */
    const uint32_t jitter=(uint32_t)nextAudioRandom()<<6U;
    const uint32_t bias=eligible*1000U;
    ambient.nextIdleMs=nowMs+6000U+(jitter>bias?jitter-bias:0U);
}
