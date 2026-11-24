#!/usr/bin/env python3
"""RES60-P23: C11 exact two-viewport layout, 192-column capacity and bad-input oracle.

No gameplay 192x144, PAK, SD, LCD, heap, hardware, or Sprite/HUD admission.
"""
from pathlib import Path
import configparser
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
ESP=ROOT/"ESP32"
cfg=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
assert cfg.read(ESP/"platformio.ini")
pilot=cfg["env:esp32-cyd-res60"]["build_flags"].split()
expected="-D DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY=1"
assert expected in cfg["env:esp32-cyd-res60"]["build_flags"]
assert cfg["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted","env:esp32-cyd-bringup","env:esp32-cyd-trace"):
    assert "DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY" not in cfg[env]["build_flags"],env

header=(ESP/"include/esp_native_raster_contract.h").read_text()
config=(ESP/"include/platform_video_config.h").read_text()
first=(ESP/"src/esp_native_first_frame.c").read_text()
frame=(ESP/"src/esp_native_gameplay_frame.c").read_text()
main=(ESP/"src/main.cpp").read_text()
assert "RES60-P23 requires isolated P21/P18/P1 preview" in config
assert "RES60-P23 raster geometry flag must be 0 or 1" in config
assert "#define ESP_NATIVE_RASTER_COLUMN_CAPACITY" in header
assert "#define MAX_SCREEN_WIDTH ESP_NATIVE_RASTER_COLUMN_CAPACITY" in first
assert "int columnScale[MAX_SCREEN_WIDTH]" in first
assert "render->screenWidth > (int)MAX_SCREEN_WIDTH" in first
assert "#define HUD_ROWS ESP_NATIVE_RASTER_CLASSIC_HUD_ROWS" in frame
assert "render->screenWidth != 160 || render->screenHeight != 80" in frame
assert "start=BSP_PREVIEW_ONLY gameplay=NOT_STARTED" in main
assert "EspNativeFirstFrame_renderGameplayViewport" in frame
assert "shapeData" not in header and "mediaTexels" not in header
assert "malloc(" not in header and "calloc(" not in header and "EspAssetPack" not in header

HARNESS=r"""
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_raster_contract.h"

#ifndef EXPECTED_COLUMN_CAP
#error "test must select explicit column capacity"
#endif
_Static_assert(ESP_NATIVE_RASTER_COLUMN_CAPACITY==EXPECTED_COLUMN_CAP,
               "immutable column scratch must be bounded by build profile");
_Static_assert(ESP_NATIVE_RASTER_CLASSIC_WORLD_WIDTH==160U, "classic");
_Static_assert(ESP_NATIVE_RASTER_RES60_WORLD_WIDTH==192U, "RES60");
_Static_assert(ESP_NATIVE_RASTER_CLASSIC_HUD_ROWS==20U, "classic HUD");
_Static_assert(ESP_NATIVE_RASTER_RES60_HUD_ROWS==24U, "RES60 HUD");
_Static_assert(sizeof(uint16_t)*192U*144U==55296U, "single RGB565");
_Static_assert(sizeof(uint16_t)*160U*120U==38400U, "same prod RGB565");

static uint16_t pixels[192U*144U+2U];
static int cols[194U];
static uint32_t checksum(const uint16_t* p,size_t length) {
    uint32_t h=2166136261U;
    for (size_t i=0;i<length;++i) {
        h=(h^(uint8_t)(p[i]&255U))*16777619U;
        h=(h^(uint8_t)(p[i]>>8U))*16777619U;
    }
    return h;
}
static void trial(EspNativeRasterMode mode, unsigned fw,unsigned fh,
                  unsigned hud,unsigned wh,size_t bytes) {
    EspNativeRasterContract c;
    size_t off=9999U, count=8888U;
    uint32_t fnv;
    const uint16_t cleared=0x6321U, world=0xbeefu;
    assert(EspNativeRasterContract_select(mode,&c));
    assert(c.frameWidth==fw && c.frameHeight==fh &&
           c.pitchPixels==fw && c.worldX==0U && c.worldY==hud &&
           c.worldWidth==fw && c.worldHeight==wh &&
           c.hudRows==hud && c.framebufferBytes==bytes &&
           c.mode==(uint8_t)mode);
    assert(EspNativeRasterContract_validate(&c,bytes));
    assert(!EspNativeRasterContract_validate(&c,bytes-2U));
    assert(!EspNativeRasterContract_validate(&c,bytes+2U));
    for(size_t i=0;i<192U*144U+2U;++i) pixels[i]=cleared;
    pixels[0]=0xa33aU; pixels[192U*144U+1U]=0xb44bU;
    for(size_t i=0;i<194U;++i)cols[i]=0x5f5f5f5f;
    /* Width 192 must not damage either side of the caller-owned column
       scratch. 160 remains exactly the classic profile. */
    for(unsigned x=0;x<c.worldWidth;++x)cols[x+1U]=0x12345678;
    assert(cols[0]==0x5f5f5f5f && cols[193]==0x5f5f5f5f);
    assert(cols[c.worldWidth+1U]==0x5f5f5f5f);

    for(uint16_t row=0;row<c.worldHeight;++row) {
        off=9999U;count=8888U;
        assert(EspNativeRasterContract_worldRow(&c,bytes,row,&off,&count));
        assert(off==((size_t)hud+row)*fw && count==fw);
        for(size_t col=0;col<count;++col)pixels[1U+off+col]=world;
    }
    for(unsigned y=0;y<fh;++y)
      for(unsigned x=0;x<fw;++x)
        assert(pixels[1U+y*fw+x] == ((y>=hud && y<hud+wh)?world:cleared));
    assert(pixels[0]==0xa33aU && pixels[192U*144U+1U]==0xb44bU);

    off=7777U;count=7777U;
    assert(!EspNativeRasterContract_worldRow(&c,bytes,c.worldHeight,&off,&count));
    assert(off==7777U && count==7777U);
    assert(!EspNativeRasterContract_worldRow(&c,bytes-2U,0U,&off,&count));
    assert(!EspNativeRasterContract_worldRow(&c,bytes,0U,NULL,&count));
    assert(!EspNativeRasterContract_worldRow(&c,bytes,0U,&off,NULL));
    assert(off==7777U && count==7777U);
    c.pitchPixels++;
    assert(!EspNativeRasterContract_validate(&c,bytes));
    c.pitchPixels--;
    c.framebufferBytes-=2U;
    assert(!EspNativeRasterContract_validate(&c,bytes));
    c.framebufferBytes+=2U;
    c.worldHeight++;
    assert(!EspNativeRasterContract_validate(&c,bytes));
    c.worldHeight--;
    c.worldY++;
    assert(!EspNativeRasterContract_validate(&c,bytes));
    c.worldY--;
    c.worldWidth--;
    assert(!EspNativeRasterContract_validate(&c,bytes));
    c.worldWidth++;
    c.mode=255U;
    assert(!EspNativeRasterContract_validate(&c,bytes));
    fnv=checksum(pixels+1U,bytes/2U);
    printf("mode=%d frame=%ux%u world=%ux%u@y%u bytes=%lu fnv=%08lx\n",
           (int)mode,fw,fh,fw,wh,hud,(unsigned long)bytes,(unsigned long)fnv);
}
int main(void) {
    EspNativeRasterContract c;
    assert(!EspNativeRasterContract_select((EspNativeRasterMode)2,&c));
    assert(!EspNativeRasterContract_select(ESP_NATIVE_RASTER_CLASSIC,NULL));
    assert(!EspNativeRasterContract_validate(NULL,38400U));
    assert(!EspNativeRasterContract_worldRow(NULL,38400U,0U,NULL,NULL));
    trial(ESP_NATIVE_RASTER_CLASSIC,160U,120U,20U,80U,38400U);
    trial(ESP_NATIVE_RASTER_RES60,192U,144U,24U,96U,55296U);
    puts("RES60-P23 real C11 raster contract PASS: classic and 192 columns, pitch, HUD canaries and guards");
    return 0;
}
"""
cc=["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
    "-I",str(ESP/"include")]
defs=["-D"+token for token in pilot if token.startswith("DOOMRPG_RES60_")]
assert "-DDOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY=1" in defs
with tempfile.TemporaryDirectory(prefix="res60-p23-") as d:
    folder=Path(d)
    src=folder/"test.c";src.write_text(HARNESS)
    for label,flags,cap in (
        ("classic",[],160),
        ("res60",defs,192),
    ):
        exe=folder/label
        subprocess.run(cc+[f"-DEXPECTED_COLUMN_CAP={cap}"]+flags+
                       [str(src),"-o",str(exe)],check=True)
        run=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
        assert "mode=0 frame=160x120 world=160x80@y20 bytes=38400" in run.stdout
        assert "mode=1 frame=192x144 world=192x96@y24 bytes=55296" in run.stdout
        assert "RES60-P23 real C11 raster contract PASS" in run.stdout
    for bad,word in (
        (["-DDOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY=1"],
         "RES60-P23 requires isolated P21/P18/P1 preview"),
        (["-DDOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY=2"],
         "RES60-P23 raster geometry flag must be 0 or 1"),
    ):
        result=subprocess.run(cc+bad+["-fsyntax-only",str(src)],
                              capture_output=True,text=True)
        assert result.returncode and word in result.stderr,(bad,result.stderr)
print("RES60-P23 HOST PASS: two compiled profiles 160/192, exact framebuffer spans, column scratch 160/192, strict fail-closed, no extra heap")
