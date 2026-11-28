#!/usr/bin/env python3
"""RES60-P12: compile the real compact inventory snapshot C owner with mocks.

The new cache may skip only the full BSP inventory scan on warm turns. Every
turn still goes through EspMapRuntime_loadPackEntry (native PAK CRC/size check,
compact arena rebuilt) and must verify the rebuilt arena's FNV before raster.
"""
from pathlib import Path
import configparser
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
INI = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
assert INI.read(ROOT/"ESP32/platformio.ini")
assert "-D DOOMRPG_RES60_P12_INVENTORY_CACHE=1" in INI["env:esp32-cyd-res60"]["build_flags"]
assert INI["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted","env:esp32-cyd-bringup",
            "env:esp32-cyd-trace"):
    assert "DOOMRPG_RES60_P12_INVENTORY_CACHE" not in INI[env]["build_flags"]
config = (ROOT/"ESP32/include/platform_video_config.h").read_text()
assert "RES60-P12 requires isolated P11/P10" in config
probe = (ROOT/"ESP32/src/esp_res60_world_probe.c").read_text()
main = (ROOT/"ESP32/src/main.cpp").read_text()
video = (ROOT/"ESP32/src/platform_video.cpp").read_text()
runtime = (ROOT/"ESP32/src/esp_map_runtime.c").read_text()
assert "EspRes60InventoryCache_read(mapName, &inv, &inventoryHit)" in probe
assert "EspMapRuntime_loadPackEntry(mapName, &inv)" in probe
assert "EspRes60InventoryCache_verify(mapName, &inv," in probe
assert "EspRes60InventoryCache_commit(mapName, &inv," in probe
assert "EspMapRuntime_reset();" in probe
assert "EspAssetPack_isOpen()" in probe
assert "entry.crc32 != inventory->crc32" in runtime
assert "entry.size != inventory->sourceBytes" in runtime
assert main.count("EspRes60InventoryCache_reset();") >= 2
assert "[RES60P12] INVENTORY cache=%s" in video

flags=[
  "-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
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
]
cc=["cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-I",str(ROOT/"ESP32/include")]
owner=ROOT/"ESP32/src/esp_res60_inventory_cache.c"
harness=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_res60_inventory_cache.h"
static int inventoryReads;
static uint32_t expectedCrc=0x623f34e4U;
static int malformed;
int EspBspReader_inventoryPackEntry(const char* resourceName,
                                    EspBspInventory* inventory) {
    assert(strcmp(resourceName, "/intro.bsp") == 0);
    ++inventoryReads;
    memset(inventory, 0, sizeof(*inventory));
    inventory->sourceBytes=21823U;
    inventory->crc32=expectedCrc;
    inventory->expectedCrc32=malformed ? 0xdeadbeefU : expectedCrc;
    inventory->plan.persistentBytes=14095U;
    inventory->nodes=223U;
    inventory->lines=480U;
    inventory->spawnIndex=904U;
    inventory->spawnDirection=64U;
    return 1;
}
int main(void) {
    EspBspInventory a,b;
    uint8_t hit=99U;
    assert(!EspRes60InventoryCache_read(NULL,&a,&hit) && hit==0U);
    assert(!EspRes60InventoryCache_read("/not-intro.bsp",&a,&hit));
    assert(!EspRes60InventoryCache_read("/intro.bsp",NULL,&hit));
    assert(!EspRes60InventoryCache_read("/intro.bsp",&a,NULL));
    assert(EspRes60InventoryCache_read("/intro.bsp",&a,&hit));
    assert(inventoryReads==1 && hit==0U);
    assert(!EspRes60InventoryCache_commit("/intro.bsp",&a,0U));
    assert(!EspRes60InventoryCache_commit("/wrong.bsp",&a,0xc3882516U));
    assert(EspRes60InventoryCache_commit("/intro.bsp",&a,0xc3882516U));
    for(unsigned turn=0U;turn<12U;++turn) {
        memset(&b,0xa5,sizeof(b));
        assert(EspRes60InventoryCache_read("/intro.bsp",&b,&hit));
        assert(hit==1U && inventoryReads==1);
        assert(memcmp(&a,&b,sizeof(a))==0);
        assert(EspRes60InventoryCache_verify("/intro.bsp",&b,0xc3882516U));
        assert(EspRes60InventoryCache_commit("/intro.bsp",&b,0xc3882516U));
    }
    assert(!EspRes60InventoryCache_verify("/intro.bsp",&b,0x33333333U));
    assert(!EspRes60InventoryCache_commit("/intro.bsp",&b,0x33333333U));
    b.sourceBytes++;
    assert(!EspRes60InventoryCache_verify("/intro.bsp",&b,0xc3882516U));
    EspRes60InventoryCache_reset(); /* successful MENU or fail-closed */
    expectedCrc=0x22334455U; /* new source after user changes PAK */
    assert(EspRes60InventoryCache_read("/intro.bsp",&b,&hit));
    assert(hit==0U && inventoryReads==2 && b.crc32==expectedCrc);
    assert(EspRes60InventoryCache_commit("/intro.bsp",&b,0xaabbccddU));
    /* If PAK entry changes DURING session, the production map loader must
       reject CRC/size mismatch before verify; our mismatch guard rejects FNV. */
    assert(!EspRes60InventoryCache_verify("/intro.bsp",&b,0xc3882516U));
    EspRes60InventoryCache_reset();
    malformed=1;
    assert(EspRes60InventoryCache_read("/intro.bsp",&b,&hit));
    assert(hit==0U && inventoryReads==3);
    assert(!EspRes60InventoryCache_commit("/intro.bsp",&b,0xaabbccddU));
    b.expectedCrc32=b.crc32;
    b.lines=513U;
    assert(!EspRes60InventoryCache_commit("/intro.bsp",&b,0xaabbccddU));
    b.lines=480U;
    assert(EspRes60InventoryCache_commit("/intro.bsp",&b,0xaabbccddU));
    EspRes60InventoryCache_reset();
    assert(EspRes60InventoryCache_read("/intro.bsp",&a,&hit));
    assert(hit==0U && inventoryReads==4);
    puts("RES60-P12 native inventory cache PASS: cold/warm/reset, CRC, arena FNV, bounds");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p12-") as d:
    d=Path(d)
    source=d/"test.c";source.write_text(harness)
    exe=d/"test"
    subprocess.run(cc+flags+[str(owner),str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    # Compiling production mode must emit no P12 owner, no cache or timer.
    obj=d/"prod.o"
    subprocess.run(cc+["-DDOOMRPG_RES60_P12_INVENTORY_CACHE=0",
                        "-c",str(owner),"-o",str(obj)],check=True)
    symbols=subprocess.check_output(["nm","-g",str(obj)],text=True)
    assert "EspRes60InventoryCache_" not in symbols
    for p12,p11 in ((1,0),(1,1)):
        invalid=subprocess.run(cc+[
            "-DDOOMRPG_RES60_P12_INVENTORY_CACHE=%d"%p12,
            "-DDOOMRPG_RES60_P11_PROFILE=%d"%p11,
            "-fsyntax-only",str(owner)],capture_output=True,text=True)
        assert invalid.returncode!=0
        assert "requires isolated" in invalid.stderr
print("RES60-P12 HOST PASS: actual C owner, warm no BSP inventory, fail-closed, session reset, prod isolation")

# P35: execute the real reader over an independent synthetic BSP with
# records and strings crossing both window sizes. The entire inventory
# except the intentionally changed readCalls must match byte for byte.
import re
import struct
import zlib
fixture=bytearray(33)
fixture[:8]=b"Fixture!"
fixture[22]=145
fixture[23]=112
fixture[27]=1
fixture[30]=64
# nodes, lines, sprites, events, bytecodes, strings; then block/plane maps.
for count, stride in ((223,10),(480,10),(344,5),(93,4),(265,9)):
    fixture+=struct.pack("<H",count)+bytes(count*stride)
fixture+=struct.pack("<H",3)
for n in (2051,313,4095):
    fixture+=struct.pack("<H",n)+bytes((i*37)&255 for i in range(n))
fixture+=bytes(256)+bytes(i%12 for i in range(2048))
real=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_bsp_reader.h"
#include "esp_asset_pack.h"
#include "fixture.h"
static unsigned reads, failAt, opened, corrupt;
static uint8_t loan[2050];
int EspAssetPack_isOpen(void){return opened;}
int EspAssetPack_open(const char* p){assert(!opened && !strcmp(p,ESP_ASSET_PACK_DEFAULT_PATH));opened=1;return 1;}
void EspAssetPack_close(void){assert(opened);opened=0;}
int EspAssetPack_findEntry(const char* name,EspAssetPackEntry* e){
 assert(opened && !strcmp(name,"/intro.bsp"));
 memset(e,0,sizeof(*e));e->size=sizeof(fixture);e->crc32=FIXTURE_CRC^(corrupt?1U:0U);return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* e,uint32_t off,void* dest,size_t n){
 assert(opened && off<=e->size && n<=e->size-off);
 if(++reads==failAt)return 0;
 memcpy(dest,fixture+off,n);return 1;
}
int main(void){
 EspBspInventory a,b;
 assert(EspBspReader_inventoryPackEntry("/intro.bsp",&a));
 assert(!opened && a.readCalls==reads && a.crc32==FIXTURE_CRC);
 unsigned direct=reads;
 reads=0;memset(loan,0xa5,sizeof(loan));
 assert(!EspBspReader_inventoryPackEntryWithScratch("/intro.bsp",&b,loan+1,2047));
 assert(!opened && !reads);
 assert(EspBspReader_inventoryPackEntryWithScratch("/intro.bsp",&b,loan+1,2048));
 unsigned wide=reads;
 assert(!opened && b.readCalls==wide && wide==(sizeof(fixture)+2047)/2048 && wide<direct);
 assert(loan[0]==0xa5 && loan[2049]==0xa5);
 b.readCalls=a.readCalls;
 assert(!memcmp(&a,&b,sizeof(a)));
 for(unsigned i=1;i<=wide;i++){
   reads=0;failAt=i;
   assert(!EspBspReader_inventoryPackEntryWithScratch("/intro.bsp",&b,loan+1,2048));
   assert(!opened);
 }
 failAt=0;corrupt=1;
 assert(!EspBspReader_inventoryPackEntryWithScratch("/intro.bsp",&b,loan+1,2048));
 assert(!opened);
 corrupt=0;opened=1;
 assert(EspBspReader_inventoryPackEntryWithScratch("/intro.bsp",&b,loan+1,2048));
 assert(opened);EspAssetPack_close();
 printf("P35 real BSP parity PASS reads=%u/%u\n",direct,wide);
 return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p35-") as d:
    d=Path(d);src=d/"test.c";src.write_text(real)
    (d/"fixture.h").write_text("static const uint8_t fixture[]={"+",".join(map(str,fixture))+"};\n#define FIXTURE_CRC "+str(zlib.crc32(fixture))+"U\n")
    fullflags=["-D"+f for f in re.findall(r"-D\s+(DOOMRPG_RES60_[A-Z0-9_]+=1)",INI["env:esp32-cyd-res60"]["build_flags"])]
    exe=d/"test"
    subprocess.run(cc+fullflags+[str(ROOT/"ESP32/src/esp_bsp_reader.c"),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
