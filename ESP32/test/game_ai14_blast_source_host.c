/* Actual extracted production radius leaf, with controllable world leaves. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_explosion.h"
#include "esp_native_gameplay_barrel_radius.h"
#include "esp_native_gameplay_monster_combat.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_player_view_state.h"
#include "esp_map_runtime.h"
#include "esp_native_gameplay_action_engine.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_dynamic_line_render.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_native_graphics_catalog.h"
#include "esp_native_sprite_renderer.h"
__DEFINES__
__TARGET__
typedef struct DoomRPG_s DoomRPG_t;
typedef struct { int nextRand; } Random_t;
__JOURNAL__
static Random_t rng;
static int playerHp;
static unsigned hitsCount;
static EspNativeGameplaySplashHit hitsSeen[128];
static char order[32]; static unsigned orderCount;
static int failMonster, reviveMonster, enemyMode;
static void mark(char ch){assert(orderCount<sizeof order-1);order[orderCount++]=ch;order[orderCount]=0;}
static Random_t* EspNativeProcessRng_live(void){return &rng;}
static uint8_t DoomRPG_randNextByte(Random_t* r){mark('D');return (uint8_t)r->nextRand++;}
uint8_t EspNativeGameplayPlayerState_health(void){return (uint8_t)playerHp;}
int EspNativeGameplayMonsterCombat_applySplash(struct DoomRPG_s* runtime,const EspNativeGameplaySplashHit* hits,uint8_t count,uint32_t sequence){assert(runtime && count==1 && sequence==7 && hitsCount<128);mark('M');if(failMonster)return 0;hitsSeen[hitsCount++]=*hits;rng.nextRand+=4;if(reviveMonster)playerHp=30;return 1;}
static EspPlayerViewState player;
static EspMapSprite immutable;
static unsigned mapReads, lookupCount, damageCalls, terminalCalls, removals;
static int mapReady, failLookup, lethal;
static uint16_t visited[64];
static unsigned kind[1024];
#define ESP_MAP_SPRITE_TOPOLOGY_NO_SPRITE 65535U
#define ESP_MAP_SPRITE_TOPOLOGY_LINKED 0x4000U
static int preflightFixture;
static uint8_t rootSubtype;
static int removed(uint32_t sprite);
static int actionGetEntity(uint32_t sprite,uint8_t* type,uint8_t* subtype,
                           uint16_t* link,uint16_t* order) {
    if (!preflightFixture || (sprite!=50 && sprite!=90)) return 0;
    *type=ACTION_ENTITY_DESTRUCTIBLE;
    *subtype=sprite==50?rootSubtype:ACTION_DESTRUCTIBLE_BARREL_SUBTYPE;
    *link=(enemyMode && removed(sprite))?0:ESP_MAP_SPRITE_TOPOLOGY_LINKED; *order=0; return 1;
}
static uint16_t entityId[1024];
static uint8_t removedBits[1024];
static uint16_t playerDamage;
int EspMapRuntime_getMapSpriteBase(uint32_t index, EspMapSprite *out) {
    assert(index==50U || (preflightFixture && index==90U)); ++mapReads;
    if(!mapReady) return 0;
    *out=immutable;
    if(preflightFixture && index==90U) out->x+=64;
    return 1;
}
static int removed(uint32_t sprite) {
    assert(sprite<1024); return removedBits[sprite]!=0;
}
static void setRemoved(uint32_t sprite,int value) {
    assert(sprite<1024 && (value==0 || value==1)); removedBits[sprite]=(uint8_t)value; if(value) ++removals;
}
static int barrelRadiusCellSupported(uint16_t tile,const EspPlayerViewState *v,
    ActionTarget *out,int *barrel,int *isPlayer) {
    assert(v==&player && tile<1024 && lookupCount<(enemyMode?64U:16U));
    visited[lookupCount++]=tile;
    if(failLookup>=0 && (int)lookupCount-1==failLookup) return 0;
    memset(out,0,sizeof(*out));
    out->spriteIndex=entityId[tile]; out->tileIndex=tile;
    *barrel=kind[tile]==3; *isPlayer=kind[tile]==2;
    if(enemyMode && *barrel && removed(entityId[tile])) *barrel=0;
    if(kind[tile]==1) out->type=ACTION_ENTITY_ENEMY;
    return 1;
}
EspNativeGameplayPlayerDamageStatus EspNativeGameplayPlayerState_applyDamageNonlethal(
    uint16_t a,uint16_t b,EspNativeGameplayPlayerDamageResult *out) {
    assert(a==b && out); ++damageCalls; playerDamage=a; mark('P');
    if(enemyMode && !lethal) playerHp-=a;
    return lethal?ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_LETHAL_DEFERRED:
                  ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_OK;
}
EspNativeGameplayPlayerDamageStatus EspNativeGameplayPlayerState_applyDamageForDeath(
    uint16_t a,uint16_t b,EspNativeGameplayPlayerDamageResult *out) {
    assert(a==b && a==playerDamage && out && lethal);
    ++terminalCalls; out->healthBefore=(uint8_t)playerHp; playerHp=0;
    return ESP_NATIVE_GAMEPLAY_PLAYER_DAMAGE_LETHAL_COMMITTED;
}
static int barrelRadiusCellSupportedExcept(uint16_t tile,const EspPlayerViewState *v,
    uint16_t ignored,ActionTarget *out,int *barrel,int *isPlayer) {
    assert(ignored==50 && preflightFixture);
    if (entityId[tile]==ignored) { memset(out,0,sizeof *out); *barrel=*isPlayer=0; return 1; }
    return barrelRadiusCellSupported(tile,v,out,barrel,isPlayer);
}
__CONTAINS__
__PREFLIGHT__
__LEAF__
__WRAPPER__
static EspNativeGameplayMonsterPositionRecord position;
static EspNativeGameplayMonsterRecord origin;
static int armedSequence;
const EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_find(uint16_t sprite){
    return sprite==50U ? &origin : NULL;
}
int EspNativeGameplayMonsterCombat_splashSequenceArmed(uint32_t sequence){
    return sequence==7U && armedSequence;
}
static unsigned frames, arms, clears, sounds, pumps, words, graphics;
static int failFrame, failArm, failGraphics, failPosition, suppressFrame;
static int ensureOwner(void){return 1;}
const EspPlayerViewState* EspPlayerView_view(void){return &player;}
int EspNativeGameplayMonsterPosition_ensure(void){return !failPosition;}
const EspNativeGameplayMonsterPositionRecord* EspNativeGameplayMonsterPosition_find(uint16_t sprite){
    return sprite==50U && !failPosition ? &position : NULL;
}
EspNativeGraphicsCatalogStatus EspNativeGraphicsCatalog_ensureSprite(uint16_t logical){
    assert(logical==180U); ++graphics;
    return failGraphics?ESP_NATIVE_GRAPHICS_CATALOG_SOURCE_MISSING:ESP_NATIVE_GRAPHICS_CATALOG_OK;
}
static struct {struct Render_s* render;} roots;
#define EspNativeRootOwners_live() (&roots)
static void EspEntranceMusic_pumpGameplay(void){++pumps;}
int EspNativeSpriteRenderer_armTransient(uint16_t logical,uint8_t phase,int16_t x,int16_t y){
    assert(logical==180 && phase==arms && x==672 && y==672);
    ++arms; return (int)phase!=failArm;
}
void EspNativeSpriteRenderer_clearTransient(void){++clears;}
int EspNativeDynamicLineRender_renderAmbient(struct Render_s* render,EspNativeGameplayFrameStats* out){
    assert(render==roots.render && out && (enemyMode || words==0));
    out->finalPresented=(int)frames!=suppressFrame; return (int)frames++!=failFrame;
}
static int EspNativeAudioIntent_publish(uint16_t resource,uint8_t flags,uint8_t priority){
    assert(resource==5061U && flags==0U && priority==4U && frames==1U);
    ++sounds; return 1;
}
static int DoomRPG_randNextInt(Random_t* r){
    assert(r==&rng && (enemyMode ? (frames%3==0 && clears>0) : (frames==3 && clears==1 && words==0)));
    mark('W');++words; ++r->nextRand; return 132;
}
__PREPARE_DEATH__
__RUN_DEATH__
/* Enemy origin uses actual preflight, wave and radius code; mock only native
 * owners and rendering. The old direct-origin assertions remain unchanged. */
#define MAP_WIDTH 32U
#define ACTION_FEEDBACK_DAMAGE 6U
#define ACTION_TRAP_DAMAGE_FLASH_MS 500U
#define FEEDBACK_DAMAGE_RED565 0xb800U
#define DRPG_LOGE(...) printf(__VA_ARGS__)
static struct { char feedbackText[64]; uint8_t feedbackPending,feedbackKind,viewportFlashPending;
    uint16_t viewportFlashDurationMs,viewportFlashColor565; } actionState;
static int sequenceActive, savedHp, savedRand, failDeath, deathArms, commits, settles;
int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState* p) {
    memset(p,0,sizeof *p);p->param1=(uint32_t)playerHp;p->ammo[0]=17;return 1;
}
int EspNativeGameplayPlayerState_restore(const EspNativeGameplayPlayerState* p) {
    assert(p->ammo[0]==17);playerHp=(int)p->param1;return 1;
}
int EspNativeGameplayMonsterCombat_beginSplashSequence(DoomRPG_t* d,uint32_t seq) {
    assert(d && seq==7 && !sequenceActive);sequenceActive=1;savedHp=playerHp;savedRand=rng.nextRand;return 1;
}
int EspNativeGameplayMonsterCombat_cancelSplashSequence(DoomRPG_t* d) {
    assert(d);if(sequenceActive){playerHp=savedHp;rng.nextRand=savedRand;hitsCount=0;sequenceActive=0;}return 1;
}
int EspNativeGameplayMonsterCombat_commitSplashSequence(uint32_t seq) {
    assert(seq==7 && sequenceActive);sequenceActive=0;++commits;return 1;
}
int EspNativeGameplayMonsterCombat_commitTerminalSplashSequence(uint32_t seq) {
    return EspNativeGameplayMonsterCombat_commitSplashSequence(seq);
}
int EspNativeGameplayMonsterCombat_publishSplashAudioAfterCommit(uint32_t seq) {
    assert(seq==7 && !sequenceActive);return 1;
}
int EspNativeGameplayMonsterCombat_finishSplashPresentation(uint32_t seq) {
    assert(seq==7 && !sequenceActive);return 1;
}
int EspNativeGameplayPlayerDeath_armWithReservedRand(DoomRPG_t* d,uint32_t seq,uint16_t tile,uint8_t byte) {
    assert(d && seq==7 && tile==331 && byte<=rng.nextRand && playerHp==0);
    if(failDeath)return 0;
    ++deathArms;return 1;
}
int EspNativeSpriteRenderer_armTransientBatch(uint16_t logical,uint8_t phase,
    const EspNativeSpriteTransient* items,uint8_t count) {
    assert(logical==180 && phase==arms%3 && items && count==1);
    ++arms;return (int)(arms-1)!=failArm;
}
int EspNativeGameplayFrame_renderTurn(struct Render_s* render,uint8_t angle,EspNativeGameplayFrameStats* out) {
    assert(render==roots.render && angle==0 && out);++settles;out->finalPresented=1;return 1;
}
static int EspNativeGameplayFacingLabel_refresh(const char* reason){assert(reason);return 1;}
static int Esp32PlatformVideo_present(void){return 1;}
__ROLLBACK_BARRELS__
__ENEMY_READY__
__ENEMY_IMPACT__
static void init(void) {
    enemyMode=0; sequenceActive=failDeath=deathArms=commits=settles=0;
    memset(&actionState,0,sizeof actionState);
    memset(&player,0,sizeof player); memset(kind,0,sizeof kind);
    memset(entityId,0,sizeof entityId); memset(removedBits,0,sizeof removedBits);
    immutable.x=32; immutable.y=32; preflightFixture=0; rootSubtype=1;
    mapReady=1; mapReads=lookupCount=damageCalls=terminalCalls=removals=0;
    failLookup=-1; lethal=0; playerDamage=0;
    rng.nextRand=10; playerHp=30; hitsCount=orderCount=0; order[0]=0; failMonster=reviveMonster=0;
    player.active=1; position=(EspNativeGameplayMonsterPositionRecord){50,330,672,672};
    origin=(EspNativeGameplayMonsterRecord){.spriteIndex=50,.subtype=8,.alive=1};
    armedSequence=1;
    roots.render=(struct Render_s*)&player;
    frames=arms=clears=sounds=pumps=words=graphics=0; failFrame=failArm=suppressFrame=-1;failGraphics=failPosition=0;
}
typedef struct {
    uint16_t chain[16];
    ExplosionImpactJournal journal;
    uint16_t message;
    uint8_t count,triggered,playerHits,dead;
} Trial;
static Trial trial(void) { Trial t; memset(&t,0,sizeof t); t.chain[0]=50; t.count=1; t.journal.runtime=(DoomRPG_t*)&player; t.journal.sequence=7; t.journal.monsterSequenceArmed=1; return t; }
static int at(Trial *t,int x,int y,int allow) {
    return explosionTriggerNeighborsAt(50,x,y,&player,17,t->chain,&t->count,
        &t->journal,&t->triggered,&t->playerHits,&t->message,allow,&t->dead);
}
static int barrel(Trial *t) {
    return barrelTriggerNeighbors(50,&player,17,t->chain,&t->count,
        &t->journal,&t->triggered,&t->playerHits,&t->message,0,&t->dead);
}
static int runDeath(const EspNativeGameplayDeathBlastPlan *plan, EspNativeGameplayDeathBlastResult *result) {
    origin.alive=0;
    return EspNativeGameplayActionEngine_runDeathBlast((DoomRPG_t*)&player,7,plan,result);
}
static void initEnemy(void) {
    init(); enemyMode=1; preflightFixture=1; immutable.x=672; immutable.y=672;
    player.viewX=player.destX=736; player.viewY=player.destY=672;
    kind[330]=3; entityId[330]=50;
    kind[331]=2;
}
static int runEnemy(void) {
    return EspNativeGameplayActionEngine_enemyBarrelImpact((DoomRPG_t*)&player,7,50);
}
static void enemyTransactions(void) {
    initEnemy();
    assert(EspNativeGameplayActionEngine_enemyBarrelReady(50));
    lookupCount=0;
    assert(runEnemy()==1 && removed(50) && words==1 && damageCalls==1);
    assert(frames==3 && settles==1 && sounds==1 && rng.nextRand==11 &&
           playerHp<30 && actionState.feedbackPending==1);
    initEnemy();kind[329]=1;entityId[329]=70;
    assert(runEnemy()==1 && removed(50) && commits==1 && hitsCount==1 &&
           !sequenceActive && strcmp(order,"WMP")==0 && rng.nextRand==15);
    initEnemy();kind[331]=3;entityId[331]=90;kind[332]=2;
    assert(runEnemy()==1 && removed(50) && removed(90) && frames==6 && words==2 &&
           damageCalls==1 && rng.nextRand==12 && settles==1);
    initEnemy();lethal=1;
    assert(runEnemy()==1 && removed(50) && playerHp==0 && deathArms==1 &&
           rng.nextRand==12 && strcmp(order,"WPD")==0);
    initEnemy();lethal=1;failDeath=1;
    assert(runEnemy()==0 && !removed(50) && playerHp==30 && !deathArms && rng.nextRand==10);
    for(int f=0;f<3;++f){
        initEnemy();failFrame=f;
        assert(runEnemy()==0 && !removed(50) && playerHp==30 && rng.nextRand==10 && !words);
    }
    initEnemy();kind[331]=3;entityId[331]=90;kind[332]=2;failFrame=4;
    assert(runEnemy()==0 && !removed(50) && !removed(90) && playerHp==30 && rng.nextRand==10);
    initEnemy();kind[329]=1;entityId[329]=70;failMonster=1;
    assert(runEnemy()==0 && !removed(50) && !sequenceActive && playerHp==30 && rng.nextRand==10);
    initEnemy();failGraphics=1;
    assert(runEnemy()==0 && !removed(50) && !frames && !words);
    initEnemy();removedBits[50]=1;
    assert(runEnemy()==0 && removed(50) && !frames && !words);
    puts("GAMEAI14 P10 enemy barrel transaction PASS: single, splash, chain, lethal, rejected death, render/monster rollback, removed/resource rejection");
}
int main(void) {
    enemyTransactions();
    const uint16_t center=330;
    const uint16_t ordered[]={329,331,298,362,361,363,299,297};
    /* Real preflight for both root types with adjacent barrel, player and
     * ordinary enemy; identical chain/impact counts, no mutations/RNG. */
    for (uint8_t type=1;type<=2;++type) {
        init(); preflightFixture=1; rootSubtype=type; immutable.x=672;immutable.y=672;
        kind[331]=3;entityId[331]=90;kind[330]=4;entityId[330]=50;
        kind[329]=2;kind[299]=1;entityId[299]=70;
        uint16_t chain[16];uint8_t count=0,monsters=0,players=0;ActionTarget unsupported;
        assert(barrelChainPreflight(50,type,&player,chain,&count,&monsters,&players,&unsupported));
        assert(count==2 && chain[0]==50 && chain[1]==90 &&
               monsters==2 && players==1 && removals==0 && rng.nextRand==10);
        assert(!barrelChainPreflight(50,3,&player,chain,&count,&monsters,&players,&unsupported));
    }
    puts("GAMEAI14 shared type1 root preflight: barrel and trapped crate, shared chain and victims");
    init(); Trial t=trial();
    kind[329]=1;entityId[329]=70; kind[363]=1;entityId[363]=71;
    kind[331]=2; kind[362]=3;entityId[362]=90;
    kind[297]=3;entityId[297]=90; /* duplicate */
    assert(at(&t,672,672,0));
    assert(mapReads==0 && lookupCount==8 && memcmp(visited,ordered,sizeof ordered)==0);
    assert(t.journal.monsterHits==2 && hitsSeen[0].spriteIndex==70 && hitsSeen[0].healthComponent==17);
    assert(hitsSeen[1].spriteIndex==71 && hitsSeen[1].healthComponent==8);
    assert(hitsSeen[0].armorComponent==17 && hitsSeen[1].armorComponent==8);
    assert(t.playerHits==1 && t.message==34 && playerDamage==17 && damageCalls==1);
    assert(t.count==2 && t.chain[1]==90 && t.triggered==1 && removals==1);
    assert(strcmp(order,"MPM")==0 && rng.nextRand==18);
    Trial expect; memcpy(&expect,&t,sizeof t); init();
    kind[329]=1;entityId[329]=70; kind[363]=1;entityId[363]=71;
    kind[331]=2; kind[362]=3;entityId[362]=90; kind[297]=3;entityId[297]=90;
    immutable.x=672;immutable.y=672;t=trial();assert(barrel(&t));
    assert(mapReads==1 && t.count==expect.count && t.journal.monsterHits==expect.journal.monsterHits && t.triggered==expect.triggered && t.playerHits==expect.playerHits && t.message==expect.message && t.dead==expect.dead);
    assert(memcmp(t.chain,expect.chain,sizeof t.chain)==0 && strcmp(order,"MPM")==0);
    init();t=trial();assert(barrel(&t));
    assert(mapReads==1 && lookupCount==3 && visited[0]==1 && visited[1]==32 && visited[2]==33);
    init();t=trial();mapReady=0;assert(!barrel(&t));
    assert(mapReads==1 && lookupCount==0 && t.count==1 && !removals);
    /* Explicit current coordinates do not consult the immutable source. */
    assert(at(&t,672,672,0));assert(mapReads==1 && lookupCount==8);
    init();t=trial();failLookup=0;assert(!at(&t,672,672,0));
    assert(lookupCount==1 && !removals && !damageCalls && !t.journal.monsterHits);
    init();t=trial();kind[center-1]=1;t.journal.monsterHits=128;
    assert(!at(&t,672,672,0));assert(t.journal.monsterHits==128 && lookupCount==1);
    init();t=trial();kind[center-1]=3;entityId[center-1]=99;t.count=16;
    assert(!at(&t,672,672,0));assert(t.count==16 && !removals);
    init();t=trial();kind[center-1]=2;lethal=1;
    assert(!at(&t,672,672,0));assert(damageCalls==1 && !terminalCalls && !t.dead);
    init();t=trial();kind[center-1]=2;lethal=1;
    assert(at(&t,672,672,1));assert(damageCalls==1 && terminalCalls==1 && t.dead);
    assert(strcmp(order,"PD")==0 && t.journal.deathRandValid && t.journal.deathRand==10);
    for(unsigned p=0;p<8;p++) for(unsigned m=0;m<8;m++) if(p!=m) {
        init();t=trial();kind[ordered[p]]=2;kind[ordered[m]]=1;
        entityId[ordered[m]]=70;lethal=1;
        assert(at(&t,672,672,1));
        assert(strcmp(order,m<p?"MPD":"PDM")==0);
        assert(t.journal.deathRand==(m<p?14U:10U) && rng.nextRand==15);
        assert(t.journal.monsterHits==1 && t.dead && t.journal.deathRandValid);
    }
    init();t=trial();kind[329]=1;kind[331]=2;lethal=1;failMonster=1;
    assert(!at(&t,672,672,1) && strcmp(order,"M")==0 && !t.journal.deathRandValid);
    init();t=trial();kind[329]=2;kind[331]=1;lethal=1;failMonster=1;
    assert(!at(&t,672,672,1) && strcmp(order,"PDM")==0 && t.journal.deathRandValid);
    init();t=trial();kind[329]=2;kind[331]=1;lethal=1;reviveMonster=1;
    assert(!at(&t,672,672,1) && playerHp==30);
    init();t=trial();kind[329]=1;t.journal.monsterSequenceArmed=0;
    assert(!at(&t,672,672,1) && orderCount==0);
    puts("GAMEAI14 A2-P1c PASS actual ordered radius leaf, 56 player/monster ordinal pairs, exact death byte, fail-closed level-up and capacity");
    /* A2-P1h: death at every ordinal of a three-barrel chain, including
     * actual material incident (second blast). Subsequent damage still
     * commits, but only one byte is drawn for death. */
    for (uint8_t death=0;death<3;death++) {
        init();t=trial();kind[329]=2;
        for (uint8_t cursor=0;cursor<3;cursor++) {
            lookupCount=0; mark('W'); rng.nextRand+=4;
            lethal=(cursor>=death);
            assert(at(&t,672,672,
                EspNativeGameplayBarrelRadius_terminalDeathOwnedAt(3,cursor)));
            assert(t.dead==(cursor>=death));
        }
        const char* expected[]={"WPDWPWP","WPWPDWP","WPWPWPD"};
        assert(strcmp(order,expected[death])==0 &&
               terminalCalls==3U-(unsigned)death && damageCalls==3U);
        assert(t.journal.randomBeforeDeathByte.nextRand==14+4*death &&
               t.journal.deathRand==14+4*death && rng.nextRand==23 &&
               playerHp==0);
    }
    /* A monster consequence after an early death still consumes its RNG
     * before the next blast. No second death draw. */
    init();t=trial();kind[329]=2;kind[331]=1;entityId[331]=70;lethal=1;
    for (uint8_t cursor=0;cursor<2;cursor++) {
        lookupCount=0;mark('W');rng.nextRand+=4;
        assert(at(&t,672,672,1));
    }
    assert(strcmp(order,"WPDMWPM")==0 && t.journal.deathRand==14 &&
           rng.nextRand==27 && terminalCalls==2 && t.journal.monsterHits==2);
    puts("GAMEAI14 A2-P1h PASS death at every blast ordinal, one death byte, post-death damage and ordered monster RNG");
    /* Actual direct-origin prepare and run bodies. Fixture leaves cover the
     * moved center, resource gate, frame-before-RNG ordering and mixed rejection. */
    EspNativeGameplayDeathBlastPlan plan;
    EspNativeGameplayDeathBlastResult result;
    init();kind[329]=1;entityId[329]=70;kind[331]=2;
    assert(EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan));
    assert(plan.sourceSprite==50 && plan.worldX==672 && plan.worldY==672 &&
           plan.monsterCount==1 && plan.playerCount==1 && graphics==1 && mapReads==0 && !words);
    assert(runDeath(&plan,&result));
    assert(frames==3 && arms==3 && clears==1 && sounds==1 && words==1);
    assert(result.monsterHits==1 && result.playerMessageDamage==34 && !result.lethal);
    assert(strcmp(order,"WMP")==0 && rng.nextRand==15 && lookupCount==16);
    init();kind[329]=2;kind[331]=1;entityId[331]=70;lethal=1;
    assert(EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan));
    assert(runDeath(&plan,&result));
    assert(result.lethal && result.deathRand==11 && strcmp(order,"WPDM")==0);
    init();kind[329]=3;entityId[329]=90;
    assert(!EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan) && !graphics && !words && !removals);
    init();failGraphics=1;assert(!EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan) && graphics==1 && !words);
    init();failPosition=1;assert(!EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan) && !graphics);
    init();assert(EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan));
    position.worldX=736;assert(!runDeath(&plan,&result) && !frames && !words);
    for(int f=0;f<3;++f){
        init();assert(EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan));failFrame=f;
        assert(!runDeath(&plan,&result));
        assert(frames==(unsigned)f+1 && clears==1 && !words && !orderCount);
        init();assert(EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan));suppressFrame=f;
        assert(!runDeath(&plan,&result));
        assert(frames==(unsigned)f+1 && clears==1 && !words && !orderCount);
        assert(sounds==(unsigned)(f!=0));
        init();assert(EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan));failArm=f;
        assert(!runDeath(&plan,&result));
        assert(arms==(unsigned)f+1 && frames==(unsigned)f && clears==1 && !words);
    }
    init();assert(EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan));
    kind[329]=3;entityId[329]=90;
    assert(!runDeath(&plan,&result));
    assert(removals==1 && !removed(90) && words==1);
    init();origin.subtype=7;assert(!EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan) && !graphics);
    init();origin.param1=1;assert(!EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan) && !graphics);
    init();assert(EspNativeGameplayActionEngine_prepareDeathBlast(50,&plan));
    assert(!EspNativeGameplayActionEngine_runDeathBlast((DoomRPG_t*)&player,7,&plan,&result) && !frames);
    armedSequence=0;assert(!runDeath(&plan,&result) && !frames && !words);
    puts("GAMEAI14 A2-P1d PASS direct moved-origin blast, preview frames, one RNG word, ordered victims, terminal byte, stale/unarmed/resource/frame/mixed rejection");

    return 0;
}
