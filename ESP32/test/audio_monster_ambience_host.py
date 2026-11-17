#!/usr/bin/env python3
"""Compile and run native monster audio-only lifecycle with 14-family table."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="doomrpg-monster-ambience-") as d:
    cmd=["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
        "-I",str(root/"ESP32/include"),"-I",str(root/"ESP32/src"),
        str(root/"ESP32/test/audio_monster_ambience_host.c"),
        str(root/"ESP32/src/esp_native_monster_ambience.c"),
        "-o",str(Path(d)/"monster-ambience")]
    subprocess.run(cmd,check=True)
    subprocess.run([str(Path(d)/"monster-ambience")],check=True)

with tempfile.TemporaryDirectory(prefix="doomrpg-monster-pain-") as d:
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra",
        "-Wno-unused-variable","-Werror","-DDOOMRPG_ESP32=1",
        "-ffunction-sections","-fdata-sections",
        "-I",str(root/"ESP32/test/host_include"),
        "-I",str(root/"ESP32/test/oracles"),
        "-I",str(root/"ESP32/include"),
        "-I",str(root/"ESP32/src"),
        "-I",str(root/"src"),
        str(root/"ESP32/test/monster_pain_hold_host.c"),
        "-Wl,--gc-sections","-o",str(Path(d)/"monster-pain")],check=True)
    subprocess.run([str(Path(d)/"monster-pain")],check=True)
