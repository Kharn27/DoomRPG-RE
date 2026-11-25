/* A1b: actual publisher + probe + RNG provider; injected owner leaves only. */
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
#include "esp_native_gameplay_monster_movement_publish.h"
#include "esp_native_gameplay_monster_movement_probe.h"
#include "esp_native_gameplay_monster_turn.h"
#include "esp_player_view_state.h"

static unsigned int tick = 12345;
unsigned int DoomRPG_GetTimeMS(void) { return tick; }
unsigned int DoomRPG_GetUpTimeMS(void) { return tick; }
void Legacy_DoomRPG_setRand(Random_t*);
unsigned int Legacy_Random_Get(void);

enum Fault { OK, NO_MOVE, PREPARE_FAIL, POSITION_FAIL, TOPOLOGY_FAIL, RENDER_FAIL, POST_RENDER_FAIL };
static enum Fault fault;
static DoomRPG_t root;
static EspNativeRootOwners_t roots;
static EspNativeGameplayMonsterRecord monster;
static EspNativeGameplayMonsterView monsters;
static EspNativeGameplayMonsterPositionRecord position, before, after;
static EspMapSpriteTopologyView topology;
static EspPlayerViewState player;
static EspNativeGameplayMonsterMovementView movement;
static uint16_t tile, order;
static unsigned renderCalls, postMoveCalls, plannerCalls;

EspNativeRootOwners_t* EspNativeRootOwners_live(void) { return &roots; }
const EspPlayerViewState* EspPlayerView_view(void) { return &player; }
const EspNativeGameplayMonsterView* EspNativeGameplayMonsterState_view(void) { return &monsters; }
const EspNativeGameplayMonsterRecord* EspNativeGameplayMonsterState_find(uint16_t sprite) {
    return sprite == monster.spriteIndex ? &monster : NULL;
}
const EspNativeGameplayMonsterPositionRecord* EspNativeGameplayMonsterPosition_find(uint16_t sprite) {
    return sprite == position.spriteIndex ? &position : NULL;
}
uint32_t EspNativeGameplayMonsterPosition_fingerprint(void) { return position.tileIndex; }
const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void) { return &topology; }
int EspMapSpriteTopology_getEntity(uint32_t sprite, uint8_t* type, uint8_t* sub,
                                  uint16_t* link, uint16_t* outOrder) {
    if (sprite != monster.spriteIndex) return 0;
    *type = ESP_MAP_ENTITY_TYPE_ENEMY; *sub = monster.subtype;
    *link = tile | ESP_MAP_SPRITE_TOPOLOGY_LINKED | ESP_MAP_SPRITE_TOPOLOGY_ALIVE;
    *outOrder = order;
    if (fault == POST_RENDER_FAIL && renderCalls == 1U) *outOrder = 999U;
    return 1;
}
int EspMapSpriteTopology_prepareRelink(uint16_t sprite, uint16_t dest, EspMapSpriteTopologyRelink* p) {
    if (fault == PREPARE_FAIL) return 0;
    memset(p, 0, sizeof(*p)); p->spriteIndex = sprite;
    p->sourceTile = tile; p->destTile = dest;
    p->linkOrderBefore = order; p->linkOrderAfter = topology.nextLinkOrder;
    p->nextLinkOrderBefore = topology.nextLinkOrder;
    p->nextLinkOrderAfter = topology.nextLinkOrder + 1U;
    return 1;
}
int EspMapSpriteTopology_commitPreparedRelink(const EspMapSpriteTopologyRelink* p) {
    if (fault == TOPOLOGY_FAIL) return 0;
    assert(tile == p->sourceTile && order == p->linkOrderBefore);
    tile = p->destTile; order = p->linkOrderAfter;
    topology.nextLinkOrder = p->nextLinkOrderAfter; topology.stateFNV1a = tile;
    return 1;
}
int EspMapSpriteTopology_rollbackPreparedRelink(const EspMapSpriteTopologyRelink* p) {
    assert(tile == p->destTile && order == p->linkOrderAfter);
    tile = p->sourceTile; order = p->linkOrderBefore;
    topology.nextLinkOrder = p->nextLinkOrderBefore; topology.stateFNV1a = tile;
    return 1;
}
int EspNativeGameplayMonsterPosition_commitPrepared(
    const EspNativeGameplayMonsterPositionRecord* b, const EspNativeGameplayMonsterPositionRecord* a) {
    if (fault == POSITION_FAIL) return 0;
    assert(memcmp(&position, b, sizeof(position)) == 0); position = *a; return 1;
}
int EspNativeGameplayMonsterPosition_rollbackPrepared(
    const EspNativeGameplayMonsterPositionRecord* a, const EspNativeGameplayMonsterPositionRecord* b) {
    assert(memcmp(&position, a, sizeof(position)) == 0); position = *b; return 1;
}
int EspNativeGameplayFrame_renderTurn(struct Render_s* render, uint8_t angle,
                                    EspNativeGameplayFrameStats* stats) {
    (void)render; (void)angle; ++renderCalls;
    memset(stats, 0, sizeof(*stats));
    return !(fault == RENDER_FAIL && renderCalls == 1U);
}
const EspNativeGameplayMonsterMovementView* EspNativeGameplayMonsterMovement_view(void) { return &movement; }
void EspNativeGameplayMonsterMovement_service(struct DoomRPG_s* d) {
    assert(d == &root); ++plannerCalls;
    if (fault == NO_MOVE) return;
    ++movement.plannedMoves;
    movement.lastSpriteIndex = before.spriteIndex;
    movement.lastSourceTile = before.tileIndex; movement.lastDestTile = after.tileIndex;
    EspNativeGameplayMonsterMovementPublish_capturePrepared(&before, &after);
}
int EspNativeGameplayMonsterTurn_postMoveGoal(struct DoomRPG_s* d, uint16_t sprite,
                                              uint16_t source, uint16_t dest) {
    assert(d == &root && sprite == before.spriteIndex && source == before.tileIndex && dest == after.tileIndex);
    ++postMoveCalls; return 1;
}
static void fixture(unsigned subtype, enum Fault failure) {
    fault = failure; renderCalls = postMoveCalls = plannerCalls = 0;
    memset(&monster, 0, sizeof(monster)); monster.spriteIndex = 17; monster.subtype = subtype; monster.alive = 1;
    monsters.records = &monster; monsters.count = 1;
    before = (EspNativeGameplayMonsterPositionRecord){17,99,224,224};
    after = (EspNativeGameplayMonsterPositionRecord){17,100,288,224};
    position = before; tile = 99; order = 4; topology.nextLinkOrder = 5; topology.stateFNV1a = tile;
    memset(&movement, 0, sizeof(movement));
    memset(&player, 0, sizeof(player)); player.active = 1;
    player.viewX = player.destX = 224; player.viewY = player.destY = 288;
    EspNativeGameplayMonsterMovementPublish_reset();
}
static void exact(const Random_t* a, const Random_t* b) { assert(memcmp(a,b,sizeof(*a)) == 0); }
static uint8_t legacyByte(Random_t* r) {
    if (r->nextRand + 1 >= RANDTABLESIZE) Legacy_DoomRPG_setRand(r);
    return r->randTable[r->nextRand++];
}
int main(void) {
    Random_t* live = EspNativeProcessRng_live();
    Random_t ref = {0};
    unsigned cases = 0, commits = 0, rollbacks = 0;
    const int offsets[] = {0, 124, 125, 126, 127};
    /* Legacy Entity_aiThink: melee no-z 1 draw; ranged close no-z 2;
       ranged close >=217 branch 3. Planner decision is injected, not tested. */
    for (unsigned mode = 0; mode < 3; ++mode) {
        unsigned calls = mode + 1;
        const char* trigger = mode == 2 ? "RANGED-AI" : "NO-IMMEDIATE-ATTACK";
        for (unsigned o = 0; o < sizeof(offsets)/sizeof(offsets[0]); ++o) {
            for (enum Fault f = OK; f <= POST_RENDER_FAIL; ++f) {
                fixture(mode == 0 ? 1 : 0, f);
                EspNativeRngReplayGuard_reset();
                DoomRPG_setRand(live); Legacy_DoomRPG_setRand(&ref); exact(live,&ref);
                live->nextRand = ref.nextRand = offsets[o];
                Random_t start = *live;
                uint8_t committed = 255;
                int canReplay = offsets[o] == 127 || offsets[o] + (int)calls < RANDTABLESIZE;
                int wantCommit = f == OK && canReplay;
                int status = EspNativeGameplayMonsterMovementProbe_serviceMember(&root, trigger, &committed);
                assert(status == (wantCommit || f == NO_MOVE));
                assert(committed == wantCommit && plannerCalls == 1);
                if (wantCommit) {
                    for (unsigned i = 0; i < calls; ++i) (void)legacyByte(&ref);
                    exact(live,&ref);
                    assert(memcmp(&position,&after,sizeof(position)) == 0);
                    assert(tile == 100 && order == 5 && topology.nextLinkOrder == 6 && topology.stateFNV1a == 100);
                    assert(EspNativeGameplayMonsterMovementPublish_isProjected(17));
                    assert(renderCalls == 1 && postMoveCalls == (mode != 2));
                    ++commits;
                } else {
                    exact(live,&start);
                    assert(memcmp(&position,&before,sizeof(position)) == 0);
                    assert(tile == 99 && order == 4 && topology.nextLinkOrder == 5 && topology.stateFNV1a == 99);
                    assert(!EspNativeGameplayMonsterMovementPublish_isProjected(17) && postMoveCalls == 0);
                    assert(renderCalls == ((canReplay && (f == RENDER_FAIL || f == POST_RENDER_FAIL)) ? 2U : 0U));
                    ++rollbacks;
                }
                /* Consume after the ordinary 1s lease, including persistent
                   reservations at 127. Then compare entire table and hidden seed. */
                tick += 4000;
                assert(DoomRPG_randNextByte(live) == legacyByte(&ref));
                exact(live,&ref);
                assert(Random_Get() == Legacy_Random_Get());
                ++cases;
            }
        }
    }
    assert(cases == 105 && commits == 12 && rollbacks == 93);
    printf("PASS GAME-AI-14 A1b real publication/probe/RNG: cases=%u commits=%u no-commit=%u\n",
           cases, commits, rollbacks);
    return 0;
}
