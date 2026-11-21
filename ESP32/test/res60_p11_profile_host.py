#!/usr/bin/env python3
"""RES60-P11: real P10 C11 raster parity plus P8 PAK/cache timing witnesses."""
from pathlib import Path
import configparser
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ini = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
assert ini.read(ROOT / "ESP32/platformio.ini")
flag = "-D DOOMRPG_RES60_P11_PROFILE=1"
assert flag in ini["env:esp32-cyd-res60"]["build_flags"]
assert ini["env:esp32-cyd-res60-inverted"]["extends"] == "env:esp32-cyd-res60"
for env in ("env:esp32-cyd", "env:esp-cyd-inverted"):
    assert "DOOMRPG_RES60_P11_PROFILE" not in ini[env]["build_flags"]

config = (ROOT/"ESP32/include/platform_video_config.h").read_text()
source = (ROOT/"ESP32/src/esp_res60_world_preview.c").read_text()
texture = (ROOT/"ESP32/src/esp_res60_texture_probe.c").read_text()
world = (ROOT/"ESP32/src/esp_res60_world_probe.c").read_text()
video = (ROOT/"ESP32/src/platform_video.cpp").read_text()
for symbol in ("preflightUs", "raysUs", "geometryUs", "fillUs", "hashUs"):
    assert symbol in source
for symbol in ("inventoryUs", "loadUs", "p3Us", "textureUs", "releaseUs"):
    assert symbol in world
for symbol in ("profileCatalogUs", "profilePackUs", "profileSwatchUs",
               "profileOverlayUs", "profileTotalUs"):
    assert symbol in texture
assert "[RES60P11] PROFILE" in video
assert "instrumentation=ON\\n" in video
assert "RES60-P11 requires isolated P10/P9" in config
assert "micros()" in video and 'extern "C" uint32_t EspRes60Profile_nowUs' in video

base_flags = [
    "-DDOOMRPG_RES60_P1_DIAGNOSTIC=1",
    "-DDOOMRPG_RES60_P2_LAYOUT=1",
    "-DDOOMRPG_RES60_P3_WORLD=1",
    "-DDOOMRPG_RES60_P5_TITLE=1",
    "-DDOOMRPG_RES60_P6_MENU=1",
    "-DDOOMRPG_RES60_P7_NAV=1",
    "-DDOOMRPG_RES60_P8_TEXEL=1",
    "-DDOOMRPG_RES60_P9_WEDGE=1",
    "-DDOOMRPG_RES60_P10_WALL_MATERIAL=1",
]
cc = ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
      "-I", str(ROOT/"ESP32/include")]
preview = ROOT/"ESP32/src/esp_res60_world_preview.c"
material = ROOT/"ESP32/src/esp_res60_wall_material.c"
p8 = ROOT/"ESP32/src/esp_res60_texture_probe.c"

def extract_harness(filename):
    text = (ROOT/"ESP32/test"/filename).read_text()
    begin = text.index('harness=r"""') if 'harness=r"""' in text else text.index('harness = r"""')
    begin = text.index('"""', begin)+3
    end = text.index('"""', begin)
    return text[begin:end]

clock = r'''
#include "esp_res60_profile.h"
/* Wrap a 32-bit monotonic timer repeatedly, without relying on host walltime. */
static uint32_t fakeTick=UINT32_MAX-120U;
uint32_t EspRes60Profile_nowUs(void) {fakeTick+=37U;return fakeTick;}
'''

with tempfile.TemporaryDirectory(prefix="res60-p11-") as folder:
    d = Path(folder)
    original = extract_harness("res60_p10_material_host.py")
    anchor = "   assert(EspRes60Preview_render(f,55296,&cam,4,readLine,0,&stats));"
    assert original.count(anchor)==1
    p11 = original.replace('#include "esp_res60_wall_material.h"',
                           '#include "esp_res60_wall_material.h"\n'+clock)
    p11 = p11.replace(anchor, anchor + r'''
   printf("P3RAW %u %08lx\n", turn, (unsigned long)stats.frameFNV);
#if DOOMRPG_RES60_P11_PROFILE
   assert(stats.preflightUs && stats.bandUs && stats.raysUs);
   assert(stats.geometryUs && stats.fillUs && stats.hashUs);
   assert(stats.rayTests==stats.wedgeCandidates*192U);
#endif
''')
    out = []
    for enabled in (0, 1):
        fixture = d/("preview%d.c"%enabled)
        executable = d/("preview%d"%enabled)
        fixture.write_text(p11)
        subprocess.run(cc+base_flags+
                       ["-DDOOMRPG_RES60_P11_PROFILE=%d"%enabled,
                        str(preview),str(material),str(fixture),
                        "-o",str(executable)],check=True)
        result=subprocess.run([str(executable)],check=True,
                              capture_output=True,text=True)
        assert "RES60-P10 PASS" in result.stdout
        lines=[line for line in result.stdout.splitlines()
               if line.startswith("P3RAW ")]
        assert len(lines)==4
        out.append(lines)
    assert out[0]==out[1], (out[0],out[1])

    p8_fixture = extract_harness("res60_p10_visible_source_host.py")
    assert p8_fixture.count('#include "esp_res60_wall_material.h"')==1
    p8_fixture=p8_fixture.replace('#include "esp_res60_wall_material.h"',
                                  '#include "esp_res60_wall_material.h"\n'+clock)
    anchor='    assert(result.logicalId==7 && result.selectedFromVisible==1);'
    assert p8_fixture.count(anchor)==1
    p8_fixture=p8_fixture.replace(anchor,anchor+r'''
    assert(result.profileCatalogUs && result.profilePackUs);
    assert(result.profileSwatchUs && result.profileOverlayUs);
    assert(result.profileTotalUs);
''')
    anchor='    assert(result.selectedFromVisible==1 && result.visibleWallColumns==0);'
    assert p8_fixture.count(anchor)==1
    p8_fixture=p8_fixture.replace(anchor,anchor+r'''
    assert(result.profileCatalogUs==0 && result.profilePackUs==0);
    assert(result.profileSwatchUs && result.profileOverlayUs);
    assert(result.profileTotalUs);
''')
    fixture=d/"source.c";exe=d/"texture"
    fixture.write_text(p8_fixture)
    subprocess.run(cc+base_flags+["-DDOOMRPG_RES60_P11_PROFILE=1",
        str(p8),str(material),str(fixture),"-o",str(exe)],check=True)
    result=subprocess.run([str(exe)],check=True,
                          capture_output=True,text=True)
    assert "RES60-P10 VISIBLE SOURCE C11 PASS" in result.stdout

    # The 160x120 path cannot expose P11 timing, even with caller override.
    no_timing=d/"no_timing.o"
    subprocess.run(cc+["-DDOOMRPG_RES60_P11_PROFILE=0",
        "-c",str(preview),"-o",str(no_timing)],check=True)
    assert "EspRes60Profile_nowUs" not in subprocess.check_output(
        ["nm","-u",str(no_timing)],text=True)
    invalid=subprocess.run(cc+["-DDOOMRPG_RES60_P11_PROFILE=1",
        "-fsyntax-only",str(preview)],capture_output=True,text=True)
    assert invalid.returncode!=0 and \
           "RES60-P11 requires isolated P10/P9" in invalid.stderr

print("RES60-P11 HOST PASS: P3 P10/P11 four-angle FNV/pixel parity, no production clock, P8 cold/warm phase counters, 32-bit micros rollover")
