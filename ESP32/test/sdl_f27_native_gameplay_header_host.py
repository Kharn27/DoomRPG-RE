#!/usr/bin/env python3
"""SDL-F27: ten actual native gameplay C modules do not import SDL.h in
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
    "esp_native_gameplay_action_engine.c": "aa55c75203ccd8ac9acc3c0b4b38f2021e13d214",
    "esp_native_gameplay_frame.c": "5c0cd0bf64beb8279ce992ff99a46f2fb3485884",
    "esp_native_gameplay_gib_fx.c": "12254012ee7c9089ee1a5e1eacbe200dd5626a55",
    "esp_native_gameplay_hazard_touch.c": "91f4b11fb4314007476318592ef4ceaf1825e257",
    "esp_native_gameplay_pickup.c": "19edc71d41586c2d93c754cf094ed34ffc9dd493",
    "esp_native_gameplay_player_death.c": "82f1cb8a4c6c573e6fa399f018b4564b6977ff1d",
    "esp_native_gameplay_player_resources.c": "8681d358abdc69af95c7362d0b295c777dc387f1",
    "esp_native_gameplay_session.c": "b84f7f973993e79135ab84963ae36fbf5ebc34eb",
    "esp_native_gameplay_weapon.c": "c12d2462615e1cd4ac02c02e7e911678f1ba24a3",
    "esp_native_resident_gameplay.c": "47ad4211f07e87dfa98dde050a26802d5cd0b760"
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
        assert current.startswith(marker), (
            f"{filename}: expected bounded host/geometry-only SDL guard")
        restored = b"#include <SDL.h>\n" + current[len(marker):]
        assert git_blob(restored) == blob, (
            f"{filename}: original pre-F27 body not preserved exactly")
        assert current.count(b"#include <SDL.h>") == 1, filename
        clean = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "",
                       current.decode("utf-8"))
        clean = clean.replace(GUARD, "")
        assert not re.search(
            r"\b(?:SDL_[A-Za-z0-9_]+|SDLK_[A-Za-z0-9_]+|"
            r"SDL_Texture|SDL_Rect|SDL_RWops|SDLCALL|Uint(?:8|16|32))\b",
            clean), f"{filename}: SDL API retained in body"
        assert '#include "DoomRPG.h"' in clean, filename
    print("SDL-F27 PASS sources: 10 real gameplay C bodies are exact "
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
    with tempfile.TemporaryDirectory(prefix="sdl-f27-gameplay-") as temp:
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
            print(f"SDL-F27 PASS C11 {label}: native DoomRPG/Render ABI")


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
    print(f"SDL-F27 PASS {profile}: 10 gameplay TUs compiled; "
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
