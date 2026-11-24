#!/usr/bin/env python3
"""RES60-P24: compiled real C11 viewport preparation, not 192 gameplay.

Strictly preserves the permanent classic renderer's pixel origin, frame size
and no-extra-buffer ownership. Laboratory 192 is mathematical PREPARATION ONLY:
no PAK, planes, BSP walk, sprites, LCD or gameplay is entered.
"""
import configparser
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ESP = ROOT / "ESP32"
cfg = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
assert cfg.read(ESP / "platformio.ini")
res60 = cfg["env:esp32-cyd-res60"]["build_flags"]
assert "-D DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY=1" in res60
assert cfg["env:esp32-cyd-res60-inverted"]["extends"] == "env:esp32-cyd-res60"
for env in ("env:esp32-cyd", "env:esp-cyd-inverted",
            "env:esp32-cyd-bringup", "env:esp32-cyd-trace"):
    assert "DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY" not in cfg[env]["build_flags"]

header = (ESP / "include/esp_native_raster_contract.h").read_text()
world = (ESP / "src/esp_native_first_frame.c").read_text()
plane = (ESP / "src/esp_native_plane_renderer.c").read_text()
frame = (ESP / "src/esp_native_gameplay_frame.c").read_text()
main = (ESP / "src/main.cpp").read_text()
assert "EspNativeRasterContract_prepareWorld(" in header
assert "EspNativeRasterWorldViewport worldViewport;" in world
assert "EspNativeRasterContract_prepareWorld(" in world
assert "worldViewport.worldFirstPixel * sizeof(uint16_t)" in world
assert "viewport->worldFirstPixel + y * pitchPixels" in world
assert "ESP_NATIVE_RASTER_CLASSIC," in world
assert "render->screenWidth != 160 || render->screenHeight != 80" in plane
assert "render->screenWidth != 160 || render->screenHeight != 80" in frame
assert "start=BSP_PREVIEW_ONLY gameplay=NOT_STARTED" in main
assert "shapeData" not in header and "mediaTexels" not in header
assert "malloc(" not in header and "EspAssetPack" not in header

HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_raster_contract.h"

#ifndef EXPECTED_CAP
#error explicit capacity needed
#endif
_Static_assert(ESP_NATIVE_RASTER_COLUMN_CAPACITY == EXPECTED_CAP, "profile");
static uint16_t framebuffer[192U * 144U + 2U];
static uint16_t reference[192U * 144U + 2U];

static uint32_t fnv(const uint16_t* p, size_t n) {
    uint32_t h = 2166136261U;
    for (size_t i = 0; i < n; ++i) {
        h = (h ^ (uint8_t)p[i]) * 16777619U;
        h = (h ^ (uint8_t)(p[i] >> 8)) * 16777619U;
    }
    return h;
}
static void rejection(EspNativeRasterMode mode, size_t bytes, int pitch,
                      int width, int height, unsigned cap) {
    EspNativeRasterWorldViewport out, untouched;
    memset(&out, 0xA5, sizeof(out));
    untouched = out;
    assert(!EspNativeRasterContract_prepareWorld(mode, bytes, pitch,
                                                  width, height, cap, &out));
    assert(memcmp(&out, &untouched, sizeof(out)) == 0);
}
static void trial(EspNativeRasterMode mode, unsigned fw, unsigned fh,
                  unsigned hud, unsigned wh, size_t bytes) {
    EspNativeRasterWorldViewport out;
    size_t firstPixel = (size_t)hud * fw;
    unsigned cap = EXPECTED_CAP;
    memset(&out, 0, sizeof(out));
    assert(EspNativeRasterContract_prepareWorld(
        mode, bytes, (int)(fw * 2U), (int)fw, (int)wh, cap, &out));
    assert(out.worldFirstPixel == firstPixel);
    assert(out.raster.pitchPixels == fw);
    assert(out.raster.worldWidth == fw && out.raster.worldHeight == wh);
    assert(out.raster.hudRows == hud);
    assert(out.raster.frameWidth == fw && out.raster.frameHeight == fh);
    assert(out.raster.framebufferBytes == bytes);
    for (size_t i = 0; i < 192U*144U+2U; ++i) {
        framebuffer[i] = 0x5a5aU;
        reference[i] = 0x5a5aU;
    }
    framebuffer[0] = reference[0] = 0x1234U;
    framebuffer[192U*144U+1U] = reference[192U*144U+1U] = 0xbeefU;
    for (unsigned y=0; y<wh; ++y) {
        size_t offset=9999U, count=9999U;
        assert(EspNativeRasterContract_worldRow(&out.raster,bytes,
                                                  (uint16_t)y,&offset,&count));
        assert(offset == out.worldFirstPixel + (size_t)y*fw);
        assert(count == fw);
        for (unsigned x=0; x<count; ++x) {
            const uint16_t pixel = (uint16_t)(0x9000U |
                    ((uint16_t)((y*223U+x*17U)&0x0fffU)));
            framebuffer[1U+offset+x] = pixel;
        }
    }
    for (unsigned y=0; y<fh; ++y) for (unsigned x=0; x<fw; ++x) {
        if(y>=hud && y<hud+wh)
            reference[1U+(size_t)y*fw+x] = (uint16_t)
               (0x9000U | (((y-hud)*223U+x*17U)&0x0fffU));
    }
    assert(memcmp(framebuffer,reference,sizeof(framebuffer))==0);
    assert(framebuffer[0]==0x1234U &&
           framebuffer[192U*144U+1U]==0xbeefU);
    assert(fnv(framebuffer+1U,bytes/2U)==fnv(reference+1U,bytes/2U));

    rejection(mode, bytes-2U, (int)(fw*2U), (int)fw, (int)wh, cap);
    rejection(mode, bytes+2U, (int)(fw*2U), (int)fw, (int)wh, cap);
    rejection(mode, bytes, (int)(fw*2U)-2, (int)fw, (int)wh, cap);
    rejection(mode, bytes, (int)(fw*2U)+2, (int)fw, (int)wh, cap);
    rejection(mode, bytes, (int)(fw*2U), (int)fw-1, (int)wh, cap);
    rejection(mode, bytes, (int)(fw*2U), (int)fw, (int)wh-1, cap);
    rejection(mode, bytes, (int)(fw*2U), (int)fw, (int)wh, fw-1U);
    rejection(mode, bytes, (int)(fw*2U), (int)fw, (int)wh, cap+1U);
    printf("viewport mode=%u %ux%u world=%ux%u origin=%lu FNV=%08lx\n",
           (unsigned)mode,fw,fh,fw,wh,(unsigned long)firstPixel,
           (unsigned long)fnv(framebuffer+1U,bytes/2U));
}
int main(void) {
    trial(ESP_NATIVE_RASTER_CLASSIC,160U,120U,20U,80U,38400U);
#if EXPECTED_CAP == 192
    trial(ESP_NATIVE_RASTER_RES60,192U,144U,24U,96U,55296U);
#else
    rejection(ESP_NATIVE_RASTER_RES60,55296U,384,192,96,192U);
#endif
    rejection((EspNativeRasterMode)2,38400U,320,160,80,160U);
    assert(!EspNativeRasterContract_prepareWorld(
        ESP_NATIVE_RASTER_CLASSIC,38400U,320,160,80,160U,NULL));
    puts("RES60-P24 PASS: exact C11 world origin, 160/192, pitch, full-frame canaries");
    return 0;
}
"""
cc = ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
      "-I", str(ESP/"include")]
defs = ["-D"+x for x in res60.split() if x.startswith("DOOMRPG_RES60_")]
with tempfile.TemporaryDirectory(prefix="res60-p24-") as tmp:
    source = Path(tmp)/"p24.c"
    source.write_text(HARNESS)
    for name, flags, capacity in (("classic", [], 160),
                                  ("res60", defs, 192)):
        exe = Path(tmp)/name
        subprocess.run(cc+[f"-DEXPECTED_CAP={capacity}"]+flags+
                       [str(source),"-o",str(exe)], check=True)
        result = subprocess.run([str(exe)],check=True,
                                capture_output=True,text=True)
        assert "RES60-P24 PASS" in result.stdout
        assert result.stdout.count("viewport mode=") == (2 if capacity==192 else 1)
        print(result.stdout.strip())
print("RES60-P24 HOST PASS: real immutable C11 preflight; no 192 gameplay activated")
