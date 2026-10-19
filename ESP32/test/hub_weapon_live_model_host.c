/* Execute real live-HUD derivation and shared ammo table on host. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_hud.h"
#include "esp_native_gameplay_weapon_control.h"
#include "esp_map_catalog.h"
int EspMapCatalog_isValidId(uint8_t id) { return id >= 1U && id <= 13U; }
int main(void) {
    EspNativeGameplayHudState retained, before, live;
    EspNativeGameplayPlayerState player, original;
    uint8_t ammoType;
    memset(&retained,0,sizeof(retained));
    memset(&player,0,sizeof(player));
    retained.active = retained.painted = 1U;
    retained.model.targetMapId = retained.model.gameplayLoadMapId = 1U;
    retained.model.health = retained.model.maxHealth = 30U;
    retained.model.maxArmor = 20U;
    retained.model.weapon = 3U;        /* stale initial shotgun */
    retained.model.ammoType = 2U;
    retained.model.ammo = 22U;
    retained.model.weaponsPresent = 1U;
    retained.model.destAngle = 64U;    /* stale initial camera */
    before=retained;
    player.active=1U;
    player.param1 = 39U | (39U<<8) | (26U<<16) | (26U<<24);
    player.weapon=2U;
    player.weapons=0xfU;
    player.ammo[1]=14U;
    player.ammo[2]=22U;
    original=player;
    assert(EspNativeGameplayHud_snapshotLive(&retained,&player,128U,&live));
    assert(live.model.weapon==2U && live.model.ammoType==1U && live.model.ammo==14U);
    assert(live.model.health==39U && live.model.maxHealth==39U);
    assert(live.model.armor==26U && live.model.maxArmor==26U);
    assert(live.model.destAngle==128U);
    assert(memcmp(&retained,&before,sizeof(retained))==0);
    assert(memcmp(&player,&original,sizeof(player))==0);
    player.weapon=3U;
    assert(EspNativeGameplayHud_snapshotLive(&retained,&player,128U,&live));
    assert(live.model.weapon==3U && live.model.ammoType==2U && live.model.ammo==22U);
    assert(EspNativeGameplayHud_snapshotLive(&retained,&player,128U,&live));
    assert(live.model.weapon==3U && live.model.ammo==22U);
    assert(memcmp(&retained,&before,sizeof(retained))==0);
    player.weapon=4U;
    assert(!EspNativeGameplayHud_snapshotLive(&retained,&player,128U,&live));
    assert(live.active==0U);
    player.weapon=3U;
    assert(!EspNativeGameplayHud_snapshotLive(&retained,&player,42U,&live));
    assert(!EspNativeGameplayHud_snapshotLive(&retained,NULL,128U,&live));
    assert(EspNativeGameplayWeaponControl_ammoType(2U,&ammoType) && ammoType==1U);
    assert(!EspNativeGameplayWeaponControl_ammoType(12U,&ammoType));
    puts("PASS HUD live Pistol->Shotgun ammo14->22 angle64->128, no stale model mutation");
    return 0;
}
