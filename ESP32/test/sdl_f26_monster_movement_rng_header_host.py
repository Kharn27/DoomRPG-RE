#!/usr/bin/env python3
"""SDL-F26: five compiled monster movement/state TUs plus their RNG replay
header no longer import SDL.h in native firmware. Keep desktop/geometry-oracle
ABI intact, restore *exact* pre-F26 Git blob contents, and check actual native
headers with a compiler. ELF/compiledb checks are run for each CYD profile.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
HEADER = ROOT / "ESP32/include/esp_native_rng_replay_guard.h"
BEFORE = {
    "esp_native_gameplay_monster_movement.c": "593140ab6737e0034c3cd7cb4c0da289ef67e0e0",
    "esp_native_gameplay_monster_movement_publish.c": "c5df65702973409928dab3a466d0e57182d358d2",
    "esp_native_gameplay_monster_three_goal_turn.c": "29fc86f16578de50de7e5dd7a5d0233cee779d20",
    "esp_native_gameplay_monster_turn.c": "ce81c39d4097154ffe6ce5f30182d59985a29840",
    "esp_native_gameplay_monster_state.c": "5932839b2583157a20610328050eba45e5d7dd7a"
}
HEADER_BEFORE = "be4dbd482abdeef36abebad78b69d7c6caa9accc"
HOST_GUARD = ("#if !defined(DOOMRPG_ESP32) || "
              "defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\n"
              "#include <SDL.h>\n"
              "#endif\n")


def git_blob(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode() + b"\0" + data
    ).hexdigest()


def source_gate() -> None:
    guard = HOST_GUARD.encode("ascii")
    for name, sha in BEFORE.items():
        current = (SRC / name).read_bytes()
        if name == "esp_native_gameplay_monster_state.c":
            # The original include follows four libc headers and one blank.
            anchor = b"\n#include <SDL.h>\n#include \"DoomRPG.h\""
            replacement = b"\n" + guard + b"#include \"DoomRPG.h\""
            assert current.count(replacement) == 1, name
            before = current.replace(replacement, anchor, 1)
        else:
            assert current.startswith(guard), name
            before = b"#include <SDL.h>\n" + current[len(guard):]
        assert git_blob(before) == sha, f"{name}: whole source body changed"
        assert current.count(b"#include <SDL.h>") == 1, name
        clean = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "",
                       current.decode("utf-8"))
        clean = clean.replace(HOST_GUARD, "")
        assert not re.search(
            r"\b(?:SDL_[A-Za-z0-9_]+|SDLCALL|SDLK_[A-Za-z0-9_]+|"
            r"SDL_Texture|SDL_Rect|SDL_RWops|Uint(?:8|16|32))\b",
            clean), name
        assert '#include "DoomRPG.h"' in clean, name

    header = HEADER.read_bytes()
    assert header.count(guard) == 1
    assert header.count(b"#include <SDL.h>") == 1
    original = header.replace(guard, b"#include <SDL.h>\n", 1)
    assert git_blob(original) == HEADER_BEFORE, "RNG ABI header changed"
    assert b"EspNativeRngReplayGuard_beginProbeBoundary" in header
    print("SDL-F26 PASS source: 5 unchanged real monster C bodies, "
          "1 unchanged RNG ABI header; 6 exact historical Git blobs")


def header_probe() -> None:
    code = r"""
#include <stdint.h>
#include "esp_native_rng_replay_guard.h"
#include "Render.h"
#if EXPECT_SDL
#ifndef DOOMRPG_ESP32_SDL_H
#error "desktop/geometry SDL include lost"
#endif
#else
#ifdef DOOMRPG_ESP32_SDL_H
#error "native replay header imported SDL transitively"
#endif
_Static_assert(sizeof(EspNativeRect) == 4*sizeof(int),
               "native rectangle ABI drift");
#endif
_Static_assert(sizeof(Random_t) == 132, "original RNG ABI drift");
int main(void) {
    Random_t state = {0};
    return state.nextRand == 0 ? 0 : 1;
}
"""
    with tempfile.TemporaryDirectory(prefix="sdl-f26-rng-") as d:
        p = Path(d)
        source, exe = p / "probe.c", p / "probe"
        source.write_text(code, encoding="utf-8")
        base = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "ESP32/include"), "-I", str(ROOT / "src")]
        for mode, flags in (
            ("native", ["-DDOOMRPG_ESP32=1", "-DEXPECT_SDL=0"]),
            ("desktop", ["-DEXPECT_SDL=1"]),
            ("geometry", ["-DDOOMRPG_ESP32=1",
                           "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1",
                           "-DEXPECT_SDL=1"]),
        ):
            subprocess.run(base + flags + [str(source), "-o", str(exe)],
                           check=True)
            subprocess.run([str(exe)], check=True)
            print(f"SDL-F26 PASS C11 {mode}: actual native RNG/Render ABI")


def compiled_profile(profile: str) -> None:
    elf = ROOT / "ESP32/.pio/build" / profile / "firmware.elf"
    db = ROOT / "ESP32/compile_commands.json"
    assert elf.is_file(), f"{profile}: firmware ELF absent"
    assert db.is_file(), f"{profile}: compile_commands missing"
    paths = {Path(item["file"]).name for item in
             json.loads(db.read_text(encoding="utf-8"))}
    assert all(name in paths for name in BEFORE), (
        f"{profile}: monster movement/state TU missing", paths)
    assert "esp_native_rng_replay_guard.c" in paths, profile
    assert "esp32_sdl.cpp" not in paths, profile
    assert "esp32_bmp.cpp" not in paths, profile
    print(f"SDL-F26 PASS {profile}: five compiled C movement/state units, "
          "RNG replay TU compiled, shim/BMP absent; ELF SDL-F3 audit retained")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=(
        "esp32-cyd", "esp32-cyd-bringup", "esp32-cyd-trace"))
    args = parser.parse_args()
    source_gate()
    if args.profile:
        compiled_profile(args.profile)
    else:
        header_probe()


if __name__ == "__main__":
    main()
