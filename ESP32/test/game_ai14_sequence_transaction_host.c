/* Real sequencer + movement probe/publisher + RNG; controlled attack and world leaves. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "DoomRPG.h"
#include "esp_native_root_state.h"
#include "esp_native_process_rng_owner.h"
#include "esp_native_rng_replay_guard.h"
#include "esp_map_sprite_topology.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_monster_active_sequence.h"
#include "esp_native_gameplay_monster_activation.h"
#include "esp_native_gameplay_monster_movement_publish.h"
#include "esp_native_gameplay_monster_movement_probe.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_native_gameplay_monster_retaliation.h"
#include "esp_native_gameplay_player_death.h"
#include "esp_native_door_animator.h"
#include "esp_player_view_state.h"

enum { N=7, BASE=17 };
static DoomRPG_t root;
static EspNativeRootOwners_t roots;
static EspNativeGameplayMonsterTurnView turn;
static EspNativeGameplayMonsterRetaliationView retaliation;
static EspNativeGameplayMonsterRecord records[N];
static EspNativeGameplayMonsterView monsters;
static EspNativeGameplayMonsterPositionRecord positions[N], originals[N];
static EspMapSpriteTopologyView topology;
static EspPlayerViewState player;
static EspNativeGameplayMonsterMovementView movement;
static uint16_t tiles[N], linkOrders[N], selected;
static unsigned visits[N], renders[N], postMoves[N], plans[N];
static unsigned count, primeCount, invalidMember;
static unsigned int tick=12345;
unsigned int DoomRPG_GetTimeMS(void) { return tick; }
unsigned int DoomRPG_GetUpTimeMS(void) { return tick; }
void Legacy_DoomRPG_setRand(Random_t*);
unsigned int Random_Get(void);
unsigned int Legacy_Random_Get(void);

static unsigned indexOf(uint16_t sprite) {
    assert(sprite>=BASE && sprite<BASE+N);
    return sprite-BASE;
}
EspNativeRootOwners_t* EspNativeRootOwners_live(void) { return &roots; }
const EspPlayerViewState* EspPlayerView_view(void) { return &player; }
const EspNativeGameplayMonsterTurnView* EspNativeGameplayMonsterTurn_view(void) { return &turn; }
const EspNativeGameplayMonsterRetaliationView* EspNativeGameplayMonsterRetaliation_view(void) { return &retaliation; }
const EspNativeGameplayMonsterView* EspNativeGameplayMonsterState_view(void) { return &monsters; }
const EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_find(uint16_t sprite) {
    return &records[indexOf(sprite)];
}
const EspNativeGameplayMonsterPositionRecord* EspNativeGameplayMonsterPosition_find(uint16_t sprite) {
    return &positions[indexOf(sprite)];
}
uint32_t EspNativeGameplayMonsterPosition_fingerprint(void) {
    uint32_t h=2166136261U;
    for(unsigned i=0;i<N;i++) h=(h^positions[i].tileIndex)*16777619U;
    return h;
}
const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void) { return &topology; }
int EspMapSpriteTopology_getEntity(uint32_t sprite,uint8_t* type,uint8_t* sub,
                                  uint16_t* state,uint16_t* order) {
    unsigned i=indexOf((uint16_t)sprite);
    *type=ESP_MAP_ENTITY_TYPE_ENEMY; *sub=records[i].subtype;
    *state=tiles[i]|ESP_MAP_SPRITE_TOPOLOGY_LINKED|ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
    *order=linkOrders[i]; return 1;
}
int EspMapSpriteTopology_prepareRelink(uint16_t sprite,uint16_t dest,EspMapSpriteTopologyRelink* p) {
    unsigned i=indexOf(sprite);
    for(unsigned j=0;j<N;j++) if(j!=i && tiles[j]==dest) return 0;
    memset(p,0,sizeof(*p)); p->spriteIndex=sprite;
    p->sourceTile=tiles[i]; p->destTile=dest;
    p->linkOrderBefore=linkOrders[i]; p->linkOrderAfter=topology.nextLinkOrder;
    p->nextLinkOrderBefore=topology.nextLinkOrder;
    p->nextLinkOrderAfter=topology.nextLinkOrder+1U;
    return 1;
}
int EspMapSpriteTopology_commitPreparedRelink(const EspMapSpriteTopologyRelink* p) {
    unsigned i=indexOf(p->spriteIndex);
    assert(tiles[i]==p->sourceTile && linkOrders[i]==p->linkOrderBefore);
    tiles[i]=p->destTile; linkOrders[i]=p->linkOrderAfter;
    topology.nextLinkOrder=p->nextLinkOrderAfter; topology.stateFNV1a=topology.nextLinkOrder;
    return 1;
}
int EspMapSpriteTopology_rollbackPreparedRelink(const EspMapSpriteTopologyRelink* p) {
    unsigned i=indexOf(p->spriteIndex);
    assert(tiles[i]==p->destTile && linkOrders[i]==p->linkOrderAfter);
    tiles[i]=p->sourceTile; linkOrders[i]=p->linkOrderBefore;
    topology.nextLinkOrder=p->nextLinkOrderBefore; topology.stateFNV1a=topology.nextLinkOrder;
    return 1;
}
int EspNativeGameplayMonsterPosition_commitPrepared(
    const EspNativeGameplayMonsterPositionRecord* b,const EspNativeGameplayMonsterPositionRecord* a) {
    unsigned i=indexOf(b->spriteIndex);
    assert(memcmp(&positions[i],b,sizeof(*b))==0); positions[i]=*a; return 1;
}
int EspNativeGameplayMonsterPosition_rollbackPrepared(
    const EspNativeGameplayMonsterPositionRecord* a,const EspNativeGameplayMonsterPositionRecord* b) {
    unsigned i=indexOf(a->spriteIndex);
    assert(memcmp(&positions[i],a,sizeof(*a))==0); positions[i]=*b; return 1;
}
int EspNativeGameplayFrame_renderTurn(struct Render_s* render,uint8_t angle,
                                      EspNativeGameplayFrameStats* stats) {
    (void)render; (void)angle;
    unsigned i=indexOf(selected);
    ++renders[i]; memset(stats,0,sizeof(*stats));
    return !(i==1U && renders[i]==1U);
}
uint32_t EspNativeGameplayMonsterActivation_count(void) { return count; }
int EspNativeGameplayMonsterActivation_getOrdered(uint32_t i,uint16_t* sprite) {
    if(i>=count || sprite==NULL) return 0;
    *sprite=(uint16_t)(BASE+i); return 1;
}
void EspNativeGameplayMonsterActivation_selectOnly(uint16_t sprite) { selected=sprite; }
void EspNativeGameplayMonsterActivation_clearSelection(void) { selected=UINT16_MAX; }
void EspNativeGameplayMonsterActivation_overrideTurnCounters(uint32_t m,uint32_t n) {(void)m;(void)n;}
void EspNativeGameplayMonsterActivation_clearTurnCounterOverride(void) {}
int EspNativeGameplayPlayerDeath_isActive(void) { return 0; }
int EspNativeDoorAnimator_isHoldingDeferredClose(void) { return 0; }
uint8_t EspNativeDoorAnimator_deferredCloseCount(void) { return 0; }
uint8_t EspNativeDoorAnimator_releaseDeferredCloses(void) { return 0; }
const EspNativeGameplayMonsterMovementView* EspNativeGameplayMonsterMovement_view(void) { return &movement; }
void EspNativeGameplayMonsterMovement_service(struct DoomRPG_s* d) {
    assert(d==&root);
    if(selected==UINT16_MAX) { ++primeCount; return; }
    unsigned i=indexOf(selected); ++plans[i];
    EspNativeGameplayMonsterPositionRecord b=positions[i], a=b;
    a.worldX+=64; ++a.tileIndex;
    ++movement.plannedMoves; movement.lastSpriteIndex=selected;
    movement.lastSourceTile=b.tileIndex; movement.lastDestTile=a.tileIndex;
    EspNativeGameplayMonsterMovementPublish_capturePrepared(&b,&a);
}
int EspNativeGameplayMonsterTurn_fallbackNoMove(struct DoomRPG_s* d,uint16_t sprite,uint16_t tile) {
    (void)d; (void)sprite; (void)tile;
    assert(0 && "no fallback expected in this existing fixture"); return 0;
}
int EspNativeGameplayMonsterTurn_postMoveGoal(struct DoomRPG_s* d,uint16_t sprite,
                                              uint16_t source,uint16_t dest) {
    assert(d==&root && selected==sprite);
    unsigned i=indexOf(sprite);
    assert(dest==source+1U && positions[i].tileIndex==dest && tiles[i]==dest);
    ++postMoves[i]; return 1;
}
EspNativeGameplayMonsterMemberProbeStatus
EspNativeGameplayMonsterTurn_probeActiveMember(struct DoomRPG_s* d,uint16_t sprite) {
    assert(d==&root);
    unsigned i=indexOf(sprite);
    assert(i<4 && visits[i]==0); ++visits[i];
    if(i==3 && invalidMember) return ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_INVALID;
    if(i==2) {
        ++turn.attackProbes;
        return ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_ATTACK_PUBLISHED;
    }
    return ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_NO_IMMEDIATE_ATTACK;
}
static void exact(const Random_t* a,const Random_t* b) { assert(memcmp(a,b,sizeof(*a))==0); }
static uint8_t legacyByte(Random_t* r) {
    if(r->nextRand+1>=RANDTABLESIZE) Legacy_DoomRPG_setRand(r);
    return r->randTable[r->nextRand++];
}
static void fixture(unsigned arena,unsigned invalid) {
    memset(&turn,0,sizeof(turn)); memset(&retaliation,0,sizeof(retaliation));
    memset(&movement,0,sizeof(movement)); memset(&player,0,sizeof(player));
    memset(&topology,0,sizeof(topology)); memset(records,0,sizeof(records));
    memset(visits,0,sizeof(visits)); memset(renders,0,sizeof(renders));
    memset(postMoves,0,sizeof(postMoves)); memset(plans,0,sizeof(plans));
    count=6; selected=UINT16_MAX; primeCount=0; invalidMember=invalid;
    for(unsigned i=0;i<N;i++) {
        records[i].spriteIndex=(uint16_t)(BASE+i); records[i].subtype=1; records[i].alive=1;
        originals[i]=(EspNativeGameplayMonsterPositionRecord){(uint16_t)(BASE+i),(uint16_t)(99+32*i),224,(uint16_t)(224+64*i)};
        positions[i]=originals[i]; tiles[i]=originals[i].tileIndex; linkOrders[i]=(uint16_t)(i+1);
    }
    records[4].alive=0; records[5].subtype=10;
    monsters.records=records; monsters.count=N;
    topology.nextLinkOrder=N+1; topology.stateFNV1a=N+1;
    player.active=1; player.viewX=player.destX=1056; player.viewY=player.destY=1056;
    turn.sourceArenaFNV1a=arena; turn.active=1;
    EspNativeGameplayMonsterMovementPublish_reset();
    EspNativeRngReplayGuard_reset();
    EspNativeGameplayMonsterActiveSequence_service(&root);
    assert(primeCount==1 && !EspNativeGameplayMonsterActiveSequence_isBusy());
    turn.noAttackTurns=1; turn.lastReason=ESP_NATIVE_GAMEPLAY_MONSTER_TURN_MOVE;
}
int main(void) {
    Random_t* live=EspNativeProcessRng_live(); Random_t ref={0};
    const int offsets[]={0,125,126,127};
    unsigned cases=0;
    for(unsigned o=0;o<4;o++) for(unsigned invalid=0;invalid<2;invalid++) {
        fixture(0xa1d00000U+1U+cases,invalid);
        DoomRPG_setRand(live); Legacy_DoomRPG_setRand(&ref); exact(live,&ref);
        live->nextRand=ref.nextRand=offsets[o];
        EspNativeGameplayMonsterActiveSequence_service(&root);
        assert(visits[0]==1 && tiles[0]==originals[0].tileIndex+1U);
        (void)legacyByte(&ref); exact(live,&ref);
        count=7; /* newly activated member excluded from this turn */
        for(unsigned n=0;n<10 && turn.attackProbes==0;n++)
            EspNativeGameplayMonsterActiveSequence_service(&root);
        assert(turn.attackProbes==1 && visits[1]==1 && visits[2]==1 && visits[3]==0);
        assert(renders[1]==2 && postMoves[1]==0 && tiles[1]==originals[1].tileIndex);
        exact(live,&ref);
        Random_t paused=*live;
        for(unsigned n=0;n<3;n++) {
            tick+=2000U;
            EspNativeGameplayMonsterActiveSequence_service(&root);
            assert(EspNativeGameplayMonsterActiveSequence_isBusy() && visits[3]==0);
            exact(live,&paused);
        }
        /* Attack decision/resolution injected; RNG draw is the real owner.
           At offset 126 the failed movement reserved the boundary refill. */
        assert(DoomRPG_randNextByte(live)==legacyByte(&ref)); exact(live,&ref);
        retaliation.lastResolvedProbe=turn.attackProbes;
        for(unsigned n=0;n<12 && EspNativeGameplayMonsterActiveSequence_isBusy();n++)
            EspNativeGameplayMonsterActiveSequence_service(&root);
        assert(!EspNativeGameplayMonsterActiveSequence_isBusy() && visits[3]==1);
        if(!invalid) (void)legacyByte(&ref);
        exact(live,&ref);
        assert(Random_Get()==Legacy_Random_Get());
        for(unsigned i=0;i<N;i++) {
            unsigned moved=i==0 || (i==3 && !invalid);
            assert(tiles[i]==originals[i].tileIndex+moved);
            assert(positions[i].tileIndex==tiles[i] && positions[i].worldX==originals[i].worldX+64U*moved);
            assert(EspNativeGameplayMonsterMovementPublish_isProjected((uint16_t)(BASE+i))==(int)moved);
            assert(postMoves[i]==moved);
            if(i>=4) assert(visits[i]==0 && plans[i]==0);
            if(i==2 || (i==3&&invalid)) assert(plans[i]==0);
        }
        assert(topology.nextLinkOrder==N+2U+(invalid?0U:1U));
        assert(topology.stateFNV1a==topology.nextLinkOrder);
        Random_t final=*live;
        EspNativeGameplayMonsterActiveSequence_service(&root); /* no token replay */
        exact(live,&final); assert(visits[0]==1 && visits[3]==1);
        ++cases;
    }
    assert(cases==8);
    puts("PASS GAME-AI-14 A1d sequencer + real movement/RNG: 8 turns, rollback, attack pause, late activation, invalid suffix");
    return 0;
}
