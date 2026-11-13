#!/usr/bin/env python3
"""SDL-F28: six real native rendering/intro C modules do not import SDL.h in
firmware. Reconstruct exact pre-F27 Git source bodies and compile the real
C11 DoomRPG/Render headers with native, host and geometry SDL boundaries.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ROOT / "ESP32/src"
BEFORE = {
    "esp_native_dynamic_line_render.c": "fa37687c047f075eb54364154339832a4b8a7b8c",
    "esp_native_intro_state.c": "7d01fc603fe36453d8819a6af88391344420eac2",
    "esp_native_plane_renderer.c": "3cff9235ac06f5f108e76c629fd0203bcce0c770",
    "esp_native_sprite_renderer.c": "702fa36c9c02fc9542df576727ff1c7e17fc283a",
    "esp_render_geometry_primitives.c": "5aa161bdd5e697edfc89b29b52812f40f5f2efa0",
    "native_first_frame_color_probe_wrappers.c": "9d4fdb192f8cb7f29df9cce1a429ee483fed63a5"
}
GUARD = ("#if !defined(DOOMRPG_ESP32) || "
         "defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\n"
         "#include <SDL.h>\n"
         "#endif\n")


def git_blob(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode() + b"\0" + data
    ).hexdigest()


def original_body_parity() -> None:
    marker = GUARD.encode("ascii")
    for filename, blob in BEFORE.items():
        current = (SOURCES / filename).read_bytes()
        if filename == "esp_render_geometry_primitives.c":
            # Unlike the five other TUs, SDL.h originally follows stdint.
            assert current.startswith(b"#include <stdint.h>\n\n" + marker), filename
            restored = current.replace(marker, b"#include <SDL.h>\n", 1)
        else:
            assert current.startswith(marker), (
                f"{filename}: expected bounded host/geometry-only SDL guard")
            restored = b"#include <SDL.h>\n" + current[len(marker):]
        assert git_blob(restored) == blob, (
            f"{filename}: original pre-F28 body not preserved exactly")
        assert current.count(b"#include <SDL.h>") == 1, filename
        clean = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "",
                       current.decode("utf-8"))
        clean = clean.replace(GUARD, "")
        assert not re.search(
            r"\b(?:SDL_[A-Za-z0-9_]+|SDLK_[A-Za-z0-9_]+|"
            r"SDL_Texture|SDL_Rect|SDL_RWops|SDLCALL|Uint(?:8|16|32))\b",
            clean), f"{filename}: SDL API retained in body"
        assert '#include "DoomRPG.h"' in clean, filename
    print("SDL-F28 PASS sources: 6 native render/intro C bodies are exact "
          "pre-F27 Git blobs, native SDL.h imports absent")


def header_probe() -> None:
    code = r"""
#include <stdint.h>
#if EXPECT_SDL
#include <SDL.h>
#endif
#include "DoomRPG.h"
#include "Render.h"
#if EXPECT_SDL
#ifndef DOOMRPG_ESP32_SDL_H
#error "host SDL oracle header no longer included"
#endif
#else
#ifdef DOOMRPG_ESP32_SDL_H
#error "native DoomRPG/Render headers imported SDL"
#endif
_Static_assert(sizeof(EspNativeRect) == 4*sizeof(int),
               "native render rectangle ABI drift");
#endif
_Static_assert(sizeof(Random_t) == 132, "RNG ABI changed");
int main(void) {
    Random_t state = {0};
    return state.nextRand != 0;
}
"""
    with tempfile.TemporaryDirectory(prefix="sdl-f28-render-intro-") as temp:
        root = Path(temp)
        src, exe = root / "probe.c", root / "probe"
        src.write_text(code, encoding="utf-8")
        cmd = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-I", str(ROOT / "ESP32/include"),
               "-I", str(ROOT / "src")]
        for label, flags in (
            ("native", ["-DDOOMRPG_ESP32=1", "-DEXPECT_SDL=0"]),
            ("desktop", ["-DEXPECT_SDL=1"]),
            ("geometry", ["-DDOOMRPG_ESP32=1",
                          "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1",
                          "-DEXPECT_SDL=1"]),
        ):
            subprocess.run(cmd + flags + [str(src), "-o", str(exe)],
                           check=True)
            subprocess.run([str(exe)], check=True)
            print(f"SDL-F28 PASS C11 {label}: native DoomRPG/Render ABI")


def compiled_profile(profile: str) -> None:
    db = ROOT / "ESP32/compile_commands.json"
    elf = ROOT / "ESP32/.pio/build" / profile / "firmware.elf"
    assert db.is_file(), f"{profile}: missing compiledb"
    assert elf.is_file(), f"{profile}: missing ELF"
    compiled = {Path(x["file"]).name for x in
                json.loads(db.read_text(encoding="utf-8"))}
    assert all(name in compiled for name in BEFORE), (
        f"{profile}: F27 native gameplay source not compiled")
    assert "esp32_sdl.cpp" not in compiled, profile
    assert "esp32_bmp.cpp" not in compiled, profile
    print(f"SDL-F28 PASS {profile}: 6 render/intro TUs compiled; "
          "old SDL shim and BMP excluded; existing ELF SDL-F3 audit retained")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=(
        "esp32-cyd", "esp32-cyd-bringup", "esp32-cyd-trace"))
    args = parser.parse_args()
    original_body_parity()
    if args.profile:
        compiled_profile(args.profile)
    else:
        header_probe()


if __name__ == "__main__":
    main()
