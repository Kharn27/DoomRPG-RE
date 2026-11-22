#!/usr/bin/env python3
"""GAME-AI-14 A1: compile ACTUAL production C attack-candidate/RNG-path bodies.

Mock only external arena/topology/LOS leaves, not the native algorithms.
This covers all fourteen subtypes and both weapon variants under cardinal,
diagonal, blocked, out-of-range and invalid-view fixtures; then compares
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
static int cardinalLosClear(int32_t sourceX, int32_t sourceY,
        int32_t destX, int32_t destY, uint16_t spriteIndex) {
    (void)sourceX; (void)sourceY; (void)destX; (void)destY;
    (void)spriteIndex;
    return fixtureLos;
}
"""
tests = r"""
static void settle(EspPlayerViewState* v, int32_t x, int32_t y) {
    memset(v, 0, sizeof(*v));
    v->active = 1U;
    v->viewX = v->destX = x;
    v->viewY = v->destY = y;
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
            if(subtype==10U) {
                /* The pre-#440 main has an intentional subtype10 skip.
                 * Source audit fails once this changes until the fixture
                 * is reviewed for the new owner and special RNG prefix. */
                assert(candidate.monster==NULL);
            } else {
                assert(candidate.monster==&fixtureMonster);
                assert(candidate.weaponId==id);
                assert(candidate.loops==monsterShots[subtype]);
                assert(candidate.tileIndex==99U);
                assert(candidate.worldDistance==4096U);
            }
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
            fixtureFlags=0U; /* topology unlinked: generic invalid, subtype10 main skips */
            assert(findCandidateForSprite(&view,17U,&candidate)==(subtype==10U));
            assert(candidate.monster==NULL);
            fixtureFlags=ESP_MAP_SPRITE_TOPOLOGY_LINKED |
                         ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
            ++cases;

            calls=255U;
            assert(movementRngCalls("NO-IMMEDIATE-ATTACK",
                &fixtureMonster,&view,&before,&calls)==(subtype!=10U));
            if(subtype!=10U) assert(calls==(uint8_t)(1+(i7>0)));
            else assert(calls==0U);
            ++cases;

            calls=255U;
            assert(movementRngCalls("RANGED-AI",
                &fixtureMonster,&view,&before,&calls)==
                (subtype!=10U && i7>0));
            if(subtype!=10U && i7>0) assert(calls==3U);
            else assert(calls==0U);
            ++cases;

            calls=255U;
            assert(movementRngCalls("INVALID",
                &fixtureMonster,&view,&before,&calls)==0);
            assert(calls==0U);
            ++cases;
        }
        printf("[GAMEAI14-C] subtype=%02u variants=2 each=9 cases=18 "
               "attackGate=%s metadata=legacy-matched activeRng=not-consumed\n",
               subtype, subtype==10U?"main-legacy-defer":"native-candidate");
    }
    assert(cases==14U*2U*9U);
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
    declaration(TURN, "static const uint8_t monsterAttacks[28] =", "};"),
    declaration(TURN, "static const uint8_t monsterShots[14] =", "};"),
    declaration(TURN, "static const MonsterWeaponSpec monsterWeapons[TURN_MONSTER_WEAPON_COUNT] =", "};"),
    declaration(PUBLISH, "static const uint8_t monsterRangeMin[PUBLISH_MOVE_WEAPON_COUNT] =", "};"),
    declaration(PUBLISH, "static const uint8_t monsterWeaponValid[PUBLISH_MOVE_WEAPON_COUNT] =", "};"),
    function(TURN, "static int centeredCoordinate("),
    function(TURN, "static int tileCenter("),
    function(TURN, "static int findCandidateForSprite("),
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
print("game_ai14_candidate_rng_host: PASS 252 actual-C path scenarios; "
      "external geometry & RNG draws stubbed, full AI parity not claimed")
