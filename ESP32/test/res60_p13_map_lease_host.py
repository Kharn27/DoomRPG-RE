#!/usr/bin/env python3
"""RES60-P13: exercise the real C map lease and rejection paths without ESP32 hardware."""
from pathlib import Path
import configparser
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
INI=configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
assert INI.read(ROOT/"ESP32/platformio.ini")
assert "-D DOOMRPG_RES60_P13_MAP_LEASE=1" in INI["env:esp32-cyd-res60"]["build_flags"]
assert INI["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted","env:esp32-cyd-bringup","env:esp32-cyd-trace"):
    assert "DOOMRPG_RES60_P13_MAP_LEASE" not in INI[env]["build_flags"]

config=(ROOT/"ESP32/include/platform_video_config.h").read_text()
world=(ROOT/"ESP32/src/esp_res60_world_probe.c").read_text()
video=(ROOT/"ESP32/src/platform_video.cpp").read_text()
main=(ROOT/"ESP32/src/main.cpp").read_text()
owner=ROOT/"ESP32/src/esp_res60_map_lease.c"
assert "RES60-P13 requires isolated P12/P7" in config
assert "EspRes60MapLease_acquire(mapName, &inv, inventoryHit," in world
assert "EspRes60MapLease_isReady()" in world and "EspRes60MapLease_reset()" in world
assert "EspRes60MapLease_isReady()" in video and "mapRuntime=RETAINED" in video
assert "EspRes60MapLease_reset()" in main
assert "EspMapRuntime_loadPackEntry" in owner.read_text()
assert "EspAssetPack_findEntry" in owner.read_text()
assert "hashArena(view->arena, view->arenaBytes)" in owner.read_text()
assert "EspRes60InventoryCache_verify" in owner.read_text()

CC=["cc","-O2","-std=c11","-Wall","-Wextra","-Werror",
    "-I",str(ROOT/"ESP32/include")]
FLAGS=["-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
       "-DDOOMRPG_RES60_P2_LAYOUT=1",
       "-DDOOMRPG_RES60_P3_WORLD=1",
       "-DDOOMRPG_RES60_P5_TITLE=1",
       "-DDOOMRPG_RES60_P6_MENU=1",
       "-DDOOMRPG_RES60_P7_NAV=1",
       "-DDOOMRPG_RES60_P8_TEXEL=1",
       "-DDOOMRPG_RES60_P9_WEDGE=1",
       "-DDOOMRPG_RES60_P10_WALL_MATERIAL=1",
       "-DDOOMRPG_RES60_P11_PROFILE=1",
       "-DDOOMRPG_RES60_P12_INVENTORY_CACHE=1",
       "-DDOOMRPG_RES60_P13_MAP_LEASE=1"]
HARNESS=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_res60_map_lease.h"

static uint8_t arena[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
static EspMapRuntimeView view;
static int loaded, packOpen, loads, resets, packOpens, lookups, corruptCatalog;
static uint32_t packCrc=0x623f34e4U, packBytes=21823U, packFlags=0U;
static uint32_t hash(const uint8_t* data, uint32_t n) {
    uint32_t h=2166136261U;
    for(uint32_t i=0;i<n;++i)h=(h^data[i])*16777619U;
    return h;
}
void EspMapRuntime_reset(void) {
    loaded=0;
    memset(&view,0,sizeof(view));
    ++resets;
}
int EspMapRuntime_loadPackEntry(const char* name,const EspBspInventory* inv) {
    assert(strcmp(name,"/intro.bsp")==0 && inv!=NULL && !packOpen);
    ++loads; loaded=1;
    view.arena=arena;view.arenaBytes=sizeof(arena);
    view.arenaFNV1a=hash(arena,sizeof(arena));
    view.sourceBytes=inv->sourceBytes;
    view.sourceCrc32=inv->crc32;
    view.nodeCount=inv->nodes;
    view.lineCount=inv->lines;
    return 1;
}
int EspMapRuntime_isLoaded(void) { return loaded; }
const EspMapRuntimeView* EspMapRuntime_view(void) {return loaded?&view:NULL;}
int EspAssetPack_open(const char* name) {
    assert(strcmp(name,ESP_ASSET_PACK_DEFAULT_PATH)==0 && !packOpen);
    ++packOpens;packOpen=1;return 1;
}
void EspAssetPack_close(void) { assert(packOpen);packOpen=0; }
int EspAssetPack_isOpen(void) {return packOpen;}
int EspAssetPack_findEntry(const char* name,EspAssetPackEntry* e) {
    assert(packOpen && strcmp(name,"/intro.bsp")==0 && e);
    ++lookups;
    memset(e,0,sizeof(*e));
    e->size=packBytes;e->crc32=packCrc;e->flags=packFlags;
    return 1;
}
int EspRes60InventoryCache_verify(const char* name,
                                 const EspBspInventory* inv,uint32_t fnv) {
    return strcmp(name,"/intro.bsp")==0 && inv!=NULL && !corruptCatalog &&
           fnv==hash(arena,sizeof(arena));
}
int main(void) {
    EspBspInventory inv;
    uint8_t hit=99U;
    memset(&inv,0,sizeof(inv));
    inv.sourceBytes=21823U;inv.crc32=0x623f34e4U;
    inv.expectedCrc32=inv.crc32;
    inv.plan.persistentBytes=sizeof(arena);
    inv.nodes=223U;inv.lines=480U;

    assert(!EspRes60MapLease_isReady());
    assert(!EspRes60MapLease_acquire("/wrong.bsp",&inv,0U,&hit));
    assert(!EspRes60MapLease_acquire("/intro.bsp",NULL,0U,&hit));
    assert(!EspRes60MapLease_acquire("/intro.bsp",&inv,0U,NULL));
    assert(!EspRes60MapLease_acquire("/intro.bsp",&inv,2U,&hit));
    assert(EspRes60MapLease_acquire("/intro.bsp",&inv,0U,&hit));
    assert(hit==0U && loads==1 && packOpens==0);
    assert(EspRes60MapLease_isReady());
    for(int i=0;i<12;++i) {
        assert(EspRes60MapLease_acquire("/intro.bsp",&inv,1U,&hit));
        assert(hit==1U && loads==1 && packOpens==i+1 && lookups==i+1);
        assert(EspRes60MapLease_isReady() && !packOpen);
    }
    /* A second MISS is forbidden during a live lease. */
    assert(!EspRes60MapLease_acquire("/intro.bsp",&inv,0U,&hit));
    assert(EspRes60MapLease_isReady());

    /* Reject altered retained payload even if metadata/FNV fields are stale. */
    arena[0]^=1U;
    assert(!EspRes60MapLease_acquire("/intro.bsp",&inv,1U,&hit));
    assert(!loaded && !packOpen && !EspRes60MapLease_isReady());
    arena[0]^=1U;
    assert(EspRes60MapLease_acquire("/intro.bsp",&inv,0U,&hit));
    assert(loads==2);
    packCrc++;
    assert(!EspRes60MapLease_acquire("/intro.bsp",&inv,1U,&hit));
    assert(!loaded && !packOpen);
    packCrc=inv.crc32;
    assert(EspRes60MapLease_acquire("/intro.bsp",&inv,0U,&hit));
    packBytes++;
    assert(!EspRes60MapLease_acquire("/intro.bsp",&inv,1U,&hit));
    assert(!loaded && !packOpen);
    packBytes=inv.sourceBytes;
    assert(EspRes60MapLease_acquire("/intro.bsp",&inv,0U,&hit));
    packFlags=ESP_ASSET_PACK_FLAG_DIRECTORY;
    assert(!EspRes60MapLease_acquire("/intro.bsp",&inv,1U,&hit));
    assert(!loaded && !packOpen);
    packFlags=0U;

    assert(EspRes60MapLease_acquire("/intro.bsp",&inv,0U,&hit));
    corruptCatalog=1;
    assert(!EspRes60MapLease_acquire("/intro.bsp",&inv,1U,&hit));
    assert(!loaded && !packOpen);
    corruptCatalog=0;

    assert(EspRes60MapLease_acquire("/intro.bsp",&inv,0U,&hit));
    inv.expectedCrc32++;
    assert(!EspRes60MapLease_acquire("/intro.bsp",&inv,1U,&hit));
    inv.expectedCrc32=inv.crc32;
    EspRes60MapLease_reset(); /* successful MENU/START boundary */
    assert(!loaded && !EspRes60MapLease_isReady());
    assert(!EspRes60MapLease_acquire("/intro.bsp",&inv,1U,&hit));
    assert(!loaded);
    assert(EspRes60MapLease_acquire("/intro.bsp",&inv,0U,&hit));
    assert(loads==7 && !packOpen);
    EspRes60MapLease_reset();
    assert(!loaded && !packOpen);
    /* P31: warm validation transfers the exact same reader to planes;
     * no second open or lookup, no foreign-owner closure on rejection. */
    assert(EspRes60MapLease_acquireForPlanes("/intro.bsp",&inv,0U,&hit));
    assert(hit==0U && !packOpen && EspRes60MapLease_isReady());
    int before=packOpens, beforeLookup=lookups;
    assert(EspRes60MapLease_acquireForPlanes("/intro.bsp",&inv,1U,&hit));
    assert(hit==1U && packOpen && packOpens==before+1 &&
           lookups==beforeLookup+1 && !EspRes60MapLease_isReady());
    assert(!EspRes60MapLease_acquireForPlanes("/intro.bsp",&inv,1U,&hit));
    assert(packOpen && loaded && packOpens==before+1);
    EspAssetPack_close(); /* same caller owns plane close */
    assert(EspRes60MapLease_isReady());
    packCrc++;
    assert(!EspRes60MapLease_acquireForPlanes("/intro.bsp",&inv,1U,&hit));
    assert(!loaded && !packOpen && hit==0U);
    packCrc=inv.crc32;
    assert(EspRes60MapLease_acquireForPlanes("/intro.bsp",&inv,0U,&hit));
    arena[0]^=1U; before=packOpens;
    assert(!EspRes60MapLease_acquireForPlanes("/intro.bsp",&inv,1U,&hit));
    assert(!loaded && !packOpen && packOpens==before);
    arena[0]^=1U;
    puts("RES60-P31 shared PAK owner PASS");
    puts("RES60-P13 C owner PASS: one real arena, 12 metadata HIT, mutations, reset");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p13-") as folder:
    folder=Path(folder)
    src=folder/"test.c";src.write_text(HARNESS)
    exe=folder/"p13"
    subprocess.run(CC+FLAGS+[str(owner),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    obj=folder/"prod.o"
    subprocess.run(CC+["-DDOOMRPG_RES60_P13_MAP_LEASE=0","-c",str(owner),"-o",str(obj)],check=True)
    assert "EspRes60MapLease_" not in subprocess.check_output(["nm","-g",str(obj)],text=True)
    bad=subprocess.run(CC+["-DDOOMRPG_RES60_P13_MAP_LEASE=1",
                           "-fsyntax-only",str(owner)],capture_output=True,text=True)
    assert bad.returncode!=0 and "requires isolated P12/P7" in bad.stderr
print("RES60-P13 HOST PASS: actual C session owner, source guards, production excludes owner")
