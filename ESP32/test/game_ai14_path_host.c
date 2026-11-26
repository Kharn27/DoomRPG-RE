/* Real native planner TUs, separate binaries; only map access leaves mocked. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if CONTINUATION
#include "../src/esp_native_gameplay_monster_three_goal_turn.c"
typedef ThreeGoalProbeRng TestRng;
typedef ThreeGoalMovePlan TestPlan;
#else
#include "../src/esp_native_gameplay_monster_movement.c"
typedef ProbeRng TestRng;
typedef MovePlan TestPlan;
#endif

static EspMapRuntimeView rt;
static EspMapSpriteTopologyView topo;
static EspPlayerViewState player;
static uint8_t walls[1024], types[8], linked[8], opened[4];
static uint16_t tiles[8];
static EspMapSprite sprites[8];
static EspMapLine lines[4];
static int failTopology;
const EspMapRuntimeView* EspMapRuntime_view(void) { return &rt; }
const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void) { return failTopology ? NULL : &topo; }
const EspPlayerViewState* EspPlayerView_view(void) { return &player; }
int EspMapState_getTileFlags(uint32_t tile, uint8_t* out) {
    if (tile >= 1024) return 0;
    *out = walls[tile] ? ESP_MAP_TILE_WALL : 0; return 1;
}
int EspMapRuntime_getLine(uint32_t i, EspMapLine* out) {
    if (i >= rt.lineCount) return 0;
    *out = lines[i]; return 1;
}
int EspMapLineState_getOpen(uint32_t i, uint8_t* out) {
    if (i >= rt.lineCount) return 0;
    *out = opened[i]; return 1;
}
int EspEntityDefTypeCatalog_isReady(void) { return 1; }
int EspEntityDefTypeCatalog_getTypeAndSubtype(uint16_t id, uint8_t* type, uint8_t* sub) {
    if (id < 305 || id >= 321) return 0;
    *type = (uint8_t)(id - 305); *sub = 0; return 1;
}
int EspMapRuntime_getMapSprite(uint32_t i, EspMapSprite* out) {
    if (i >= topo.spriteCount) return 0;
    *out = sprites[i]; return 1;
}
int EspMapSpriteTopology_getEntity(uint32_t i, uint8_t* type, uint8_t* sub,
                                  uint16_t* state, uint16_t* order) {
    if (i >= topo.spriteCount) return 0;
    *type = types[i]; *sub = 0;
    *state = tiles[i] | (linked[i] ? ESP_MAP_SPRITE_TOPOLOGY_LINKED : 0);
    *order = (uint16_t)(i+1); return 1;
}
static void resetFixture(void) {
    memset(walls,0,sizeof(walls)); memset(types,0,sizeof(types));
    memset(linked,0,sizeof(linked)); memset(tiles,0,sizeof(tiles));
    memset(sprites,0,sizeof(sprites)); memset(lines,0,sizeof(lines));
    memset(opened,0,sizeof(opened));
    rt.lineCount=0; topo.spriteCount=8; failTopology=0;
    memset(&player,0,sizeof(player)); player.active=1;
    player.viewX=player.destX=2016; player.viewY=player.destY=2016;
    types[0]=1; linked[0]=1; tiles[0]=330;
}
static int center(int t) { return 64*t+32; }
static int trace(int sx,int sy,int dx,int dy,uint16_t mask,int strict) {
    return cardinalTraceClear(center(sx),center(sy),center(dx),center(dy),0,mask,strict);
}
/* Legacy bodies are extracted by the Python runner, not reimplemented.
 * This adapter maps both center and tile-origin queries onto the same
 * discrete fixture; special geometry is deliberately excluded here. */
typedef struct RefMonster { int x,y; } RefMonster;
typedef struct RefDef { int eSubType; } RefDef;
typedef struct RefRoot { Random_t random; void* game; } RefRoot;
typedef struct Entity_t {
    RefDef* def; RefMonster* monster; RefRoot* doomRpg;
    int visitOrderCount, visitOrder[4], visitDist[4];
} Entity_t;
static int legacyCalls, legacyLinks;
static boolean RefSight(Entity_t* e,int sx,int sy,int dx,int dy,int mask) {
    (void)e;
    int x=sx/64, y=sy/64, a=dx/64, b=dy/64;
    if(sx<0||sy<0||dx<0||dy<0||x>=32||y>=32||a>=32||b>=32) return false;
    if(walls[y*32+x]||walls[b*32+a]) return false;
    for(unsigned i=1;i<topo.spriteCount;i++) {
        if(linked[i] && (mask & (1U<<types[i])) &&
           (tiles[i]==y*32+x || tiles[i]==b*32+a)) return false;
    }
    return true;
}
static byte RefByte(Random_t* rng) { ++legacyCalls; return rng->randTable[rng->nextRand++]; }
static void RefUnlink(void* g,Entity_t* e) { (void)g;(void)e; ++legacyLinks; }
static void RefLink(void* g,Entity_t* e,int x,int y) { (void)g;assert(e->monster->x/64==x&&e->monster->y/64==y);++legacyLinks; }
#include "legacy_goal.inc"

int main(void) {
    resetFixture();
    uint16_t mask=0xff87U;
    assert(trace(10,10,11,10,mask,0)==1);
    assert(trace(10,10,11,11,mask,0)==0);
    walls[331]=1; assert(trace(10,10,11,10,mask,0)==0); walls[331]=0;
    walls[330]=1; assert(trace(10,10,11,10,mask,0)==0); walls[330]=0;
    types[1]=1; linked[1]=1; tiles[1]=331;
    assert(trace(10,10,11,10,mask,0)==0); linked[1]=0;
    assert(trace(10,10,11,10,mask,0)==1);
    linked[1]=1; tiles[1]=330;
    assert(trace(10,10,11,10,mask,0)==0);
    types[1]=10; tiles[1]=331;
    assert(trace(10,10,11,10,0xf387U,0)==1);
    assert(trace(10,10,11,10,mask,0)==0);
    linked[1]=0;
    rt.lineCount=1;
    lines[0]=(EspMapLine){704,640,704,704,7,0};
    assert(trace(10,10,11,10,mask,0)==0);
    opened[0]=1; assert(trace(10,10,11,10,mask,0)==1); rt.lineCount=0;
    types[1]=14; linked[1]=1; tiles[1]=331;
    sprites[1]=(EspMapSprite){704,672,0x00220000U};
    assert(trace(10,10,11,10,mask,0)==0);
    /* A4-P2: original tile-origin segment ends at plane x=704;
       equality at the destination is clear (half-open crossing). */
    assert(trace(10,10,11,10,mask,1)==1);
    linked[1]=0;
    failTopology=1; assert(trace(10,10,11,10,mask,0)==-1); failTopology=0;
    player.destX=player.viewX=center(11); player.destY=player.viewY=center(10);
    int playerTrace=trace(10,10,11,10,mask,0);
    assert(trace(10,10,11,10,0x5687U,0)==1);
    printf("GAMEAI14 player sentinel continuation=%d result=%d (0=blocked)\n",CONTINUATION,playerTrace);
    assert(playerTrace==0);
    unsigned cases=0;
    for(unsigned scenario=0;scenario<8;scenario++) {
        resetFixture();
        if(scenario&1) walls[331]=1;
        if(scenario&2) { types[1]=1;linked[1]=1;tiles[1]=362; }
        if(scenario&4) { types[2]=11;linked[2]=1;tiles[2]=329; }
        for(unsigned sub=0;sub<14;sub++) {
#if CONTINUATION
            /* The real postMoveGoal entry admits only subtypes 4 and 13. */
            if(sub!=4U && sub!=13U) continue;
#endif
            for(int dx=8;dx<=12;dx++) for(int dy=8;dy<=12;dy++) {
                for(unsigned tie=0;tie<4;tie++) {
                    RefMonster m={0}; RefDef d={(int)sub}; RefRoot root={0};
                    Entity_t e={.def=&d,.monster=&m,.doomRpg=&root};
                    TestRng rng={0}; TestPlan plan={0};
                    root.random.randTable[0]=(byte)tie; rng.state=root.random;
                    legacyCalls=legacyLinks=0;
                    int want=RefGoal(&e,center(10),center(10),center(dx),center(dy));
                    int got=aiGoalPlan(0,(uint8_t)sub,center(10),center(10),center(dx),center(dy),&rng,&plan);
                    assert(got==want);
                    assert(rng.calls==(unsigned)legacyCalls && rng.state.nextRand==root.random.nextRand);
                    assert(plan.visitCount==e.visitOrderCount);
                    if(want) {
                        assert(center(10)+plan.deltaX==m.x && center(10)+plan.deltaY==m.y);
                        assert(plan.destTile==(m.y/64)*32+m.x/64);
                        assert(legacyLinks==2 && plan.tieRandUsed==1);
                    } else assert(legacyLinks==0);
                    ++cases;
                }
            }
        }
    }
    resetFixture();
    walls[331]=walls[329]=walls[362]=walls[298]=1;
    TestRng rng={0}; TestPlan plan={0};
    assert(aiGoalPlan(0,4,center(10),center(10),center(12),center(12),&rng,&plan)==0);
    assert(rng.calls==0 && plan.visitCount==0);
    resetFixture(); rng.state.nextRand=127;
    assert(aiGoalPlan(0,4,center(10),center(10),center(12),center(12),&rng,&plan)==-3);
    assert(rng.calls==0 && rng.boundary==1 && rng.state.nextRand==127);
    assert(cases==(CONTINUATION?1600U:11200U));
    /* Two enemies competing for the same free tile. Replan after publishing
       the first position into the fixture, in both activation orders.
       This tests actual planner occupancy reads, not the live publisher. */
    unsigned conflictCases=0;
    for(unsigned order=0;order<2;order++) {
        for(unsigned tie=0;tie<4;tie++) {
            resetFixture();
            types[1]=1; linked[1]=1; tiles[1]=332;
            /* Force the second monster to have only the contested neighbor. */
            unsigned loser=order?0U:1U, winner=1U-loser;
            unsigned lx=loser?12U:10U;
            walls[10U*32U+(loser?13U:9U)]=1;
            walls[9U*32U+lx]=1;
            walls[11U*32U+lx]=1;
            TestRng a={0}, z={0};
            TestPlan first={0}, second={0}, stale={0};
            a.state.randTable[0]=(byte)tie;
            z.state.randTable[0]=(byte)tie;
            TestRng speculative=z;
            assert(aiGoalPlan((uint16_t)loser,4,center((int)lx),center(10),
                              center(11),center(10),&speculative,&stale)==1);
            assert(stale.destTile==331 && speculative.calls==1);
            assert(aiGoalPlan((uint16_t)winner,4,center(winner?12:10),center(10),
                              center(11),center(10),&a,&first)==1);
            assert(first.destTile==331 && a.calls==1);
            tiles[winner]=first.destTile;
            assert(aiGoalPlan((uint16_t)loser,4,center((int)lx),center(10),
                              center(11),center(10),&z,&second)==0);
            assert(z.calls==0 && second.visitCount==0 && tiles[loser]!=tiles[winner]);
            /* Occupancy removal must be visible to the same planner. */
            linked[winner]=0;
            assert(aiGoalPlan((uint16_t)loser,4,center((int)lx),center(10),
                              center(11),center(10),&z,&second)==1);
            assert(second.destTile==331 && z.calls==1);
            ++conflictCases;
        }
    }
    assert(conflictCases==8);
#if CONTINUATION
    /* Real higher-level shortcut suppresses continuation when adjacent. */
    resetFixture();
    EspNativeGameplayMonsterPositionRecord pos={0,330,672,672};
    uint32_t d2=0;
    player.destX=736; player.destY=672;
    assert(adjacentLegacyShortcut(&player,&pos,&d2) && d2==4096U);
    player.destY=736;
    assert(!adjacentLegacyShortcut(&player,&pos,&d2) && d2==8192U);
    player.destX=800; player.destY=672;
    assert(!adjacentLegacyShortcut(&player,&pos,&d2) && d2==16384U);
#endif
    printf("PASS GAME-AI-14 A1c continuation=%d contested-replan=%u\n",CONTINUATION,conflictCases);
    printf("PASS GAME-AI-14 A1c continuation=%d differential-goals=%u + collision fixtures\n",CONTINUATION,cases);
    return 0;
}
