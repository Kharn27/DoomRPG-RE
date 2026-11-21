#!/usr/bin/env python3
"""RES60-P8: exact real C wall nibble+palette+PAK sample, no gameplay start.

The sampled swatch is diagnostic: actual MAP_INTRO source texels, NOT yet
perspective-correct textured world polygons.
"""
from pathlib import Path
import configparser
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
cfg=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
assert cfg.read(ROOT/"ESP32/platformio.ini")
flags=cfg["env:esp32-cyd-res60"]["build_flags"]
assert "-D DOOMRPG_RES60_P8_TEXEL=1" in flags
assert cfg["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted"):
    assert "DOOMRPG_RES60_P8_TEXEL" not in cfg[env]["build_flags"]
v=(ROOT/"ESP32/src/platform_video.cpp").read_text()
w=(ROOT/"ESP32/src/esp_res60_world_probe.c").read_text()
t=(ROOT/"ESP32/src/esp_res60_texture_probe.c").read_text()
assert "EspRes60TextureProbe_render(framebuffer,framebufferBytes," in w
assert "EspMapRuntime_reset();" in w
assert w.index("EspRes60Preview_render(")<w.index("EspRes60TextureProbe_render(")
assert "rawP3FNV" in v and "[RES60P8] SWATCH" in v
assert "sourceToFramebuffer565" in t
assert "EspNativeGraphicsCatalog_buildFromRuntime()" in t
assert 'EspAssetPack_findEntry("wtexels.bin",&texels)' in t
assert "sourceOffset&1U" in t
assert "EspNativeGraphicsCatalog_reset();" in t
assert "EspAssetPack_close();" in t
assert "EspRes60TextureProbe_paint(" in t
assert "cachedSourceCrc==activeMap->sourceCrc32" in t
assert "cachedArenaFNV==activeMap->arenaFNV1a" in t
assert "out->cacheHit=1U;" in t
assert "out->readBytes=0U;" in t
assert "EspRes60TextureProbe_reset();" in (ROOT/"ESP32/src/main.cpp").read_text()
assert "report.texture.cacheHit" in v

c=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_asset_pack.h"
#include "esp_map_runtime.h"
#include "esp_native_graphics_catalog.h"
#include "esp_res60_layout.h"
#include "esp_res60_texture_probe.h"
static uint16_t arr[192U*144U+2U];
static int runtime=1,pack=0,catalogActive=0,opens=0,closes=0,reads=0;
static int failBuild=0,failRange=0,badHeader=0,badNibble=0;
static EspMapRuntimeView map;
static EspNativeGraphicsCatalogView view;
static EspNativeGraphicsCatalogRecord rec;
int EspMapRuntime_isLoaded(void){return runtime;}
const EspMapRuntimeView* EspMapRuntime_view(void){return runtime?&map:NULL;}
int EspMapRuntime_getLine(uint32_t i,EspMapLine* line){
 if(!runtime||i>=map.lineCount||!line)return 0;
 memset(line,0,sizeof(*line));
 line->texture=5;
 return 1;
}
int EspNativeGraphicsCatalog_isReady(void){return catalogActive;}
EspNativeGraphicsCatalogStatus EspNativeGraphicsCatalog_buildFromRuntime(void){
 assert(runtime && !pack && !catalogActive);
 if(failBuild)return ESP_NATIVE_GRAPHICS_CATALOG_READ_FAILED;
 catalogActive=1;
 rec.sourceOffset=badNibble?1:0;
 view.textures=&rec;view.textureCount=1;view.stateFNV1a=0x12345678;
 return ESP_NATIVE_GRAPHICS_CATALOG_OK;
}
const EspNativeGraphicsCatalogView* EspNativeGraphicsCatalog_view(void) {
 return catalogActive?&view:NULL;
}
const EspNativeGraphicsCatalogRecord* EspNativeGraphicsCatalog_findTexture(uint16_t id){
 assert(catalogActive);
 return id==5?&rec:NULL;
}
void EspNativeGraphicsCatalog_reset(void){catalogActive=0;}
int EspAssetPack_isOpen(void){return pack;}
int EspAssetPack_open(const char* p){
 assert(!pack && !strcmp(p,ESP_ASSET_PACK_DEFAULT_PATH));
 pack=1;opens++;return 1;
}
void EspAssetPack_close(void){assert(pack);pack=0;closes++;}
int EspAssetPack_findEntry(const char* name,EspAssetPackEntry* e){
 assert(pack && !strcmp(name,"wtexels.bin") && e);
 memset(e,0,sizeof(*e));e->size=2052;return 1;
}
int EspAssetPack_readRange(const EspAssetPackEntry* e,uint32_t offset,
                          void* dest,size_t n) {
 assert(pack && e && dest);
 reads++;
 if(failRange && n==2048)return 0;
 if(offset==0 && n==4){
   uint8_t hdr[4]={0x00U,0x08U,0x00U,0x00U};
   if(badHeader)hdr[1]=7;
   memcpy(dest,hdr,4);return 1;
 }
 assert(offset==4 && n==2048);
 memset(dest,0x10,2048);return 1;
}
int main(void) {
 uint16_t* f=arr+1;uint32_t h=9876;
 EspRes60TextureProbeReport report;
 arr[0]=0xface;arr[192U*144U+1U]=0xbeef;
 for(unsigned i=0;i<192U*144U;++i)f[i]=0x4dd3;
 map.lineCount=1;
 map.sourceCrc32=0x623f34e4U;
 map.arenaFNV1a=0xc3882516U;
 map.sourceBytes=21823U;
 memset(&rec,0,sizeof(rec));rec.resourceId=5;
 rec.paletteRgb565[0]=0x001f; /* native SOURCE BGR565 red -> framebuffer red */
 rec.paletteRgb565[1]=0xf800; /* native SOURCE blue -> framebuffer blue */
 assert(!EspRes60TextureProbe_paint(NULL,55296,NULL,0,NULL,&h)&&h==0);
 h=9876;
 assert(!EspRes60TextureProbe_render(NULL,55296,&report));
 assert(!EspRes60TextureProbe_render(f,55294,&report));
 assert(!EspRes60TextureProbe_render(f,55296,NULL));
 assert(!pack && !catalogActive && opens==0);
 runtime=0;
 assert(!EspRes60TextureProbe_render(f,55296,&report));
 runtime=1;pack=1;
 assert(!EspRes60TextureProbe_render(f,55296,&report));pack=0;
 catalogActive=1;
 assert(!EspRes60TextureProbe_render(f,55296,&report));catalogActive=0;
 failBuild=1;
 assert(!EspRes60TextureProbe_render(f,55296,&report));
 failBuild=0;badNibble=1;
 assert(!EspRes60TextureProbe_render(f,55296,&report));
 assert(!catalogActive && !pack && opens==0);
 badNibble=0;badHeader=1;
 assert(!EspRes60TextureProbe_render(f,55296,&report));
 assert(!pack && !catalogActive && opens==closes);
 assert(f[24U*192U+144U]==0x4dd3);
 badHeader=0;failRange=1;
 assert(!EspRes60TextureProbe_render(f,55296,&report));
 assert(f[24U*192U+144U]==0x4dd3);
 assert(!pack && !catalogActive && opens==closes);
 failRange=0;
 assert(EspRes60TextureProbe_render(f,55296,&report));
 assert(report.cacheHit==0 && report.readBytes==2052);
 assert(report.logicalId==5 && report.catalogCount==1);
 assert(report.sourceOffsetNibbles==0 && report.catalogFNV==0x12345678);
 assert(report.readBytes==2052 && report.pixels==2304);
 assert(report.texelFNV!=0 && report.sampleFNV!=0);
 assert(f[24U*192U+144U]==0xf800); /* low nibble0 */
 assert(f[24U*192U+145U]==0x001f); /* high nibble1 */
 assert(f[24U*192U+143U]==0x4dd3); /* left remains original world */
 assert(f[72U*192U+144U]==0x4dd3); /* below inset remains original world */
 assert(f[23U*192U+144U]==0x4dd3); /* above inset remains original HUD */
 assert(arr[0]==0xface && arr[192U*144U+1U]==0xbeef);
 assert(!pack && !catalogActive && opens==closes);
 uint32_t witness=EspRes60Layout_fnv(f,55296), same=report.sampleFNV;
 int previousOpens=opens, previousReads=reads;
 /* Warm turns must NOT build catalog or read any PAK bytes. */
 failBuild=1;failRange=1;
 assert(EspRes60TextureProbe_render(f,55296,&report));
 assert(report.cacheHit==1 && report.readBytes==0);
 assert(report.sampleFNV==same &&
        EspRes60Layout_fnv(f,55296)==witness &&
        !pack && !catalogActive && opens==closes &&
        opens==previousOpens && reads==previousReads);
 /* Existing owners are still rejected even with a warm cache. */
 pack=1;
 assert(!EspRes60TextureProbe_render(f,55296,&report));
 pack=0;catalogActive=1;
 assert(!EspRes60TextureProbe_render(f,55296,&report));
 catalogActive=0;
 assert(opens==previousOpens && reads==previousReads);
 /* A distinct compact source identity must NOT reuse the swatch. */
 map.sourceCrc32^=1U;
 assert(!EspRes60TextureProbe_render(f,55296,&report));
 assert(!pack && !catalogActive && opens==closes && opens==previousOpens);
 map.sourceCrc32^=1U;
 failBuild=0;failRange=0;
 assert(EspRes60TextureProbe_render(f,55296,&report));
 assert(report.cacheHit==0 && report.readBytes==2052);
 assert(opens==previousOpens+1 && reads==previousReads+2);
 /* START/MENU invalidation forces full original resource revalidation. */
 EspRes60TextureProbe_reset();
 assert(EspRes60TextureProbe_render(f,55296,&report));
 assert(report.cacheHit==0 && report.readBytes==2052);
 assert(opens==previousOpens+2 && reads==previousReads+4);
 assert(report.sampleFNV==same && EspRes60Layout_fnv(f,55296)==witness);
 assert(arr[0]==0xface && arr[192U*144U+1U]==0xbeef);
 assert(!pack && !catalogActive && opens==closes);
 puts("RES60-P8 CACHE C11 PASS: zero catalog/PAK reads on turns, source/session invalidation, bounded canvas canaries");
 return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p8-") as d:
    tmp=Path(d);file=tmp/"test.c";exe=tmp/"test";file.write_text(c)
    buildflags=["-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
                "-DDOOMRPG_RES60_P2_LAYOUT=1",
                "-DDOOMRPG_RES60_P3_WORLD=1",
                "-DDOOMRPG_RES60_P5_TITLE=1",
                "-DDOOMRPG_RES60_P6_MENU=1",
                "-DDOOMRPG_RES60_P7_NAV=1",
                "-DDOOMRPG_RES60_P8_TEXEL=1"]
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
        *buildflags,"-I",str(ROOT/"ESP32/include"),
        str(ROOT/"ESP32/src/esp_res60_texture_probe.c"),
        str(file),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    obj=tmp/"production.o"
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
        "-DDOOMRPG_RES60_P8_TEXEL=0",
        "-I",str(ROOT/"ESP32/include"),"-c",
        str(ROOT/"ESP32/src/esp_res60_texture_probe.c"),
        "-o",str(obj)],check=True)
    assert "EspRes60TextureProbe_render" not in subprocess.check_output(
        ["nm","-g",str(obj)],text=True)
    forbid=subprocess.run(["cc","-std=c11","-fsyntax-only",
       "-DDOOMRPG_RES60_P8_TEXEL=1",
       "-I",str(ROOT/"ESP32/include"),
       str(ROOT/"ESP32/src/esp_res60_texture_probe.c")],
       capture_output=True,text=True)
    assert forbid.returncode!=0 and "requires P7 and original P3 world owner" in forbid.stderr
print("RES60-P8 HOST PASS native wall swatch, PAK provenance and production isolation")
