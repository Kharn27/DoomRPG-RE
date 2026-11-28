#!/usr/bin/env python3
"""C11 tests for original PAK title ownership, bounds and flag isolation."""
from pathlib import Path
import configparser
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
cfg=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
assert cfg.read(root/"ESP32/platformio.ini")
assert "DOOMRPG_RES60_P5_TITLE=1" in cfg["env:esp32-cyd-res60"]["build_flags"]
assert cfg["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted"):
    assert "DOOMRPG_RES60_P5_TITLE" not in cfg[env]["build_flags"]
main=(root/"ESP32/src/main.cpp").read_text()
assert "res60TouchWasDown" in main
assert "PlatformVideo_showRes60WorldPreview();" in main
assert "PlatformVideo_showRes60TitlePreview();" in main
assert "RES60-P5 requires isolated P1/P2/P3" in (root/"ESP32/include/platform_video_config.h").read_text()
src=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_asset_pack.h"
#include "esp_native_indexed_bmp.h"
#include "esp_res60_layout.h"
#include "esp_res60_title_preview.h"
static int active,opened,closed,fail;
int EspAssetPack_isOpen(void){return active;}
int EspAssetPack_open(const char* p){
 assert(!active && !strcmp(p,ESP_ASSET_PACK_DEFAULT_PATH));
 active=1;opened++;return 1;
}
void EspAssetPack_close(void){assert(active);active=0;closed++;}
EspNativeIndexedBmpStatus EspNativeIndexedBmp_open(
 const char* p,EspNativeIndexedBmp* b,EspNativeIndexedBmpStats* s){
 assert(active && b && s);
 if(fail && !strcmp(p,"j.bmp"))return ESP_NATIVE_INDEXED_BMP_NOT_FOUND;
 memset(b,0,sizeof(*b));
 if(!strcmp(p,"c.bmp")){b->width=192;b->height=32;}
 else{assert(!strcmp(p,"j.bmp"));b->width=108;b->height=74;}
 s->packReads++;s->bytesRead+=128;return ESP_NATIVE_INDEXED_BMP_OK;
}
EspNativeIndexedBmpStatus EspNativeIndexedBmp_tile(
 const EspNativeIndexedBmp* b,uint16_t* f,uint16_t w,uint16_t h,
 int16_t x,int16_t y,uint16_t rw,uint16_t rh,uint8_t t,EspNativeIndexedBmpStats* s){
 assert(active && b->width==192 && w==192 && h==144 && x==0 && y==0);
 assert(rw==192 && rh==144 && t==0);
 for(size_t i=0;i<192U*144U;i++)f[i]=0x001f;
 s->pixelsWritten+=w*h;return ESP_NATIVE_INDEXED_BMP_OK;
}
EspNativeIndexedBmpStatus EspNativeIndexedBmp_blit(
 const EspNativeIndexedBmp* b,uint16_t* f,uint16_t w,uint16_t h,
 uint16_t sx,uint16_t sy,uint16_t rw,uint16_t rh,
 int16_t x,int16_t y,uint8_t t,EspNativeIndexedBmpStats* s){
 assert(active && b->width==108 && w==192 && h==144);
 assert(sx==0 && sy==0 && rw==108 && rh==74 && x==42 && y==35 && t==1);
 f[(size_t)y*w+x]=0xf800;s->pixelsWritten+=rw*rh;
 return ESP_NATIVE_INDEXED_BMP_OK;
}
int main(void){
 uint16_t g[192U*144U+2U],*f=g+1;EspRes60TitlePreviewReport r;
 g[0]=0xface;g[192U*144U+1U]=0xbeef;f[0]=0xabcd;
 assert(!EspRes60TitlePreview_render(NULL,ESP_RES60_FRAMEBUFFER_BYTES,&r));
 assert(!EspRes60TitlePreview_render(f,ESP_RES60_FRAMEBUFFER_BYTES-2,&r));
 assert(opened==0 && f[0]==0xabcd);
 active=1;assert(!EspRes60TitlePreview_render(f,ESP_RES60_FRAMEBUFFER_BYTES,&r));active=0;
 fail=1;assert(!EspRes60TitlePreview_render(f,ESP_RES60_FRAMEBUFFER_BYTES,&r));
 assert(opened==1 && closed==1 && f[0]==0xabcd);
 fail=0;assert(EspRes60TitlePreview_render(f,ESP_RES60_FRAMEBUFFER_BYTES,&r));
 assert(opened==2 && closed==2 && !active);
 assert(r.logoWidth==108 && r.logoHeight==74);
 assert(r.frameFNV==EspRes60Layout_fnv(f,ESP_RES60_FRAMEBUFFER_BYTES));
 assert(f[0]==0x001f && f[35U*192U+42U]==0xf800);
 assert(g[0]==0xface && g[192U*144U+1U]==0xbeef);
 puts("RES60-P5 native title/lease/canaries PASS");return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p5-") as d:
 d=Path(d);f=d/"h.c";exe=d/"h";f.write_text(src)
 subprocess.run(["cc","-O2","-std=c11","-Wall","-Wextra","-Werror",
 "-DDOOMRPG_RES60_P1_DIAGNOSTIC=1","-DDOOMRPG_RES60_P2_LAYOUT=1",
 "-DDOOMRPG_RES60_P3_WORLD=1","-DDOOMRPG_RES60_P5_TITLE=1",
 "-I",str(root/"ESP32/include"),str(root/"ESP32/src/esp_res60_title_preview.c"),
 str(f),"-o",str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
 "-DDOOMRPG_RES60_P5_TITLE=0","-I",str(root/"ESP32/include"),
 "-c",str(root/"ESP32/src/esp_res60_title_preview.c"),"-o",str(d/"normal.o")],check=True)
 assert "EspRes60TitlePreview_render" not in subprocess.check_output(["nm","-g",str(d/"normal.o")],text=True)
print("RES60-P5 HOST PASS isolated native original title")
