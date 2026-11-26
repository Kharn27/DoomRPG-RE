#!/usr/bin/env python3
"""RES60-P4: same genuine MAP_INTRO P3 preview, alternate CYD LCD polarity only.

Configuration/host admission tests do not establish colorimetric CYD fidelity.
"""
from pathlib import Path
import configparser
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
cfg = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
assert cfg.read(root / "ESP32/platformio.ini")
assert cfg["platformio"]["default_envs"] == "esp32-cyd"
base = cfg["env:esp32-cyd"]
normal_inv = cfg["env:esp-cyd-inverted"]
pilot = cfg["env:esp32-cyd-res60"]
p4 = cfg["env:esp32-cyd-res60-inverted"]
assert normal_inv["extends"] == "env:esp32-cyd"
assert pilot["extends"] == "env:esp32-cyd"
assert p4["extends"] == "env:esp32-cyd-res60"
pilot_flags = pilot["build_flags"].split()
p4_flags = p4["build_flags"].split()
assert pilot_flags == [
    "${env:esp32-cyd.build_flags}",
    "-D", "DOOMRPG_RES60_P1_DIAGNOSTIC=1",
    "-D", "DOOMRPG_RES60_P2_LAYOUT=1",
    "-D", "DOOMRPG_RES60_P3_WORLD=1",
    "-D", "DOOMRPG_RES60_P5_TITLE=1",
    "-D", "DOOMRPG_RES60_P6_MENU=1",
    "-D", "DOOMRPG_RES60_P7_NAV=1",
    "-D", "DOOMRPG_RES60_P8_TEXEL=1",
    "-D", "DOOMRPG_RES60_P9_WEDGE=1",
    "-D", "DOOMRPG_RES60_P10_WALL_MATERIAL=1",
    "-D", "DOOMRPG_RES60_P11_PROFILE=1",
    "-D", "DOOMRPG_RES60_P12_INVENTORY_CACHE=1",
    "-D", "DOOMRPG_RES60_P13_MAP_LEASE=1",
    "-D", "DOOMRPG_RES60_P14_LINE_SNAPSHOT=1",
    "-D", "DOOMRPG_RES60_P15_COMPACT_SNAPSHOT=1",
    "-D", "DOOMRPG_RES60_P16_HEAP_TOPOLOGY=1",
    "-D", "DOOMRPG_RES60_P17_HEAP_SNAPSHOT=1",
    "-D", "DOOMRPG_RES60_P18_LOCAL_SNAPSHOT=1",
    "-D", "DOOMRPG_RES60_P20_COMPACT_SERIAL=1",
    "-D", "DOOMRPG_RES60_P21_INTERSECTION32=1",
    "-D", "DOOMRPG_RES60_P23_NATIVE_RASTER_GEOMETRY=1",
    "-D", "DOOMRPG_RES60_P25_TEXTURED_PLANES=1",
    "-D", "DOOMRPG_RES60_P26_BATCH_INDEX=1",
    "-I.pio/libdeps/esp32-cyd-res60/TFT_eSPI",
]
assert p4_flags == [
    "${env:esp32-cyd-res60.build_flags}",
    "-D", "DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION=1",
    "-I.pio/libdeps/esp32-cyd-res60-inverted/TFT_eSPI",
]
assert "DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION" not in " ".join(pilot_flags)
assert "DOOMRPG_RES60_" not in " ".join(base["build_flags"].split())
assert "DOOMRPG_RES60_" not in " ".join(normal_inv["build_flags"].split())
assert "DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION=1" in normal_inv["build_flags"]

video = (root / "ESP32/src/platform_video.cpp").read_text()
main = (root / "ESP32/src/main.cpp").read_text()
config = (root / "ESP32/include/platform_video_config.h").read_text()
preview = (root / "ESP32/src/esp_res60_world_preview.c").read_text()
assert "#if DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION" in video
assert "platformDisplay->invertDisplay(false);" in video
assert "platformDisplay->invertDisplay(true);" in video
assert "gamma=ILI9341-driver-E0E1-trial" in video
assert "gamma=classic-E0-driver-E1-trial" in video
assert "setSwapBytes(true)" in video
assert 'Serial.println("[RES60P3] BOOT mode=INTRO_GEOMETRY_ONLY' in main
assert "PlatformVideo_showRes60WorldPreview();" in main
assert "#if DOOMRPG_RES60_P3_WORLD" in preview
assert "shapeData" not in preview and "mediaTexels" not in preview
assert "DOOMRPG_LOGICAL_WIDTH 160" in config
assert "DOOMRPG_LOGICAL_HEIGHT 120" in config

source = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "platform_video_config.h"
#include "esp_res60_layout.h"
#include "esp_video_nearest.h"
_Static_assert(DOOMRPG_RES60_P1_DIAGNOSTIC==1, "P4 requires 192x144");
_Static_assert(DOOMRPG_RES60_P2_LAYOUT==1, "P4 requires 24/96/24");
_Static_assert(DOOMRPG_RES60_P3_WORLD==1, "P4 uses same P3 raycaster");
_Static_assert(DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION==1,
               "P4 differs only by panel gamma/inversion flag");
_Static_assert(DOOMRPG_VIDEO_BUFFER_WIDTH==192, "same P3 framebuffer width");
_Static_assert(DOOMRPG_VIDEO_BUFFER_HEIGHT==144, "same P3 framebuffer height");
_Static_assert(ESP_RES60_FRAMEBUFFER_BYTES==55296, "single 55,296B RGB565");
_Static_assert(ESP_RES60_WORLD_ROWS==96, "192x96 world");
_Static_assert(DOOMRPG_LOGICAL_WIDTH==160 && DOOMRPG_LOGICAL_HEIGHT==120,
               "production geometry immutable");
int main(void) {
    assert(EspVideoNearest_source(319,192,320)==191);
    assert(EspVideoNearest_source(239,144,240)==143);
    for (int y=0; y<144; ++y) {
        EspRes60Region expected=y<24?ESP_RES60_TOP_HUD:
                     y<120?ESP_RES60_WORLD:ESP_RES60_BOTTOM_HUD;
        assert(EspRes60Layout_regionAt(0,y)==expected);
    }
    puts("RES60-P4 profile geometry PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="res60-p4-") as d:
    c_path = Path(d) / "profile.c"
    elf = Path(d) / "profile"
    c_path.write_text(source)
    subprocess.run([
        "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
        "-DDOOMRPG_RES60_P2_LAYOUT=1",
        "-DDOOMRPG_RES60_P3_WORLD=1",
        "-DDOOMRPG_CYD_ALTERNATE_PANEL_INVERSION=1",
        "-I", str(root / "ESP32/include"), str(c_path), "-o", str(elf)
    ], check=True)
    subprocess.run([str(elf)], check=True)
print("RES60-P4 HOST PASS: P3 map geometry unchanged, panel inversion isolated")
