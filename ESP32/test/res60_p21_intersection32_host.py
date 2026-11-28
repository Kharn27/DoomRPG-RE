#!/usr/bin/env python3
"""P21: actual C11 P18 heap raycast versus P15 64-bit 261-frame UV oracle."""
from pathlib import Path
import configparser
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
ini=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
assert ini.read(ROOT/"ESP32/platformio.ini")
flag="-D DOOMRPG_RES60_P21_INTERSECTION32=1"
assert flag in ini["env:esp32-cyd-res60"]["build_flags"]
assert ini["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted","env:esp32-cyd-bringup","env:esp32-cyd-trace"):
    assert flag not in ini[env]["build_flags"]
config=(ROOT/"ESP32/include/platform_video_config.h").read_text()
source=(ROOT/"ESP32/src/esp_res60_world_preview.c").read_text()
assert "RES60-P21 requires isolated P20/P18/P15 preview" in config
assert "RES60-P21 intersection32 flag must be 0 or 1" in config
assert "#if DOOMRPG_RES60_P21_INTERSECTION32" in source
assert "const int32_t sx" in source and "int32_t den" in source
assert "const int64_t sx" in source and "int64_t den" in source
assert "num * bestDen < bestNum * den" in source  # retain exact 64-bit tie/nearest
assert "(uint32_t)num * 192U" in source
assert "(uint32_t)PREVIEW_FAR * (uint32_t)den" in source
assert "PREVIEW_MAP_SIZE = 2048" in source
assert "PREVIEW_FAR = 4096" in source
assert "PREVIEW_NEAR = 4" in source
# Strict upper bounds, including endpoint x=2048 and camera x=0.
MAX=2048
assert 2*MAX*MAX <= 2**31-1
assert 2*MAX*MAX*192 <= 2**31-1
assert 383*MAX*4096 <= 2**32-1

cc=["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
    "-I",str(ROOT/"ESP32/include")]
projector=str(ROOT/"ESP32/src/esp_res60_world_preview.c")
with tempfile.TemporaryDirectory(prefix="res60-p21-gates-") as directory:
    for value,diagnostic in (("1","RES60-P21 requires isolated P20/P18/P15 preview"),
                             ("2","RES60-P21 intersection32 flag must be 0 or 1")):
        r=subprocess.run(cc+[f"-DDOOMRPG_RES60_P21_INTERSECTION32={value}",
                             "-fsyntax-only",projector],
                         capture_output=True,text=True)
        assert r.returncode!=0 and diagnostic in r.stderr, (value,r.stderr)
    obj=Path(directory)/"prod.o"
    subprocess.run(cc+["-DDOOMRPG_RES60_P3_WORLD=0",
                       "-c",projector,"-o",str(obj)],check=True)
    assert "EspRes60Preview_render" not in subprocess.check_output(
        ["nm","-g",str(obj)],text=True)
# Reuse genuine C11 P18/P15 test with P21 enabled only for the P17 heap
# candidate, including 261 exact full-frame byte/pixel/UV comparisons.
p18=str(ROOT/"ESP32/test/res60_p18_local_snapshot_host.py")
r=subprocess.run([sys.executable,p18,"--p21"],check=True,
                 capture_output=True,text=True)
assert "RES60-P21 INTERSECTION32 HOST PASS" in r.stdout
assert "261 full-frame UV parity" in r.stdout
print("RES60-P21 HOST PASS: 261 C11 full-frame 64/32-bit parity, UV/clip/ties/canaries, single heap owner, exact overflow proofs, production gated")
