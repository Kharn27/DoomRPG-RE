/* Real production dog terminal planner + V11 dynamic-world sprite owner.
 * Only the immutable BSP map/catalog providers are mocked. No desktop
 * GSprite graph, ZIP, renderer workspaces or heap allocation. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_dog_familiar.h"
#include "esp_native_gameplay_monster_drop.h"
#include "esp_native_gameplay_monster_position.h"
#include "esp_map_runtime.h"

static EspMapRuntimeView world;
static uint32_t catalogCalls;
const EspMapRuntimeView* EspMapRuntime_view(void) { return &world; }
int EspEntityDefTypeCatalog_getParm(uint16_t tile,int32_t* parm) {
    (void)tile;(void)parm; ++catalogCalls;return 0;
}
int EspEntityDefTypeCatalog_findTileIndex(
    uint8_t type,uint8_t subtype,uint16_t* tile) {
    (void)type;(void)subtype;(void)tile;++catalogCalls;return 0;
}
int EspNativeGameplayMonsterPosition_ensure(void){return 0;}
const EspNativeGameplayMonsterPositionRecord*
EspNativeGameplayMonsterPosition_find(uint16_t sprite){
    (void)sprite;return NULL;
}

static void assertSnapshotExact(
    const EspNativeGameplayMonsterDropSnapshot* before) {
    EspNativeGameplayMonsterDropSnapshot check;
    assert(EspNativeGameplayMonsterDrop_snapshot(&check));
    assert(memcmp(&check,before,sizeof check)==0);
    assert(EspNativeGameplayMonsterDrop_fingerprint()==before->stateFNV1a);
}

int main(void) {
    EspNativeGameplayPlayerState player,playerBefore;
    EspNativeDogFamiliarDeathPlan death;
    EspNativeGameplayMonsterDropSpawnPlan corpse,stale;
    EspNativeGameplayMonsterDropSnapshot old,live,checkpoint;
    const EspNativeGameplayMonsterDropView* view;
    uint32_t rngBefore=0x51e6a19bU,rng=rngBefore;
    uint16_t positionX=96U,positionY=160U;
    unsigned k,i;
    memset(&world,0,sizeof world);
    world.arenaFNV1a=0xabc123efU;
    EspNativeGameplayMonsterDrop_reset();
    assert(EspNativeGameplayMonsterDrop_ensure());
    assert(EspNativeGameplayMonsterDrop_snapshot(&old));
    assert(old.spawnSerial==0U&&old.nextSlot==0U);

    for(k=9U;k<=11U;++k) {
        memset(&player,0,sizeof player);
        player.active=1U;player.weapon=(uint8_t)k;
        player.weapons=(uint16_t)((1U<<k)|(1U<<0)|(1U<<2));
        player.ammo[5]=10U;
        player.param1=0x140a1e1eU;
        playerBefore=player;
        assert(EspNativeDogFamiliar_planDeath(&player,10U,&death));
        assert(death.corpseGSpriteKind==20U+(k-9U));
        assert(death.playerAfter.ammo[5]==0U&&death.nextWeapon==0U);
        assert(EspNativeGameplayMonsterDrop_snapshot(&old));
        assert(EspNativeGameplayMonsterDrop_prepareDogCorpse(
            death.corpseGSpriteKind,positionX,positionY,&corpse));
        assert(corpse.slot==old.nextSlot);
        assert(corpse.record.tileIndex==2U*32U+1U);
        assert(corpse.record.type==ESP_NATIVE_GAMEPLAY_DOG_CORPSE_TYPE);
        assert(corpse.record.subtype==death.corpseGSpriteKind);
        assert(corpse.record.defTile==death.corpseGSpriteKind);
        assert(corpse.record.taken==0U&&corpse.record.active==1U);
        assertSnapshotExact(&old);
        stale=corpse;
        player=death.playerAfter;
        assert(EspNativeGameplayMonsterDrop_commit(&corpse));
        assert(!EspNativeGameplayMonsterDrop_commit(&stale));
        assert(EspNativeGameplayMonsterDrop_snapshot(&live));
        assert(live.spawnSerial==old.spawnSerial+1U);
        assert(live.records[corpse.slot].type==
               ESP_NATIVE_GAMEPLAY_DOG_CORPSE_TYPE);
        assert(live.records[corpse.slot].defTile==20U+(k-9U));
        assert(!EspNativeGameplayMonsterDrop_setTaken(corpse.slot,1));
        assert(EspNativeGameplayMonsterDrop_snapshot(&checkpoint));
        assert(memcmp(&checkpoint,&live,sizeof live)==0);
        assert(EspNativeGameplayMonsterDrop_restore(&checkpoint));
        assertSnapshotExact(&checkpoint);
        /* Simulate a failed renderer after player + world commit: the
         * exact V11 world image and original player/RNG must return. */
        assert(EspNativeGameplayMonsterDrop_restore(&old));
        player=playerBefore;
        rng=rngBefore;
        assertSnapshotExact(&old);
        assert(memcmp(&player,&playerBefore,sizeof player)==0);
        assert(rng==rngBefore);
        /* Commit again as if the renderer succeeded and persist it. */
        assert(EspNativeGameplayMonsterDrop_commit(&corpse));
        assert(EspNativeGameplayMonsterDrop_snapshot(&checkpoint));
        assert(EspNativeGameplayMonsterDrop_restore(&checkpoint));
        assertSnapshotExact(&checkpoint);
        ++positionX; /* invalid for next prepare until reset below */
        positionX=96U;
    }
    view=EspNativeGameplayMonsterDrop_view();
    assert(view!=NULL&&view->visibleCount==3U);
    assert(catalogCalls==0U); /* corpse is not an EntityDef/pickup */
    assert(!EspNativeGameplayMonsterDrop_prepareDogCorpse(19U,96U,160U,&corpse));
    assert(!EspNativeGameplayMonsterDrop_prepareDogCorpse(23U,96U,160U,&corpse));
    assert(!EspNativeGameplayMonsterDrop_prepareDogCorpse(20U,97U,160U,&corpse));
    assert(!EspNativeGameplayMonsterDrop_prepareDogCorpse(20U,96U,161U,&corpse));
    assert(!EspNativeGameplayMonsterDrop_prepareDogCorpse(20U,0U,160U,&corpse));
    assert(!EspNativeGameplayMonsterDrop_prepareDogCorpse(20U,96U,0U,&corpse));
    assert(!EspNativeGameplayMonsterDrop_prepareDogCorpse(20U,96U,160U,NULL));

    /* The source Game_gsprite_alloc() replaces oldest allocations once
     * its bounded custom sprite pool is exhausted. Our existing V11
     * eight-slot dynamic pool does likewise without allocating. */
    for(i=0U;i<18U;++i) {
        assert(EspNativeGameplayMonsterDrop_prepareDogCorpse(
            (uint8_t)(20U+i%3U),96U,160U,&corpse));
        assert(EspNativeGameplayMonsterDrop_commit(&corpse));
        assert(EspNativeGameplayMonsterDrop_snapshot(&checkpoint));
        assert(checkpoint.spawnSerial==4U+i);
        assert(checkpoint.nextSlot==(uint8_t)((4U+i)%8U));
        assert(EspNativeGameplayMonsterDrop_restore(&checkpoint));
        assertSnapshotExact(&checkpoint);
    }
    view=EspNativeGameplayMonsterDrop_view();
    assert(view&&view->visibleCount==8U);
    for(i=0U;i<ESP_NATIVE_GAMEPLAY_MONSTER_DROP_SLOTS;++i) {
        assert(view->records[i].active==1U);
        assert(view->records[i].type==ESP_NATIVE_GAMEPLAY_DOG_CORPSE_TYPE);
        assert(view->records[i].subtype>=20U&&view->records[i].subtype<=22U);
        assert(!EspNativeGameplayMonsterDrop_setTaken((uint8_t)i,1));
    }
    assert(EspNativeGameplayMonsterDrop_snapshot(&old));
    world.arenaFNV1a=0x0badcafeU;
    assert(!EspNativeGameplayMonsterDrop_restore(&old));
    assert(EspNativeGameplayMonsterDrop_ensure());
    assert(EspNativeGameplayMonsterDrop_snapshot(&live));
    assert(live.spawnSerial==0U && live.nextSlot==0U);
    assert(catalogCalls==0U);
    puts("PASS dog death: real 3 gSprite kinds frame2, V11 save/restore, 8-slot rotation, invalid rejects, no pickup, player/world/RNG exact rollback");
    return 0;
}
