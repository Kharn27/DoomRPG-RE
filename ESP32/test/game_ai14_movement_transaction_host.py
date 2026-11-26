#!/usr/bin/env python3
"""Build real native publisher/probe/RNG TUs with actual headers and stub leaves."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
src = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
names = ("Random_SetSeed", "Random_Get", "Random_getRand", "DoomRPG_setRand")
signatures = ("void Random_SetSeed(unsigned int seed)", "unsigned int Random_Get(void)",
              "void  Random_getRand(byte* buff, int i)", "void DoomRPG_setRand(Random_t* rand)")
def extract(signature):
    anchor = signature + "\n{"
    assert src.count(anchor) == 1, signature
    start = src.index(anchor)
    depth = 0
    for i in range(src.index("{", start), len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start:i+1]
    raise AssertionError(signature)

reference = "static unsigned int _seed = 0;\nstatic int resetRand = 0;\n"
reference += "\n".join(extract(sig) for sig in signatures)
for name in names:
    reference = re.sub(r"\b" + name + r"\b", "Legacy_" + name, reference)

with tempfile.TemporaryDirectory(prefix="gameai14-move-") as tmp:
    p = Path(tmp)
    (p / "reference.c").write_text('#include "DoomRPG.h"\n' + reference)
    exe = p / "oracle"
    subprocess.run([
        "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
        "-DDOOMRPG_ESP32=1", "-DDOOMRPG_PERF_GAME_P1=0",
        "-IESP32/test/oracles", "-IESP32/include", "-Isrc",
        "ESP32/test/game_ai14_movement_transaction_host.c",
        "ESP32/src/esp_native_gameplay_monster_movement_publish.c",
        "ESP32/src/esp_native_gameplay_monster_movement_probe.c",
        "ESP32/src/esp_doomrpg_process_rng.c",
        "ESP32/src/esp_native_rng_replay_guard.c",
        str(p / "reference.c"), "-o", str(exe)
    ], cwd=ROOT, check=True)
    subprocess.run([str(exe)], check=True)

    # A1d uses the same independent legacy generator witness, plus the real
    # active-sequence owner. Each executable has independent process state.
    exe = p / "sequence-oracle"
    subprocess.run([
        "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
        "-DDOOMRPG_ESP32=1", "-DDOOMRPG_PERF_GAME_P1=0",
        "-IESP32/test/oracles", "-IESP32/include", "-Isrc",
        "ESP32/test/game_ai14_sequence_transaction_host.c",
        "ESP32/src/esp_native_gameplay_monster_active_sequence.c",
        "ESP32/src/esp_native_gameplay_monster_movement_publish.c",
        "ESP32/src/esp_native_gameplay_monster_movement_probe.c",
        "ESP32/src/esp_doomrpg_process_rng.c",
        "ESP32/src/esp_native_rng_replay_guard.c",
        str(p / "reference.c"), "-o", str(exe)
    ], cwd=ROOT, check=True)
    subprocess.run([str(exe)], check=True)
