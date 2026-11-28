#!/usr/bin/env python3
"""RES60-P10: real P8/P10 C11 integration, visible-source preference and fallback."""
from pathlib import Path
import subprocess
import tempfile
import configparser
import re

ROOT = Path(__file__).resolve().parents[2]
t = (ROOT / "ESP32/src/esp_res60_texture_probe.c").read_text()
p = (ROOT / "ESP32/src/platform_video.cpp").read_text()
assert "EspRes60WallMaterial_visibleTextureAt(" in t
assert "selectedFromVisible=1U" in t
assert "if(!found) {" in t
assert "selected=%s visibleColumns=%u" in p

harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_native_graphics_catalog.h"
#include "esp_res60_texture_probe.h"
#include "esp_res60_wall_material.h"

static uint16_t framebuffer[192U*144U+2U];
static EspMapRuntimeView runtime;
static EspNativeGraphicsCatalogView catalog;
static EspNativeGraphicsCatalogRecord records[2];
static int active=1, pack=0, catalogReady=0;
static unsigned opens=0, closes=0, reads=0;
static int visibleOddNibble=0, badHeader=0, outsideRange=0;
#if DOOMRPG_RES60_P39_WALL_STREAM
static int twoTiles=0, failTile=0, changedEntry=0;
#endif

int EspMapRuntime_isLoaded(void) {return active;}
const EspMapRuntimeView* EspMapRuntime_view(void) {return active?&runtime:NULL;}
int EspMapRuntime_getLine(uint32_t index, EspMapLine* out) {
    assert(active);
    if(index>=runtime.lineCount || !out)return 0;
    memset(out,0,sizeof(*out));
    out->texture=index==0?87:7; /* first raw line is NOT visible */
    return 1;
}
int EspNativeGraphicsCatalog_isReady(void) {return catalogReady;}
EspNativeGraphicsCatalogStatus EspNativeGraphicsCatalog_buildFromRuntime(void) {
    assert(active && !catalogReady && !pack);
    catalogReady=1;
    records[1].sourceOffset=visibleOddNibble?1U:(outsideRange?4096U:0U);
#if DOOMRPG_RES60_P39_WALL_STREAM
    records[0].sourceOffset=twoTiles?4096U:0U;
#endif
    catalog.textures=records;
    catalog.textureCount=2;
    catalog.stateFNV1a=0x11223344U;
    return ESP_NATIVE_GRAPHICS_CATALOG_OK;
}
#if DOOMRPG_RES60_P40_FULL_WALLS && DOOMRPG_RES60_P34_CATALOG_WINDOWS
EspNativeGraphicsCatalogStatus EspNativeGraphicsCatalog_buildFromRuntimeWithScratch(
    void* scratch,size_t bytes) {
    /* Mirror the real catalog ABI: exactly two 1024B windows, not the
     * entire 6144B owner loan. Reject rather than silently truncate. */
    if(!scratch || bytes!=2048U)return ESP_NATIVE_GRAPHICS_CATALOG_INVALID;
    memset(scratch,0x71,bytes);
    return EspNativeGraphicsCatalog_buildFromRuntime();
}
#endif
#if DOOMRPG_RES60_P40_FULL_WALLS && DOOMRPG_RES60_P11_PROFILE
uint32_t EspRes60Profile_nowUs(void) {
    static uint32_t tick=UINT32_MAX-25U;
    tick+=7U;return tick;
}
#endif
const EspNativeGraphicsCatalogView* EspNativeGraphicsCatalog_view(void) {
    return catalogReady?&catalog:NULL;
}
const EspNativeGraphicsCatalogRecord* EspNativeGraphicsCatalog_findTexture(uint16_t id) {
    assert(catalogReady);
    for(unsigned i=0;i<2;++i)if(records[i].resourceId==id)return &records[i];
    return NULL;
}
void EspNativeGraphicsCatalog_reset(void) {catalogReady=0;}
int EspAssetPack_isOpen(void) {return pack;}
int EspAssetPack_open(const char* path) {
    assert(!pack && !strcmp(path,ESP_ASSET_PACK_DEFAULT_PATH));
    pack=1;++opens;return 1;
}
void EspAssetPack_close(void) {assert(pack);pack=0;++closes;}
int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* entry) {
    assert(pack && !strcmp(name,"wtexels.bin") && entry);
    memset(entry,0,sizeof(*entry));entry->size=2052;
#if DOOMRPG_RES60_P39_WALL_STREAM
    if(twoTiles)entry->size=4100;
    if(changedEntry)entry->crc32=1;
#endif
    return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* entry, uint32_t off,
                           void* dest, size_t size) {
    assert(pack && entry && dest);
    ++reads;
    if(off==0U && size==4U) {
        uint8_t header[4]={0U,badHeader?7U:8U,0U,0U};
#if DOOMRPG_RES60_P39_WALL_STREAM
        if(twoTiles)header[1]=16U;
#endif
        memcpy(dest,header,sizeof(header));return 1;
    }
#if DOOMRPG_RES60_P39_WALL_STREAM
    if(twoTiles && off==2052U && size==2048U) {
        if(failTile)return 0;
        memset(dest,0x11,size);return 1;
    }
#endif
    assert(off==4U && size==2048U);
    memset(dest,0x10,size);return 1;
}
static void plan(uint16_t texture) {
    EspRes60WallMaterial_reset();
    for(uint32_t x=0;x<192U;++x)
       assert(EspRes60WallMaterial_record(x,texture,0,80,(uint8_t)(x%64U)));
    EspRes60WallMaterial_publish();
}
#if DOOMRPG_RES60_P40_FULL_WALLS
int main(void) {
    EspRes60TextureProbeReport result;
    EspRes60WallCoverage demand;
    uint8_t loan[6146];
    memset(loan,0xa5,sizeof(loan));
    runtime.lineCount=2;
    runtime.sourceCrc32=0x623f34e4U;
    runtime.arenaFNV1a=0xc3882516U;
    runtime.sourceBytes=21823U;
    records[0].resourceId=87;records[1].resourceId=7;
    records[0].paletteRgb565[1]=0xf800U;
    records[1].paletteRgb565[0]=0x001fU;
    twoTiles=1;
    loan[0]=0x6a;loan[2049]=0xb6;
    framebuffer[0]=0xface;framebuffer[192U*144U+1U]=0xbeef;
    uint32_t priorFNV=0U;
    for(unsigned cycle=0;cycle<2;++cycle) {
        EspRes60WallMaterial_reset();
        assert(EspRes60WallMaterial_record(96,7,-5,10,0));
        assert(EspRes60WallMaterial_record(1,87,90,10,0));
        assert(EspRes60WallMaterial_record(144,87,40,20,0));
        assert(EspRes60WallMaterial_record(145,7,0,20,0));
        assert(EspRes60WallMaterial_record(146,87,0,5,0));
        assert(EspRes60WallMaterial_record(147,55,-20,10,0));
        EspRes60WallMaterial_publish();
        assert(EspRes60WallMaterial_measure(&demand));
        assert(demand.pixels==56 && demand.columns==5 && demand.insetPixels==0);
        for(unsigned i=1;i<=192U*144U;++i)framebuffer[i]=0x5555;
        unsigned beforeReads=reads,beforeOpens=opens;
#if DOOMRPG_RES60_P41_SHARED_WALL_PAK
        if(cycle) {
            assert(EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH));
            const unsigned ownOpens=opens,ownCloses=closes;
            assert(EspRes60TextureProbe_renderFullWallsWithOpenPack(framebuffer+1,55296,loan+1,6144,&result));
            assert(result.sharedWallPak==1 && pack && opens==ownOpens && closes==ownCloses);
            EspAssetPack_close();
        } else {
            assert(EspRes60TextureProbe_renderFullWalls(framebuffer+1,55296,loan+1,6144,&result));
            assert(result.sharedWallPak==0);
        }
#else
        assert(EspRes60TextureProbe_renderFullWalls(framebuffer+1,55296,loan+1,6144,&result));
#endif
        assert(result.cacheHit==cycle && result.admittedWallIds==2);
        assert(result.wallPixels==56 && result.wallColumns==5 && result.streamReads==2);
        assert(result.streamBytes==4096 && result.readBytes==4096+(cycle?0:4));
        assert(reads==beforeReads+2+(cycle?0:1) && opens==beforeOpens+1);
        assert(result.streamMetadataBytes==2*sizeof(records[0]));
        assert(result.sampleFNV==0 && result.pixels==0);
        assert(framebuffer[24U*192U+96U+1U]==0xf800U);
        assert(framebuffer[114U*192U+1U+1U]==0x001fU);
        assert(framebuffer[24U*192U+146U+1U]==0x001fU); /* formerly hidden */
        assert(framebuffer[24U*192U+145U+1U]==0xf800U);
        assert(framebuffer[24U*192U+147U+1U]==0x5555U);
        assert(framebuffer[29U*192U+96U+1U]==0x5555U);
        assert(framebuffer[0]==0xface && framebuffer[192U*144U+1U]==0xbeef);
        assert(loan[0]==0x6a && loan[2049]==0xb6 && loan[6145]==0xa5 && !pack && !catalogReady && opens==closes);
        if(cycle)assert(result.streamFNV==priorFNV);
        priorFNV=result.streamFNV;
    }
    plan(55);unsigned beforeReads=reads;
    assert(!EspRes60TextureProbe_renderFullWalls(framebuffer+1,55296,loan+1,6144,&result));
    assert(reads==beforeReads && !pack && !catalogReady && opens==closes);
    plan(7);
    assert(EspRes60TextureProbe_renderFullWalls(framebuffer+1,55296,loan+1,6144,&result));
    changedEntry=1;plan(87);
    assert(!EspRes60TextureProbe_renderFullWalls(framebuffer+1,55296,loan+1,6144,&result));
    assert(!pack && !catalogReady && opens==closes);
    changedEntry=0;
    plan(87);failTile=1;
    assert(!EspRes60TextureProbe_renderFullWalls(framebuffer+1,55296,loan+1,6144,&result));
    assert(!pack && !catalogReady && opens==closes && result.wallPixels==0);
    failTile=0;
    plan(87);
    assert(EspRes60TextureProbe_renderFullWalls(framebuffer+1,55296,loan+1,6144,&result));
    assert(!result.cacheHit && result.wallPixels==192U*80U);
#if DOOMRPG_RES60_P41_SHARED_WALL_PAK
    /* Borrowed owner failure leaves its handle with caller; a cold
     * borrow is forbidden, no hidden reopen or extra close. */
    plan(87);changedEntry=1;
    assert(EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH));
    unsigned ownOpens=opens,ownCloses=closes;
    assert(!EspRes60TextureProbe_renderFullWallsWithOpenPack(framebuffer+1,55296,loan+1,6144,&result));
    assert(pack && opens==ownOpens && closes==ownCloses && !result.wallPixels);
    changedEntry=0;plan(87);
    assert(!EspRes60TextureProbe_renderFullWallsWithOpenPack(framebuffer+1,55296,loan+1,6144,&result));
    assert(pack && opens==ownOpens && closes==ownCloses);
    EspAssetPack_close();
    plan(87);
    assert(EspRes60TextureProbe_renderFullWalls(framebuffer+1,55296,loan+1,6144,&result));
    plan(87);failTile=1;
    assert(EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH));
    ownOpens=opens;ownCloses=closes;
    assert(!EspRes60TextureProbe_renderFullWallsWithOpenPack(framebuffer+1,55296,loan+1,6144,&result));
    assert(pack && opens==ownOpens && closes==ownCloses && !result.wallPixels);
    EspAssetPack_close();failTile=0;
    puts("RES60-P41 borrowed PAK success/failure ownership PASS");
#endif
    for(size_t i=2050U;i<sizeof(loan);++i)assert(loan[i]==0xa5);
    EspRes60TextureProbe_reset();
    puts("RES60-P40 FULL WALLS C11 PASS");
    return 0;
}
#else
int main(void) {
    EspRes60TextureProbeReport result;
    uint16_t chosen=0;
    runtime.lineCount=2;
    runtime.sourceCrc32=0x623f34e4U;
    runtime.arenaFNV1a=0xc3882516U;
    runtime.sourceBytes=21823U;
    memset(records,0,sizeof(records));
    records[0].resourceId=87;
    records[1].resourceId=7;
    records[0].paletteRgb565[0]=records[1].paletteRgb565[0]=0x001fU;
    records[0].paletteRgb565[1]=records[1].paletteRgb565[1]=0xf800U;
    framebuffer[0]=0xface;framebuffer[192U*144U+1U]=0xbeef;
    for(unsigned i=1;i<=192U*144U;++i)framebuffer[i]=0x5555;
    /* P3 column winners are texture 7; first raw BSP line uses 87. */
    plan(7);
    assert(EspRes60WallMaterial_visibleTextureAt(96,&chosen) && chosen==7);
    assert(EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(result.logicalId==7 && result.selectedFromVisible==1);
    assert(result.visibleWallColumns==192 && result.wallColumns==192);
    assert(result.wallPixels>0 && result.wallFNV!=2166136261U);
    assert(!result.cacheHit && result.readBytes==2052 && reads==2 && opens==closes);
    assert(framebuffer[0]==0xface && framebuffer[192U*144U+1U]==0xbeef);
    /* Change view: keep bounded P8 cache and zero additional PAK reads. */
    plan(87);
    assert(EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(result.logicalId==7 && result.cacheHit==1 && result.readBytes==0);
    assert(result.selectedFromVisible==1 && result.visibleWallColumns==0);
    assert(result.wallColumns==0 && result.wallPixels==0 && reads==2);
    assert(!pack && !catalogReady && opens==closes);
#if DOOMRPG_RES60_P38_WALL_ADMISSION
    assert(result.admittedWallIds==1U);
    /* A new uncatalogued warm winner is rejected before any swatch write. */
    plan(55);
    uint16_t beforeWarm=framebuffer[24U*192U+144U+1U];
    assert(!EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(framebuffer[24U*192U+144U+1U]==beforeWarm && reads==2);
    assert(!pack && !catalogReady && opens==closes);
    /* Cold missing, odd-offset, and out-of-range winners must not fallback. */
    plan(55);
    assert(!EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(reads==3 && !pack && !catalogReady && opens==closes);
    EspRes60TextureProbe_reset();
    visibleOddNibble=1;
    plan(7);
    assert(!EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(reads==4 && !pack && !catalogReady && opens==closes);
    EspRes60TextureProbe_reset();
    visibleOddNibble=0;outsideRange=1;
    plan(7);
    assert(!EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(reads==5 && !pack && !catalogReady && opens==closes);
    outsideRange=0;
    /* Include a winner entirely behind the swatch and ignore clipped-out rays. */
    EspRes60TextureProbe_reset();
    assert(EspRes60WallMaterial_record(0,7,0,80,0));
    assert(EspRes60WallMaterial_record(144,87,0,30,0));
    assert(EspRes60WallMaterial_record(145,55,-20,10,0));
    EspRes60WallMaterial_publish();
    assert(EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(result.admittedWallIds==2U && reads==7);
    EspRes60TextureProbe_reset();
    assert(EspRes60WallMaterial_record(0,7,0,80,0));
    assert(EspRes60WallMaterial_record(144,256,0,30,0));
    EspRes60WallMaterial_publish();
    assert(!EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(reads==8 && !pack && !catalogReady && opens==closes);
#else
    /* Uncatalogued visible wall must fallback to original P8 first line. */
    EspRes60TextureProbe_reset();
    plan(55);
    assert(EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(result.logicalId==87 && result.selectedFromVisible==0);
    assert(result.wallColumns==0 && result.readBytes==2052 && reads==4);
    /* A visible but odd-offset source must never be admitted. */
    EspRes60TextureProbe_reset();
    visibleOddNibble=1;
    plan(7);
    assert(EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(result.logicalId==87 && result.selectedFromVisible==0);
    assert(result.wallColumns==0 && reads==6);
#endif
    /* Corrupted PAK size fails closed before the P8 swatch is drawn. */
    EspRes60TextureProbe_reset();
    visibleOddNibble=0;badHeader=1;
    plan(7);
    const uint16_t before=framebuffer[24U*192U+144U+1U];
    assert(!EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    assert(framebuffer[24U*192U+144U+1U]==before);
    assert(!pack && !catalogReady && opens==closes);
    assert(framebuffer[0]==0xface && framebuffer[192U*144U+1U]==0xbeef);
#if DOOMRPG_RES60_P39_WALL_STREAM
    EspRes60TextureProbe_reset();
    badHeader=0;twoTiles=1;
    /* Two genuinely distinct offsets, patterns and palettes; test clipped
     * and inset-hidden columns, cold and warm, independent expected pixels. */
    uint8_t loan[2050];
    loan[0]=0x6a;loan[2049]=0xb6;
    for(unsigned cycle=0;cycle<2;++cycle) {
        EspRes60WallMaterial_reset();
        assert(EspRes60WallMaterial_record(96,7,-5,10,0));
        assert(EspRes60WallMaterial_record(1,87,90,10,0));
        assert(EspRes60WallMaterial_record(144,87,40,20,0));
        assert(EspRes60WallMaterial_record(145,7,0,20,0));
        EspRes60WallMaterial_publish();
        for(unsigned i=1;i<=192U*144U;++i)framebuffer[i]=0x5555;
        assert(EspRes60TextureProbe_render(framebuffer+1,55296,&result));
        assert(result.admittedWallIds==2 && result.cacheHit==cycle);
        const uint16_t swatchBefore=framebuffer[24U*192U+144U+1U];
        unsigned beforeReads=reads;
        assert(EspRes60TextureProbe_streamWalls(framebuffer+1,55296,loan+1,2048,&result));
        assert(result.wallPixels==23 && result.wallColumns==3);
        assert(result.streamReads==1 && result.streamBytes==2048 && reads==beforeReads+1);
        assert(result.streamMetadataBytes==2*sizeof(records[0]));
        assert(result.streamFNV!=2166136261U);
        assert(framebuffer[24U*192U+144U+1U]==swatchBefore);
        assert(framebuffer[24U*192U+96U+1U]==0xf800U); /* ID7, even nibble */
        assert(framebuffer[114U*192U+1U+1U]==0x001fU); /* ID87 */
        assert(framebuffer[72U*192U+144U+1U]==0x001fU);
        assert(framebuffer[29U*192U+96U+1U]==0x5555U);
        assert(framebuffer[0]==0xface && framebuffer[192U*144U+1U]==0xbeef);
        assert(loan[0]==0x6a && loan[2049]==0xb6 && !pack && !catalogReady && opens==closes);
    }
    /* Same published plan but the backing entry changes; reject and close. */
    plan(87);
    assert(EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    changedEntry=1;
    assert(!EspRes60TextureProbe_streamWalls(framebuffer+1,55296,loan+1,2048,&result));
    assert(!pack && !catalogReady && opens==closes);
    changedEntry=0;
    /* Cold selected ID7 plus ID87, then inject remaining tile IO failure. */
    EspRes60WallMaterial_reset();
    assert(EspRes60WallMaterial_record(96,7,0,20,0));
    assert(EspRes60WallMaterial_record(0,87,0,20,0));
    EspRes60WallMaterial_publish();
    assert(EspRes60TextureProbe_render(framebuffer+1,55296,&result));
    failTile=1;
    assert(!EspRes60TextureProbe_streamWalls(framebuffer+1,55296,loan+1,2048,&result));
    assert(!pack && !catalogReady && opens==closes);
    assert(!EspRes60TextureProbe_streamWalls(framebuffer+1,55296,loan+1,2048,&result));
    EspRes60TextureProbe_reset();
#endif
    puts("RES60-P10 VISIBLE SOURCE C11 PASS");
    return 0;
}
#endif
"""
with tempfile.TemporaryDirectory(prefix="res60-p10-visible-") as name:
    root=Path(name)
    src=root/"source.c";exe=root/"fixture"
    src.write_text(harness)
    defs=["-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
          "-DDOOMRPG_RES60_P2_LAYOUT=1",
          "-DDOOMRPG_RES60_P3_WORLD=1",
          "-DDOOMRPG_RES60_P5_TITLE=1",
          "-DDOOMRPG_RES60_P6_MENU=1",
          "-DDOOMRPG_RES60_P7_NAV=1",
          "-DDOOMRPG_RES60_P8_TEXEL=1",
          "-DDOOMRPG_RES60_P9_WEDGE=1",
          "-DDOOMRPG_RES60_P10_WALL_MATERIAL=1"]
    for admission,stream,full in ((0,0,0),(1,0,0),(1,1,0),(1,1,1)):
        subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                        *defs,f"-DDOOMRPG_RES60_P38_WALL_ADMISSION={admission}",
                        f"-DDOOMRPG_RES60_P39_WALL_STREAM={stream}",
                        f"-DDOOMRPG_RES60_P40_FULL_WALLS={full}",
                        "-I",str(ROOT/"ESP32/include"),
                        str(ROOT/"ESP32/src/esp_res60_texture_probe.c"),
                        str(ROOT/"ESP32/src/esp_res60_wall_material.c"),
                        str(src),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    # Exercise the exact target profile too: P34 two-window catalog stub,
    # P11 timing, P40 full walls, real 6144B scratch loan and tail canaries.
    cfg=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
    assert cfg.read(ROOT/"ESP32/platformio.ini")
    profile=["-D"+f for f in re.findall(r"-D\s+(DOOMRPG_RES60_[A-Z0-9_]+=1)",
                cfg["env:esp32-cyd-res60"]["build_flags"])]
    assert "-DDOOMRPG_RES60_P40_FULL_WALLS=1" in profile
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                    *profile,"-I",str(ROOT/"ESP32/include"),
                    str(ROOT/"ESP32/src/esp_res60_texture_probe.c"),
                    str(ROOT/"ESP32/src/esp_res60_wall_material.c"),
                    str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print("RES60-P10 HOST PASS: real C11 P8/P10 chosen visible material and strict safety")
