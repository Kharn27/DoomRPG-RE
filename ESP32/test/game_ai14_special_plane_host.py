#!/usr/bin/env python3
"""Compile original Game_trace and actual native continuation trace together."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
src=(ROOT/"src/Game.c").read_text(encoding="latin-1")
anchor="void Game_trace(Game_t* game,"
assert src.count(anchor)==1
start=src.index(anchor)
depth=0
end=None
for i in range(src.index("{",start),len(src)):
    if src[i]=="{": depth+=1
    elif src[i]=="}":
        depth-=1
        if depth==0:
            end=i+1
            break
assert end is not None
ref=src[start:end].replace("void Game_trace(","void RefGame_trace(",1)
with tempfile.TemporaryDirectory(prefix="gameai14-plane-") as tmp:
    p=Path(tmp)
    (p/"legacy_trace.inc").write_text(ref)
    for continuation in (0, 1):
        exe=p/f"oracle-{continuation}"
        subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                        "-ffunction-sections","-fdata-sections","-Wl,--gc-sections",
                        "-DDOOMRPG_ESP32=1","-DDOOMRPG_PERF_GAME_P1=0",
                        f"-DCONTINUATION={continuation}",
                        "-I",str(p),"-IESP32/test/oracles","-IESP32/include","-Isrc",
                        "ESP32/test/game_ai14_special_plane_host.c","-o",str(exe)],
                       cwd=ROOT,check=True)
        subprocess.run([str(exe)],check=True)
