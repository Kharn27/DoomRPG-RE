#!/usr/bin/env python3
"""GAME-AI-14 A1: compile ACTUAL production C attack-candidate/RNG-path bodies.

Mock only external arena/topology/LOS leaves, not the native algorithms.
This covers the 13 generic subtypes and both weapon variants under cardinal,
diagonal, blocked, out-of-range and invalid-view fixtures; all 14 masks are tested.
Subtype 10 requires its own behavioral oracle (#438), not a mandatory skip in production.
Then compares
the native movement RNG call-count calculation to an independent legacy
Entity_aiThink expression. This does NOT claim full turn/collision parity.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
TURN = (SRC / "esp_native_gameplay_monster_turn.c").read_text()
MOVE = (SRC / "esp_native_gameplay_monster_movement.c").read_text()
PUBLISH = (SRC / "esp_native_gameplay_monster_movement_publish.c").read_text()
LEGACY = (ROOT / "src/Entity.c").read_text()
TURN_HEADER = (ROOT / "ESP32/include/esp_native_gameplay_monster_turn.h").read_text()
RETALIATION = (SRC / "esp_native_gameplay_monster_retaliation.c").read_text()
assert "if (!EspNativeGameplayMonsterTurn_isFallbackProbe(turn->attackProbes) &&" in RETALIATION
assert "else if (z) {" in LEGACY and "Entity_attack(entity);" in LEGACY

def declaration(source: str, start_marker: str, end_marker: str) -> str:
    assert source.count(start_marker) == 1, (start_marker, "ambiguous")
    start = source.index(start_marker)
    end = source.index(end_marker, start) + len(end_marker)
    return source[start:end]

def function(source: str, marker: str) -> str:
    assert source.count(marker) == 1, (marker, "ambiguous")
    start = source.index(marker)
    opened = source.index("{", start)
    level = 0
    for i in range(opened, len(source)):
        if source[i] == "{":
            level += 1
        elif source[i] == "}":
            level -= 1
            if level == 0:
                return source[start:i + 1]
    raise AssertionError("unbalanced production function: " + marker)

for k in ("entity->def->eSubType == 10", "DoomRPG_randNextByte(&entity->doomRpg->random) >= 217",
          "DoomRPG_randNextByte(&entity->doomRpg->random) < 38",
          "Entity_aiGoal_MOVE(entity, sprite->x, sprite->y, i9, i10)"):
    assert k in LEGACY, ("legacy source changed", k)

prefix = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define DRPG_LOGT(...) ((void)0)
#define TURN_MAP_WIDTH 32U
#define TURN_TILE_SIZE 64
#define TURN_TILE_CENTER 32
#define TURN_TYPE_ENEMY 1U
#define TURN_TYPE_DESTRUCTIBLE 12U
#define TURN_SUBTYPE_SPECIAL_AI 10U
#define TURN_MONSTER_WEAPON_COUNT 19U
#define ESP_MAP_SPRITE_TOPOLOGY_LINKED 0x0400U
#define ESP_MAP_SPRITE_TOPOLOGY_ALIVE 0x1000U
#define ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK 0x03ffU
#define MOVE_TRACE_LEGACY_MASK 0xff87U
#define PUBLISH_MOVE_WEAPON_COUNT 19U
#define PUBLISH_MOVE_TILE_SIZE 64
#define PUBLISH_MOVE_SPECIAL_AI 10U

typedef struct EspNativeGameplayMonsterRecord_s {
    uint32_t param1, param2;
    uint16_t spriteIndex, defTile;
    uint8_t subtype, mType, alternateAttack, alive;
} EspNativeGameplayMonsterRecord;
typedef struct EspNativeGameplayMonsterPositionRecord_s {
    uint16_t spriteIndex, tileIndex, worldX, worldY;
} EspNativeGameplayMonsterPositionRecord;
typedef struct EspPlayerViewState_s {
    int32_t viewX, viewY, destX, destY, viewAngle, destAngle;
    uint8_t active;
} EspPlayerViewState;
typedef struct EspMapSpriteTopologyView_s { unsigned int ready; }
    EspMapSpriteTopologyView;

static EspNativeGameplayMonsterRecord fixtureMonster;
static EspMapSpriteTopologyView fixtureTopology = {1U};
static uint16_t fixtureTile = 99U;
static uint16_t fixtureFlags = ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                               ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
static int fixtureLos = 1;

static const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void) {
    return &fixtureTopology;
}
static const EspNativeGameplayMonsterRecord*
EspNativeGameplayMonsterState_find(uint16_t spriteIndex) {
    return spriteIndex == fixtureMonster.spriteIndex ? &fixtureMonster : NULL;
}
static int EspMapSpriteTopology_getEntity(uint32_t spriteIndex,
        uint8_t* type, uint8_t* sub, uint16_t* link, uint16_t* order) {
    if (spriteIndex != fixtureMonster.spriteIndex) return 0;
    *type = TURN_TYPE_ENEMY;
    *sub = fixtureMonster.subtype;
    *link = fixtureFlags | fixtureTile;
    *order = 1U;
    return 1;
}
typedef struct MonsterTurnTraceTarget_s {
    uint16_t sprite,tile; uint8_t type,subtype;
} MonsterTurnTraceTarget;
static int fixtureBarrel=0;
static int EspNativeGameplayActionEngine_enemyBarrelReady(uint16_t sprite) {
    return fixtureBarrel && sprite==271U;
}
static int cardinalLosTarget(int32_t sourceX, int32_t sourceY,
        int32_t destX, int32_t destY, uint16_t spriteIndex,
        MonsterTurnTraceTarget* target) {
    *target=(MonsterTurnTraceTarget){271U,570U,fixtureBarrel?12U:0xffU,1U};
    (void)sourceX; (void)sourceY; (void)destX; (void)destY;
    (void)spriteIndex;
    return fixtureLos;
}
"""
# P11: run the actual production producer and retained gate. Stub external
# combat math, player, RNG and render owners; the movement transaction oracle
# separately uses the actual legacy generator and refill reservation.
owner_fixture = r"""
typedef struct DoomRPG_s { int dummy; } DoomRPG_t;
typedef struct { int nextRand; uint8_t randTable[128]; } Random_t;
typedef struct { uint32_t param1; } EspNativeGameplayPlayerState;
static MonsterTurnOwner turnOwner;
static EspNativeGameplayMonsterTurnView delivered;
static EspPlayerViewState fixtureView;
static EspNativeGameplayPlayerState fixturePlayer;
static Random_t fixtureRng;
static unsigned rollCalls;
static const EspPlayerViewState* EspPlayerView_view(void){return &fixtureView;}
static const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void){return &fixturePlayer;}
static int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState* p){*p=fixturePlayer;return 1;}
static int EspNativeGameplayPlayerState_restore(const EspNativeGameplayPlayerState* p){fixturePlayer=*p;return 1;}
static uint32_t EspNativeGameplayPlayerState_fingerprint(void){return fixturePlayer.param1;}
static const EspNativeGameplayMonsterTurnView* EspNativeGameplayMonsterActivation_turnView(void){return &delivered;}
static int syncOwner(void){return 1;}
static const char* reasonName(uint8_t reason){(void)reason;return "PASS_TURN";}
static Random_t* EspNativeProcessRng_live(void){return &fixtureRng;}
static uint32_t randomFNV(const Random_t* r){return (uint32_t)r->nextRand;}
static uint8_t p1Health(uint32_t p){return (uint8_t)p;}
static uint8_t p1Armor(uint32_t p){return (uint8_t)(p>>16);}
static uint8_t DoomRPG_randNextByte(Random_t* r){assert(r->nextRand<128);return r->randTable[r->nextRand++];}
static int EspNativeRngReplayGuard_beginAttackProbe(Random_t* r){assert(r==&fixtureRng);return 1;}
static int EspNativeRngReplayGuard_endAttackProbe(Random_t* r,const Random_t* b){return memcmp(r,b,sizeof *r)==0;}
static int rollMonsterAttack(DoomRPG_t* d,const EspNativeGameplayMonsterRecord* m,
    const EspNativeGameplayPlayerState* p,uint8_t weapon,uint8_t loops,MonsterTurnRoll* roll){
    assert(d && m==&fixtureMonster && p==&fixturePlayer && weapon==15U && loops==1U);
    ++rollCalls;roll->loops=loops;roll->rngCalls=2U;
    (void)DoomRPG_randNextByte(&fixtureRng);(void)DoomRPG_randNextByte(&fixtureRng);return 1;
}
static void prospectivePlayerPain(const EspNativeGameplayPlayerState* p,int32_t damage,
    int32_t armor,uint8_t* hp,uint8_t* ar){assert(damage==0 && armor==0);*hp=p1Health(p->param1);*ar=p1Armor(p->param1);}
"""
tests = r"""
static void settle(EspPlayerViewState* v, int32_t x, int32_t y) {
    memset(v, 0, sizeof(*v));
    v->active = 1U;
    v->viewX = v->destX = x;
    v->viewY = v->destY = y;
}
static void setupGate(uint8_t aiByte,int barrel,int los) {
    memset(&fixtureMonster,0,sizeof fixtureMonster);
    fixtureMonster.spriteIndex=17U;fixtureMonster.subtype=3U;fixtureMonster.alive=1U;
    fixtureTile=99U;
    fixtureFlags=ESP_MAP_SPRITE_TOPOLOGY_LINKED|ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
    fixtureBarrel=barrel;fixtureLos=los;
    settle(&fixtureView,288,224);
    memset(&turnOwner,0,sizeof turnOwner);
    turnOwner.view.active=1U;turnOwner.view.probes=1U;turnOwner.view.sourceArenaFNV1a=77U;
    delivered=turnOwner.view;
    fixturePlayer.param1=40U|(28U<<16);
    memset(&fixtureRng,0,sizeof fixtureRng);
    fixtureRng.randTable[0]=aiByte;fixtureRng.randTable[1]=37U;
    rollCalls=0U;
}
static void fallbackCases(void) {
    DoomRPG_t root={0};
    uint16_t sprite,tile;
    int ready;
    uint8_t aiByte;
    for(int barrel=0;barrel<2;++barrel) {
        setupGate(216U,barrel,!barrel);
        assert(EspNativeGameplayMonsterTurn_probeActiveMember(&root,17U)==ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_ATTACK_PUBLISHED);
        assert(fixtureRng.nextRand==0 && turnOwner.view.attackProbes==1U &&
               rollCalls==(unsigned)!barrel &&
               !EspNativeGameplayMonsterTurn_isFallbackProbe(1U));
        assert(EspNativeGameplayMonsterTurn_barrelTarget(1U,&sprite,&tile)==barrel);
        if(barrel) assert(sprite==271U && tile==570U);
        for(unsigned byte=217;byte<=255;byte+=38) {
            setupGate((uint8_t)byte,barrel,!barrel);
            assert(EspNativeGameplayMonsterTurn_probeActiveMember(&root,17U)==ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_RANGED_MOVE);
            assert(fixtureRng.nextRand==0 && !turnOwner.view.attackProbes && !rollCalls);
            assert(EspNativeGameplayMonsterTurn_movementGate(17U,99U,&ready,&aiByte) && ready && aiByte==byte);
            assert(!EspNativeGameplayMonsterTurn_movementGate(18U,99U,&ready,&aiByte));
            assert(!EspNativeGameplayMonsterTurn_movementGate(17U,100U,&ready,&aiByte));
            fixtureView.viewAngle=fixtureView.destAngle=64;
            assert(!EspNativeGameplayMonsterTurn_movementGate(17U,99U,&ready,&aiByte));
            fixtureView.viewAngle=fixtureView.destAngle=0;
            /* The actual publisher commits AI and close-bias before this
             * call. The fallback must never repeat either draw. */
            assert(DoomRPG_randNextByte(&fixtureRng)==byte);
            assert(DoomRPG_randNextByte(&fixtureRng)==37U);
            Random_t afterPrefix=fixtureRng;
            assert(EspNativeGameplayMonsterTurn_fallbackNoMove(&root,17U,99U));
            assert(memcmp(&fixtureRng,&afterPrefix,sizeof fixtureRng)==0 &&
                   turnOwner.view.attackProbes==1U && rollCalls==(unsigned)!barrel &&
                   EspNativeGameplayMonsterTurn_isFallbackProbe(1U));
            assert(EspNativeGameplayMonsterTurn_barrelTarget(1U,&sprite,&tile)==barrel);
            if(barrel) assert(sprite==271U && tile==570U);
            assert(!EspNativeGameplayMonsterTurn_fallbackNoMove(&root,17U,99U));
        }
    }
    setupGate(255U,1,0);
    assert(EspNativeGameplayMonsterTurn_probeActiveMember(&root,17U)==ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_RANGED_MOVE);
    fixtureBarrel=0;
    assert(!EspNativeGameplayMonsterTurn_fallbackNoMove(&root,17U,99U) && !turnOwner.view.attackProbes);
    setupGate(255U,0,0);
    assert(EspNativeGameplayMonsterTurn_probeActiveMember(&root,17U)==ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_NO_IMMEDIATE_ATTACK);
    assert(EspNativeGameplayMonsterTurn_movementGate(17U,99U,&ready,&aiByte) && !ready);
    assert(!EspNativeGameplayMonsterTurn_fallbackNoMove(&root,17U,99U));
    puts("PASS P11 real producer: 216/217/255, player/barrel identity, no AI reroll, stale/duplicate rejection");
}
int main(void) {
    EspPlayerViewState view;
    EspNativeGameplayMonsterPositionRecord before;
    MonsterTurnCandidate candidate;
    unsigned int subtype, alternate, cases=0U;
    uint8_t calls;
    unsigned int mask;

    memset(&before,0,sizeof(before));
    before.spriteIndex=17U;
    before.tileIndex=99U;
    before.worldX=224U;
    before.worldY=224U;
    for (subtype=0U; subtype<14U; ++subtype) {
        mask=0xff87U;
        if (subtype==4U || subtype==13U) mask &= ~0x0c00U;
        else if (subtype==6U || subtype==7U) mask &= ~0x0800U;
        else if (subtype==10U) mask &= ~0x0400U;
        assert(movementMaskForSubtype((uint8_t)subtype)==mask);

        if (subtype==10U) {
            puts("[GAMEAI14-C] subtype=10 generic-candidate/RNG cases=SKIPPED special-oracle=#438 mask=PASS");
            continue;
        }
        for (alternate=0U; alternate<2U; ++alternate) {
            const uint8_t id=monsterAttacks[2U*subtype+alternate];
            const int i7=(1+(int)monsterRangeMin[id])/2;
            memset(&fixtureMonster,0,sizeof(fixtureMonster));
            fixtureMonster.spriteIndex=17U;
            fixtureMonster.subtype=(uint8_t)subtype;
            fixtureMonster.alternateAttack=(uint8_t)alternate;
            fixtureMonster.alive=1U;
            fixtureTile=99U;
            fixtureFlags=ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                         ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
            fixtureLos=1;

            settle(&view,288,224);  /* cardinal one tile away */
            assert(findCandidateForSprite(&view,17U,&candidate)==1);
            assert(candidate.monster==&fixtureMonster);
            assert(candidate.weaponId==id);
            assert(candidate.loops==monsterShots[subtype]);
            assert(candidate.tileIndex==99U);
            assert(candidate.worldDistance==4096U);
            ++cases;

            settle(&view,288,288); /* diagonal */
            assert(findCandidateForSprite(&view,17U,&candidate)==1);
            assert(candidate.monster==NULL);
            ++cases;

            settle(&view,1952,224); /* outside every weapon range */
            assert(findCandidateForSprite(&view,17U,&candidate)==1);
            assert(candidate.monster==NULL);
            ++cases;

            settle(&view,288,224);
            fixtureLos=0; /* non-owned external trace returns blocked */
            assert(findCandidateForSprite(&view,17U,&candidate)==1);
            assert(candidate.monster==NULL);
            fixtureLos=1;
            ++cases;

            view.active=0;
            assert(findCandidateForSprite(&view,17U,&candidate)==0);
            ++cases;

            settle(&view,288,224);
            fixtureFlags=0U; /* topology unlinked: invalid */
            assert(findCandidateForSprite(&view,17U,&candidate)==0);
            assert(candidate.monster==NULL);
            fixtureFlags=ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                         ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
            ++cases;

            calls=255U;
            assert(movementRngCalls("NO-IMMEDIATE-ATTACK",
                &fixtureMonster,&view,&before,&calls)==1);
            assert(calls==(uint8_t)(1+(i7>0)));
            ++cases;

            calls=255U;
            assert(movementRngCalls("RANGED-AI",
                &fixtureMonster,&view,&before,&calls)==
                (i7>0));
            if(i7>0) assert(calls==3U);
            else assert(calls==0U);
            ++cases;

            /* Far cardinal and close diagonal must not consume close-bias. */
            settle(&view,1952,224);
            calls=255U;
            assert(movementRngCalls("NO-IMMEDIATE-ATTACK",
                &fixtureMonster,&view,&before,&calls)==1);
            assert(calls==1U);
            ++cases;
            calls=255U;
            assert(movementRngCalls("RANGED-AI",
                &fixtureMonster,&view,&before,&calls)==(i7>0));
            assert(calls==(i7>0?2U:0U));
            ++cases;
            settle(&view,288,288);
            calls=255U;
            assert(movementRngCalls("NO-IMMEDIATE-ATTACK",
                &fixtureMonster,&view,&before,&calls)==1);
            assert(calls==1U);
            ++cases;
            calls=255U;
            assert(movementRngCalls("INVALID",
                &fixtureMonster,&view,&before,&calls)==0);
            assert(calls==0U);
            ++cases;
        }
        printf("[GAMEAI14-C] subtype=%02u variants=2 each=12 cases=24 "
               "attackGate=%s metadata=legacy-matched activeRng=not-consumed\n",
               subtype, "native-candidate");
    }
    fixtureMonster.subtype=3U;
    fixtureMonster.alternateAttack=0U;
    fixtureLos=0; fixtureBarrel=1;
    settle(&view,288,224);
    assert(findCandidateForSprite(&view,17U,&candidate)==1);
    assert(candidate.monster==&fixtureMonster && candidate.barrelTarget==1U &&
           candidate.weaponId==15U && candidate.targetSprite==271U &&
           candidate.targetTile==570U);
    fixtureMonster.alternateAttack=1U;
    assert(findCandidateForSprite(&view,17U,&candidate)==1);
    assert(candidate.monster==NULL);
    assert(cases==13U*2U*12U);
    fallbackCases();
    printf("[GAMEAI14-C] PASS cases=%u realBodies=3 "
           "cardinal/diagonal/range/LOS/invalid/link/aiRngCounts/masks "
           "full-turn-fidelity=NOT-PROVEN\n",cases);
    return 0;
}
"""
parts = [
    prefix,
    declaration(TURN, "typedef struct MonsterWeaponSpec_s {", "} MonsterWeaponSpec;"),
    declaration(TURN, "typedef struct MonsterTurnCandidate_s {", "} MonsterTurnCandidate;"),
    declaration(TURN, "typedef struct MonsterTurnRoll_s {", "} MonsterTurnRoll;"),
    declaration(TURN_HEADER, "typedef enum EspNativeGameplayMonsterMemberProbeStatus_e {", "} EspNativeGameplayMonsterMemberProbeStatus;"),
    declaration(TURN_HEADER, "typedef struct EspNativeGameplayMonsterTurnView_s {", "} EspNativeGameplayMonsterTurnView;"),
    declaration(TURN, "typedef struct MonsterTurnOwner_s {", "} MonsterTurnOwner;"),
    owner_fixture,
    declaration(TURN, "static const uint8_t monsterAttacks[28] =", "};"),
    declaration(TURN, "static const uint8_t monsterShots[14] =", "};"),
    declaration(TURN, "static const MonsterWeaponSpec monsterWeapons[TURN_MONSTER_WEAPON_COUNT] =", "};"),
    declaration(PUBLISH, "static const uint8_t monsterRangeMin[PUBLISH_MOVE_WEAPON_COUNT] =", "};"),
    declaration(PUBLISH, "static const uint8_t monsterWeaponValid[PUBLISH_MOVE_WEAPON_COUNT] =", "};"),
    function(TURN, "static int centeredCoordinate("),
    function(TURN, "static int tileCenter("),
    function(TURN, "static int tileIndexFor("),
    function(TURN, "static int findCandidateForSprite("),
    function(TURN, "static int retainMovementGate("),
    function(TURN, "int EspNativeGameplayMonsterTurn_movementGate("),
    function(TURN, "static EspNativeGameplayMonsterMemberProbeStatus probeMember("),
    function(TURN, "EspNativeGameplayMonsterMemberProbeStatus\nEspNativeGameplayMonsterTurn_probeActiveMember("),
    function(TURN, "int EspNativeGameplayMonsterTurn_fallbackNoMove("),
    function(TURN, "int EspNativeGameplayMonsterTurn_isFallbackProbe("),
    function(TURN, "int EspNativeGameplayMonsterTurn_barrelTarget("),
    function(MOVE, "static uint16_t movementMaskForSubtype("),
    function(PUBLISH, "static int movementRngCalls("),
    tests,
]
with tempfile.TemporaryDirectory(prefix="gameai14_real_c_") as d:
    p=Path(d)
    unit=p/"gameai14.c"
    exe=p/"gameai14"
    unit.write_text("\n\n".join(parts), encoding="utf-8")
    subprocess.run(["cc", "-std=c11","-O2","-Wall","-Wextra","-Werror",
                    str(unit),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print("game_ai14_candidate_rng_host: PASS 312 generic actual-C path scenarios + 14 masks; "
      "external geometry & RNG draws stubbed, full AI parity not claimed")
