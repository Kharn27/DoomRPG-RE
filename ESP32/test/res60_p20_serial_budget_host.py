#!/usr/bin/env python3
"""P20: exercise the actual pure C warm/cold Serial policy and isolation gates."""
from pathlib import Path
import configparser
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
cfg=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
assert cfg.read(root/"ESP32/platformio.ini")
flag="-D DOOMRPG_RES60_P20_COMPACT_SERIAL=1"
assert flag in cfg["env:esp32-cyd-res60"]["build_flags"]
assert cfg["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd", "env:esp-cyd-inverted", "env:esp32-cyd-bringup", "env:esp32-cyd-trace"):
    assert flag not in cfg[env]["build_flags"]
config=(root/"ESP32/include/platform_video_config.h").read_text()
policy=(root/"ESP32/include/esp_res60_serial_policy.h").read_text()
video=(root/"ESP32/src/platform_video.cpp").read_text()
assert 'RES60-P20 requires isolated P18/P13/P11 preview' in config
assert '#if DOOMRPG_RES60_P20_COMPACT_SERIAL' in video
assert "EspRes60Serial_compactWarm(" in video
assert "report.mapLeaseHit" in video and "report.texture.cacheHit" in video
assert "[RES60P20] WARM angle=%u turns=%u" in video
for key in ("rawP3FNV=%08lx", "fnv=%08lx", "p10FNV=%08lx",
            "swatchFNV=%08lx", "raysUs=%lu", "preparedUs=%lu",
            "heap8=%u", "largest8=%u", "minHeap8=%u"):
    assert key in video
assert '[RES60P17] COMPACT' in video and '[RES60P11] PROFILE' in video
assert '[RES60P7] FRAME totalUs' in video and '[RES60P7] RASTER' in video
assert "PlatformVideo_present()" in video
assert "malloc" not in policy and "static inline" in policy
source=r"""
#include <assert.h>
#include <stdint.h>
#include "esp_res60_serial_policy.h"
int main(void) {
#if DOOMRPG_RES60_P20_COMPACT_SERIAL
  assert(EspRes60Serial_compactWarm(1,1)==1);
  assert(EspRes60Serial_compactWarm(1,0)==0);
  assert(EspRes60Serial_compactWarm(0,1)==0);
  assert(EspRes60Serial_compactWarm(0,0)==0);
  assert(EspRes60Serial_compactWarm(2,1)==0);
  assert(EspRes60Serial_compactWarm(1,2)==0);
#endif
  return 0;
}
"""
flags=["DOOMRPG_RES60_P1_DIAGNOSTIC", "DOOMRPG_RES60_P2_LAYOUT",
       "DOOMRPG_RES60_P3_WORLD", "DOOMRPG_RES60_P5_TITLE",
       "DOOMRPG_RES60_P6_MENU", "DOOMRPG_RES60_P7_NAV",
       "DOOMRPG_RES60_P8_TEXEL", "DOOMRPG_RES60_P9_WEDGE",
       "DOOMRPG_RES60_P10_WALL_MATERIAL","DOOMRPG_RES60_P11_PROFILE",
       "DOOMRPG_RES60_P12_INVENTORY_CACHE",
       "DOOMRPG_RES60_P13_MAP_LEASE",
       "DOOMRPG_RES60_P14_LINE_SNAPSHOT",
       "DOOMRPG_RES60_P15_COMPACT_SNAPSHOT",
       "DOOMRPG_RES60_P16_HEAP_TOPOLOGY",
       "DOOMRPG_RES60_P17_HEAP_SNAPSHOT",
       "DOOMRPG_RES60_P18_LOCAL_SNAPSHOT"]
cc=["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
    "-I",str(root/"ESP32/include")]
with tempfile.TemporaryDirectory(prefix="res60-p20-") as p:
    temp=Path(p); h=temp/"test.c";h.write_text(source)
    for enabled in (0,1):
        exe=temp/str(enabled)
        f=["-D"+key+"=1" for key in flags] if enabled else []
        subprocess.run(cc+f+[f"-DDOOMRPG_RES60_P20_COMPACT_SERIAL={enabled}",
            str(h),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    invalid=subprocess.run(cc+["-DDOOMRPG_RES60_P20_COMPACT_SERIAL=1",
        "-fsyntax-only",str(h)],capture_output=True,text=True)
    assert invalid.returncode!=0
    assert "RES60-P20 requires isolated P18/P13/P11 preview" in invalid.stderr
    invalid=subprocess.run(cc+["-DDOOMRPG_RES60_P20_COMPACT_SERIAL=2",
        "-fsyntax-only",str(h)],capture_output=True,text=True)
    assert invalid.returncode!=0
    assert "RES60-P20 compact serial flag must be 0 or 1" in invalid.stderr
print("RES60-P20 HOST PASS: warm-only compact output, cold verbose and production isolation")
