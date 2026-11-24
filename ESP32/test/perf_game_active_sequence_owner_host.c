/* PERF-GAME active OWNER deterministic host oracle.
 * Compile the actual esp_native_gameplay_monster_active_sequence.c, not a
 * hand-copied scheduling model. Mock only external world/AI side effects.
 * World rules/geometry still require real CYD + separate parity coverage. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_root_state.h"
#include "esp_native_gameplay_monster_active_sequence.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_monster_movement.h"
#include "esp_native_gameplay_monster_movement_probe.h"
#include "esp_native_gameplay_monster_state.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_retaliation.h"
#include "esp_native_gameplay_player_death.h"
#include "esp_native_door_animator.h"
#include "esp_native_gameplay_frame.h"
#include "esp_player_view_state.h"

enum { MAX_MEMBERS=32, BASE_SPRITE=40 };
static EspNativeGameplayMonsterTurnView turn;
static EspNativeGameplayMonsterRetaliationView retaliation;
static EspNativeGameplayMonsterRecord monsters[MAX_MEMBERS];
static uint16_t order[MAX_MEMBERS];
static uint16_t tiles[MAX_MEMBERS];
static uint8_t visited[MAX_MEMBERS];
static uint8_t events[MAX_MEMBERS];
static unsigned count, prefix, eventCount, movementCount, primeCount;
static unsigned selected, released, getOrderedFailAt;
static uint32_t actualRng, actualHash, actualDraws, deniedProbes;
static EspNativeRootOwners_t root;
static EspPlayerViewState player;

static uint32_t nextRand(uint32_t state) {
    return state * 1664525U + 1013904223U;
}
static void appendEvent(uint8_t code,unsigned i) {
    assert(eventCount<MAX_MEMBERS);
    events[eventCount++]=code;
    actualHash=(actualHash ^ ((uint32_t)code*257U+i))*16777619U;
}
const EspNativeGameplayMonsterTurnView* EspNativeGameplayMonsterTurn_view(void) {
    return &turn;
}
const EspNativeGameplayMonsterRetaliationView* EspNativeGameplayMonsterRetaliation_view(void) {
    return &retaliation;
}
uint32_t EspNativeGameplayMonsterActivation_count(void) {return count;}
int EspNativeGameplayMonsterActivation_getOrdered(uint32_t i,uint16_t* sprite) {
    if(i>=count || i==getOrderedFailAt || sprite==NULL) return 0;
    *sprite=order[i];return 1;
}
const EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_find(uint16_t id) {
    if(id<BASE_SPRITE || id>=BASE_SPRITE+MAX_MEMBERS) return NULL;
    return &monsters[id-BASE_SPRITE];
}
void EspNativeGameplayMonsterActivation_selectOnly(uint16_t id) {selected=id;}
void EspNativeGameplayMonsterActivation_clearSelection(void) {selected=UINT16_MAX;}
void EspNativeGameplayMonsterActivation_overrideTurnCounters(uint32_t m,uint32_t n) {(void)m;(void)n;}
void EspNativeGameplayMonsterActivation_clearTurnCounterOverride(void) {}
void EspNativeGameplayMonsterMovement_service(struct DoomRPG_s* r) {(void)r; ++primeCount;}
int EspNativeGameplayPlayerDeath_isActive(void) {return 0;}
int EspNativeDoorAnimator_isHoldingDeferredClose(void) {return 0;}
uint8_t EspNativeDoorAnimator_deferredCloseCount(void) {return 0;}
uint8_t EspNativeDoorAnimator_releaseDeferredCloses(void) {++released;return 0;}
EspNativeRootOwners_t* EspNativeRootOwners_live(void) {return &root;}
const EspPlayerViewState* EspPlayerView_view(void) {return &player;}
int EspNativeGameplayFrame_renderTurn(struct Render_s* r,uint8_t angle,
                                     EspNativeGameplayFrameStats* out) {
    (void)r;(void)angle;(void)out;return 1;
}
EspNativeGameplayMonsterMemberProbeStatus
EspNativeGameplayMonsterTurn_probeActiveMember(struct DoomRPG_s* r,uint16_t id) {
    unsigned i=(unsigned)(id-BASE_SPRITE);
    (void)r;
    assert(i<prefix && monsters[i].alive && monsters[i].subtype!=10U);
    assert(visited[i]==0U);
    visited[i]=1U;
    actualRng=nextRand(actualRng);++actualDraws;
    if(i%4U==0U) {
        ++turn.attackProbes;
        appendEvent('A',i);
        return ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_ATTACK_PUBLISHED;
    }
    return i%4U==1U ? ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_RANGED_MOVE :
                         ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_NO_IMMEDIATE_ATTACK;
}
int EspNativeGameplayMonsterMovementProbe_serviceMember(
    struct DoomRPG_s* r,const char* trigger,uint8_t* committed) {
    unsigned i;
    (void)r;(void)trigger;
    assert(selected>=BASE_SPRITE && selected<BASE_SPRITE+prefix);
    i=(unsigned)(selected-BASE_SPRITE);
    ++movementCount;
    actualRng=nextRand(actualRng);++actualDraws;
    /* A rejected move publishes no topology; the probe rollback is exact. */
    if(i%6U==3U) {
        *committed=0U;
    } else {
        ++tiles[i];
        *committed=1U;
    }
    appendEvent((i%4U==1U)?'R':'M',i);
    return 1;
}
static void start(unsigned n,uint32_t arena) {
    unsigned i;
    memset(&turn,0,sizeof(turn));
    memset(&retaliation,0,sizeof(retaliation));
    memset(monsters,0,sizeof(monsters));
    memset(visited,0,sizeof(visited));
    memset(tiles,0,sizeof(tiles));
    memset(events,0,sizeof(events));
    eventCount=movementCount=primeCount=released=actualDraws=deniedProbes=0U;
    selected=UINT16_MAX;
    getOrderedFailAt=UINT16_MAX;
    actualRng=0x12345678U;actualHash=2166136261U;
    prefix=count=n;
    for(i=0;i<n;i++) {
        order[i]=(uint16_t)(BASE_SPRITE+i);
        monsters[i].spriteIndex=order[i];
        monsters[i].alive=(uint8_t)(i%9U!=5U);
        monsters[i].subtype=(uint8_t)(i%7U==2U?10U:4U);
        tiles[i]=(uint16_t)(100U+i);
    }
    turn.sourceArenaFNV1a=arena;
    turn.active=1U;
    /* A fresh arena causes a reset/prime without creating an enemy turn. */
    EspNativeGameplayMonsterActiveSequence_service(NULL);
    assert(!EspNativeGameplayMonsterActiveSequence_isBusy());
    turn.noAttackTurns=1U;
    turn.lastReason=ESP_NATIVE_GAMEPLAY_MONSTER_TURN_MOVE;
}
static void checkOracle(unsigned originalCount,int abortedAt) {
    uint32_t refRng=0x12345678U, refHash=2166136261U;
    unsigned i, expected=0U, expectedMoves=0U, expectedDraws=0U;
    for(i=0;i<originalCount;i++) {
        uint8_t code;
        if(abortedAt>=0 && (int)i>=abortedAt) break;
        if(!monsters[i].alive || monsters[i].subtype==10U) {
            assert(!visited[i]);
            continue;
        }
        assert(visited[i]==1U);
        refRng=nextRand(refRng);++expectedDraws;
        if(i%4U==0U) code='A';
        else {
            code=i%4U==1U?'R':'M';
            refRng=nextRand(refRng);++expectedDraws;
            ++expectedMoves;
        }
        assert(expected<eventCount && events[expected]==code);
        refHash=(refHash ^ ((uint32_t)code*257U+i))*16777619U;
        if(code!='A' && i%6U!=3U) assert(tiles[i]==101U+i);
        else assert(tiles[i]==100U+i);
        ++expected;
    }
    assert(eventCount==expected && movementCount==expectedMoves);
    assert(actualRng==refRng && actualDraws==expectedDraws);
    assert(actualHash==refHash);
}
static void run(unsigned n,uint32_t arena) {
    unsigned serviceCalls=0U;
    start(n,arena);
    while(++serviceCalls<n*4U+8U) {
        unsigned oldCount=eventCount;
        EspNativeGameplayMonsterActiveSequence_service(NULL);
        if(serviceCalls==1U) {
            /* Activate another sprite mid-turn; it must NOT join the prefix. */
            count=n+1U;
            order[n]=(uint16_t)(BASE_SPRITE+n);
            monsters[n].spriteIndex=order[n];
            monsters[n].alive=1U;
            monsters[n].subtype=4U;
        }
        if(EspNativeGameplayMonsterActiveSequence_isBusy() &&
           retaliation.lastResolvedProbe<turn.attackProbes) {
            /* Confirm the in-flight attack is a strict scheduling barrier. */
            uint32_t rngBefore=actualRng;
            uint32_t hashBefore=actualHash;
            EspNativeGameplayMonsterActiveSequence_service(NULL);
            assert(actualRng==rngBefore && actualHash==hashBefore);
            assert(eventCount==oldCount+1U || eventCount==oldCount);
            ++deniedProbes;
            retaliation.lastResolvedProbe=turn.attackProbes;
        }
        if(!EspNativeGameplayMonsterActiveSequence_isBusy()) break;
    }
    assert(serviceCalls<n*4U+8U);
    assert(primeCount==1U);
    assert(eventCount<=n && deniedProbes>0U);
    assert(visited[n]==0U); /* new activation deferred to following turn */
    checkOracle(n,-1);
}
int main(void) {
    run(7U,0xa1b2c301U);
    run(24U,0xa1b2c302U);
    /* Fail-closed activation-order read: committed prefix persists, no replay. */
    start(7U,0xa1b2c303U);
    getOrderedFailAt=2U;
    while(EspNativeGameplayMonsterActiveSequence_isBusy()==0)
        EspNativeGameplayMonsterActiveSequence_service(NULL);
    retaliation.lastResolvedProbe=turn.attackProbes;
    EspNativeGameplayMonsterActiveSequence_service(NULL);
    assert(!EspNativeGameplayMonsterActiveSequence_isBusy());
    checkOracle(7U,2);
    puts("PERF-GAME real active-sequence owner oracle PASS 7/24 ordered, RNG, topology, death skips, pause, rollback, activation snapshot, fail-closed");
    return 0;
}
