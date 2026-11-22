#!/usr/bin/env python3
"""RES60-P16: heap region observer only, no change to the real P15 C ray/UV owners."""
from pathlib import Path
import configparser, hashlib, subprocess, tempfile

ROOT=Path(__file__).resolve().parents[2]
ini=configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
assert ini.read(ROOT/"ESP32/platformio.ini")
pilot=ini["env:esp32-cyd-res60"]
assert "-D DOOMRPG_RES60_P15_COMPACT_SNAPSHOT=1" in pilot["build_flags"]
assert "-D DOOMRPG_RES60_P16_HEAP_TOPOLOGY=1" in pilot["build_flags"]
assert ini["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted","env:esp32-cyd-bringup","env:esp32-cyd-trace"):
    assert "DOOMRPG_RES60_P16_HEAP_TOPOLOGY" not in ini[env]["build_flags"]

config=(ROOT/"ESP32/include/platform_video_config.h").read_text()
video=(ROOT/"ESP32/src/platform_video.cpp").read_text()
ray=(ROOT/"ESP32/src/esp_res60_world_preview.c").read_text()
mat=(ROOT/"ESP32/src/esp_res60_wall_material.c").read_text()
assert "RES60-P16 requires isolated P15/P13/P11 immutable preview" in config
assert "DOOMRPG_RES60_P16_HEAP_TOPOLOGY" not in ray+mat
assert "heap_caps_get_info(&p16BeforeFramebuffer,MALLOC_CAP_8BIT)" in video
assert "heap_caps_get_info(&p16AfterFramebuffer,MALLOC_CAP_8BIT)" in video
assert "res60P16_printHeapTopology(\"framebuffer-ready\")" in video
assert "res60P16_printHeapTopology(\"first-map-hit\")" in video
assert "heap_caps_print_heap_info(MALLOC_CAP_8BIT)" in video
assert "if (!res60P16WarmDumped && report.mapLeaseHit)" in video
assert "neutral=%u" in video
assert video.index("const uint32_t preparedUs=micros()-started;") < video.index('res60P16_printHeapTopology("first-map-hit")')
assert video.index('Serial.printf("[RES60P7] FRAME') < video.index('res60P16_printHeapTopology("first-map-hit")')
assert video.count('res60P16_printHeapTopology("first-map-hit")')==1
assert "static uint8_t res60P16WarmDumped=0U" in video
assert "malloc(" not in video[video.index("static void res60P16_printHeapTopology"):video.index("#endif",video.index("static void res60P16_printHeapTopology"))]
assert "calloc(" not in video[video.index("static void res60P16_printHeapTopology"):video.index("#endif",video.index("static void res60P16_printHeapTopology"))]

CC=["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
    "-I",str(ROOT/"ESP32/include")]
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
    "-DDOOMRPG_RES60_P13_MAP_LEASE=1",
    "-DDOOMRPG_RES60_P14_LINE_SNAPSHOT=1",
    "-DDOOMRPG_RES60_P15_COMPACT_SNAPSHOT=1",
]
with tempfile.TemporaryDirectory(prefix="res60-p16-") as temp:
    temp=Path(temp)
    for name in ("esp_res60_world_preview.c","esp_res60_wall_material.c"):
        source=str(ROOT/"ESP32/src"/name)
        outputs=[]
        for mode in (0,1):
            obj=temp/(name+str(mode)+".o")
            subprocess.run(CC+flags+[f"-DDOOMRPG_RES60_P16_HEAP_TOPOLOGY={mode}",
                         "-c",source,"-o",str(obj)],check=True)
            outputs.append(hashlib.sha256(obj.read_bytes()).hexdigest())
        assert outputs[0]==outputs[1],f"P16 altered real C owner: {name}"
    bad=subprocess.run(CC+["-DDOOMRPG_RES60_P16_HEAP_TOPOLOGY=1",
                           "-fsyntax-only",str(ROOT/"ESP32/src/esp_res60_world_preview.c")],
                       capture_output=True,text=True)
    assert bad.returncode and "requires isolated P15/P13/P11" in bad.stderr
    prod=temp/"prod.o"
    subprocess.run(CC+["-DDOOMRPG_RES60_P3_WORLD=0",
                       "-c",str(ROOT/"ESP32/src/esp_res60_world_preview.c"),
                       "-o",str(prod)],check=True)
    assert "EspRes60Preview_render" not in subprocess.check_output(["nm","-g",str(prod)],text=True)
print("RES60-P16 HOST PASS: read-only bounded heap observer, exact P15 ray/material objects, strict production isolation")
