#!/usr/bin/env python3
"""RES60-P6: original PAK title + original skin 160x120 view inside 192x144.

C11 tests compile the REAL draw/hit/router code. No CYD claims from host tests.
"""
from pathlib import Path
import configparser
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
cfg = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
assert cfg.read(root / "ESP32/platformio.ini")
pilot = cfg["env:esp32-cyd-res60"]
assert "-D DOOMRPG_RES60_P6_MENU=1" in pilot["build_flags"]
assert cfg["env:esp32-cyd-res60-inverted"]["extends"] == "env:esp32-cyd-res60"
for env in ("env:esp32-cyd", "env:esp-cyd-inverted"):
    assert "DOOMRPG_RES60_P6_MENU" not in cfg[env]["build_flags"]
main = (root / "ESP32/src/main.cpp").read_text()
assert "PlatformInput_setTapCallback(res60MenuTapCallback);" in main
assert "EspRes60Menu_onTap(x, y, &res60MenuArmed)" in main
assert "PlatformVideo_showRes60WorldPreview();" in main
assert "PlatformVideo_updateRes60MenuArmed" in main
assert "gameplay=NOT_STARTED" in main
src = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_asset_pack.h"
#include "esp_native_indexed_bmp.h"
#include "esp_res60_layout.h"
#include "esp_res60_title_preview.h"
#include "esp_res60_menu.h"
static int active, opens, closes, failLogo, tileCount, blitCount;
static uint16_t f1[192U*144U+2U], f2[192U*144U+2U];
int EspAssetPack_isOpen(void){return active;}
int EspAssetPack_open(const char* p){
 assert(!active && !strcmp(p, ESP_ASSET_PACK_DEFAULT_PATH));
 active=1; opens++; return 1;
}
void EspAssetPack_close(void){assert(active);active=0;closes++;}
EspNativeIndexedBmpStatus EspNativeIndexedBmp_open(
 const char* path, EspNativeIndexedBmp* b, EspNativeIndexedBmpStats* stats) {
 assert(active && b && stats);
 if(failLogo && !strcmp(path,"j.bmp"))
     return ESP_NATIVE_INDEXED_BMP_NOT_FOUND;
 memset(b,0,sizeof(*b));
 if(!strcmp(path,"c.bmp")){b->width=192;b->height=128;}
 else {assert(!strcmp(path,"j.bmp"));b->width=108;b->height=74;}
 stats->packReads++;stats->bytesRead+=32;return ESP_NATIVE_INDEXED_BMP_OK;
}
EspNativeIndexedBmpStatus EspNativeIndexedBmp_tile(
 const EspNativeIndexedBmp* b,uint16_t* fb,uint16_t w,uint16_t h,
 int16_t x,int16_t y,uint16_t rw,uint16_t rh,uint8_t tr,
 EspNativeIndexedBmpStats* stats){
 assert(active && b->width==192 && w==192 && h==144);
 assert(x==0 && y==0 && rw==192 && rh==144 && tr==0);
 for(size_t i=0;i<192U*144U;i++)fb[i]=0x1234U;
 tileCount++;stats->pixelsWritten+=(uint32_t)w*h;
 return ESP_NATIVE_INDEXED_BMP_OK;
}
EspNativeIndexedBmpStatus EspNativeIndexedBmp_blit(
 const EspNativeIndexedBmp* b,uint16_t* fb,uint16_t w,uint16_t h,
 uint16_t sx,uint16_t sy,uint16_t rw,uint16_t rh,
 int16_t x,int16_t y,uint8_t tr,EspNativeIndexedBmpStats* stats){
 assert(active && b->width==108 && w==192 && h==144);
 assert(sx==0 && sy==0 && rw==108 && rh==74 && tr==1);
 assert(x==12 && y==35);
 fb[(size_t)y*w+x]=0x5678U;
 blitCount++;stats->pixelsWritten+=rw*rh;
 return ESP_NATIVE_INDEXED_BMP_OK;
}
int main(void){
 EspRes60MenuReport a,b;int8_t selected=-1;
 uint16_t* f=f1+1,*g=f2+1;
 f1[0]=0xface;f1[192U*144U+1]=0xbeef;
 f2[0]=0xcafe;f2[192U*144U+1]=0xdead;f[0]=0x4321;
 assert(!EspRes60Menu_render(NULL,ESP_RES60_FRAMEBUFFER_BYTES,-1,&a));
 assert(!EspRes60Menu_render(f,ESP_RES60_FRAMEBUFFER_BYTES-2,-1,&a));
 assert(!EspRes60Menu_render(f,ESP_RES60_FRAMEBUFFER_BYTES,-2,&a));
 assert(opens==0 && f[0]==0x4321);
 active=1;assert(!EspRes60Menu_render(f,ESP_RES60_FRAMEBUFFER_BYTES,-1,&a));active=0;
 failLogo=1;assert(!EspRes60Menu_render(f,ESP_RES60_FRAMEBUFFER_BYTES,-1,&a));
 assert(opens==1 && closes==1 && f[0]==0x4321);
 failLogo=0;
 assert(EspRes60Menu_render(f,ESP_RES60_FRAMEBUFFER_BYTES,-1,&a));
 assert(opens==2 && closes==2 && !active && tileCount==1 && blitCount==1);
 assert(a.title.logoWidth==108 && a.title.logoHeight==74);
 assert(a.finalFNV==EspRes60Layout_fnv(f,ESP_RES60_FRAMEBUFFER_BYTES));
 assert(f[35U*192U+12U]==0x5678 && f[0]==0x1234);
 const uint32_t idleHash=a.finalFNV;
 assert(!EspRes60Menu_repaintSelection(f,ESP_RES60_FRAMEBUFFER_BYTES-2,-1,2));
 assert(EspRes60Menu_repaintSelection(f,ESP_RES60_FRAMEBUFFER_BYTES,-1,2));
 assert(a.finalFNV!=EspRes60Layout_fnv(f,ESP_RES60_FRAMEBUFFER_BYTES));
 assert(opens==2 && closes==2 && !active); /* no PAK on selection */
 assert(EspRes60Menu_render(g,ESP_RES60_FRAMEBUFFER_BYTES,2,&b));
 assert(EspRes60Layout_fnv(f,ESP_RES60_FRAMEBUFFER_BYTES)==b.finalFNV);
 assert(EspRes60Menu_repaintSelection(f,ESP_RES60_FRAMEBUFFER_BYTES,2,-1));
 assert(EspRes60Layout_fnv(f,ESP_RES60_FRAMEBUFFER_BYTES)==idleHash);
 assert(!EspRes60Menu_repaintSelection(f,ESP_RES60_FRAMEBUFFER_BYTES,-2,0));
 assert(f1[0]==0xface && f1[192U*144U+1]==0xbeef);
 assert(f2[0]==0xcafe && f2[192U*144U+1]==0xdead);
 assert(EspRes60Menu_hitPhysical(-1,1)==-1);
 assert(EspRes60Menu_hitPhysical(319,239)==-1);
 assert(EspRes60Menu_hitPhysical(320,100)==-1);
 assert(EspRes60Menu_hitPhysical(0,0)==-1);
 /* Exact floor inverse of 320x240 nearest scaling, 4 real card centres. */
 for(int i=0;i<4;i++){
   int px=270, py=((12+8+12+27*i)*5)/3;
   assert(EspRes60Menu_hitPhysical(px,py)==i);
   assert(EspRes60Menu_onTap(px,py,&selected)==ESP_RES60_MENU_TAP_SELECTION);
   assert(selected==i);
   assert(EspRes60Menu_onTap(px,py,&selected)==
      (i==0?ESP_RES60_MENU_TAP_START_PREVIEW:ESP_RES60_MENU_TAP_UNSUPPORTED));
 }
 assert(selected==3);
 assert(EspRes60Menu_onTap(0,0,&selected)==ESP_RES60_MENU_TAP_SELECTION);
 assert(selected==-1);
 assert(EspRes60Menu_onTap(0,0,&selected)==ESP_RES60_MENU_TAP_NO_CHANGE);
 assert(EspRes60Menu_onTap(-2,0,&selected)==ESP_RES60_MENU_TAP_INVALID);
 assert(EspRes60Menu_onTap(270,53,NULL)==ESP_RES60_MENU_TAP_INVALID);
 selected=-2;assert(EspRes60Menu_onTap(270,53,&selected)==ESP_RES60_MENU_TAP_INVALID);
 assert(opens==3 && closes==3 && !active);
 puts("RES60-P6 original title, industrial skin, four hitboxes, two-tap, fail-closed and canaries PASS");
 return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p6-") as tmp:
    d=Path(tmp); c=d/"test.c"; exe=d/"test";c.write_text(src)
    flags=["-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
           "-DDOOMRPG_RES60_P2_LAYOUT=1",
           "-DDOOMRPG_RES60_P3_WORLD=1",
           "-DDOOMRPG_RES60_P5_TITLE=1",
           "-DDOOMRPG_RES60_P6_MENU=1"]
    cmd=["cc","-O2","-std=c11","-Wall","-Wextra","-Werror",*flags,
         "-I",str(root/"ESP32/include"),
         str(root/"ESP32/src/esp_res60_title_preview.c"),
         str(root/"ESP32/src/esp_res60_menu.c"),
         str(root/"ESP32/src/esp_main_menu_skin.c"),
         str(c),"-o",str(exe)]
    subprocess.run(cmd,check=True)
    subprocess.run([str(exe)],check=True)
    obj=d/"normal.o"
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                    "-DDOOMRPG_RES60_P6_MENU=0",
                    "-I",str(root/"ESP32/include"),"-c",
                    str(root/"ESP32/src/esp_res60_menu.c"),"-o",str(obj)],check=True)
    assert "EspRes60Menu_render" not in subprocess.check_output(["nm","-g",str(obj)],text=True)
    missing_p5=subprocess.run(["cc","-std=c11","-fsyntax-only",
              "-DDOOMRPG_RES60_P6_MENU=1",
              "-I",str(root/"ESP32/include"),
              str(root/"ESP32/src/esp_res60_menu.c")],
              capture_output=True,text=True)
    assert missing_p5.returncode != 0 and "requires original title P5" in missing_p5.stderr
print("RES60-P6 HOST PASS isolated 192x144 original menu and input")
