#!/usr/bin/env python3
"""SDL-F29: seven real native main-menu C modules do not import SDL.h in
firmware. Reconstruct exact pre-F29 Git source bodies and compile the real
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
    "native_main_menu_actions.c": "d931afd1343b3e5196e5d6b3ee78ef957a494e9c",
    "native_main_menu_model.c": "ce502276130ac28ce9a2dc85d9bf8d6e52482a31",
    "native_main_menu_options_action.c": "b0a135cd3b4d18ff31120129e3be858039f791e1",
    "native_main_menu_options_back.c": "4a7cd8ba97e4ddd7fa3b8885e6995a69186ac2c2",
    "native_main_menu_start_action.c": "a2a2b0e1af984c1ae082f07eb8b83c2b50a6ea50",
    "native_main_menu_touch.c": "de351d7bd1465015dfffa0e5b6a5e35d48723003",
    "native_main_menu_touch_layout.c": "4eee7986eafcb8bd80b8d68077a50740b767ee29"
}
COUNTS = {
    "native_main_menu_actions.c": 2,
    "native_main_menu_model.c": 1,
    "native_main_menu_options_action.c": 1,
    "native_main_menu_options_back.c": 1,
    "native_main_menu_start_action.c": 1,
    "native_main_menu_touch.c": 2,
    "native_main_menu_touch_layout.c": 3
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
    old_import = b"#include <SDL.h>\n"
    for filename, blob in BEFORE.items():
        current = (SOURCES / filename).read_bytes()
        expected = COUNTS[filename]
        assert current.startswith(marker), (
            f"{filename}: missing host/geometry-only SDL guard at TU entry")
        assert current.count(marker) == expected, (
            f"{filename}: expected {expected} SDL include sites, got "
            f"{current.count(marker)}")
        # Some native menu translation units contain multiple concatenated
        # former .c modules, each with its own original SDL.h include.
        # Restore every one of those original imports before whole-blob SHA.
        restored = current.replace(marker, old_import)
        assert git_blob(restored) == blob, (
            f"{filename}: original pre-F29 Git blob not preserved exactly")
        assert current.count(old_import) == expected, filename
        clean = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "",
                       current.decode("utf-8"))
        clean = clean.replace(GUARD, "")
        assert not re.search(
            r"\b(?:SDL_[A-Za-z0-9_]+|SDLK_[A-Za-z0-9_]+|"
            r"SDL_Texture|SDL_Rect|SDL_RWops|SDLCALL|Uint(?:8|16|32))\b",
            clean), f"{filename}: SDL API retained in native C body"
        assert '#include "DoomRPG.h"' in clean, filename
    print("SDL-F29 PASS sources: 7 native menu C bodies / 11 include "
          "sites exact pre-F29 Git blobs, direct native SDL.h absent")


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
    with tempfile.TemporaryDirectory(prefix="sdl-f29-main-menu-") as temp:
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
            print(f"SDL-F29 PASS C11 {label}: native DoomRPG/Render ABI")


def compiled_profile(profile: str) -> None:
    db = ROOT / "ESP32/compile_commands.json"
    elf = ROOT / "ESP32/.pio/build" / profile / "firmware.elf"
    assert db.is_file(), f"{profile}: missing compiledb"
    assert elf.is_file(), f"{profile}: missing ELF"
    compiled = {Path(x["file"]).name for x in
                json.loads(db.read_text(encoding="utf-8"))}
    assert all(name in compiled for name in BEFORE), (
        f"{profile}: F29 native main-menu source not compiled")
    assert "esp32_sdl.cpp" not in compiled, profile
    assert "esp32_bmp.cpp" not in compiled, profile
    print(f"SDL-F29 PASS {profile}: 7 main-menu TUs compiled; "
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
