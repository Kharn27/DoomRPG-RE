#!/usr/bin/env python3
"""RES60-P10: real P8/P10 C11 integration, visible-source preference and fallback."""
from pathlib import Path
import subprocess
import tempfile

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
    catalog.textures=records;
    catalog.textureCount=2;
    catalog.stateFNV1a=0x11223344U;
    return ESP_NATIVE_GRAPHICS_CATALOG_OK;
}
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
    memset(entry,0,sizeof(*entry));entry->size=2052;return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* entry, uint32_t off,
                           void* dest, size_t size) {
    assert(pack && entry && dest);
    ++reads;
    if(off==0U && size==4U) {
        const uint8_t header[4]={0U,badHeader?7U:8U,0U,0U};
        memcpy(dest,header,sizeof(header));return 1;
    }
    assert(off==4U && size==2048U);
    memset(dest,0x10,size);return 1;
}
static void plan(uint16_t texture) {
    EspRes60WallMaterial_reset();
    for(uint32_t x=0;x<192U;++x)
       assert(EspRes60WallMaterial_record(x,texture,0,80,(uint8_t)(x%64U)));
    EspRes60WallMaterial_publish();
}
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
    puts("RES60-P10 VISIBLE SOURCE C11 PASS: visible-over-first BSP, cached turns, fallback, odd-offset rejection, corrupt PAK rollback, canaries");
    return 0;
}
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
    for admission in (0,1):
        subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                        *defs,f"-DDOOMRPG_RES60_P38_WALL_ADMISSION={admission}",
                        "-I",str(ROOT/"ESP32/include"),
                        str(ROOT/"ESP32/src/esp_res60_texture_probe.c"),
                        str(ROOT/"ESP32/src/esp_res60_wall_material.c"),
                        str(src),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
print("RES60-P10 HOST PASS: real C11 P8/P10 chosen visible material and strict safety")
