#!/usr/bin/env python3
"""SDL-F25: four compiled native monster-combat TUs stop importing SDL.h.

Only the first direct include gets a host/geometry guard. The original whole
C files are recovered from exact Git blobs; the combat, active-turn and
visual implementations are not modified. The existing real attack timeline
host C oracle and PlatformIO/ELF audits remain mandatory CI gates.
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
BEFORE = {
    "esp_native_gameplay_monster_active_sequence.c": "3d3a1b3433c688a81f2c68326573d2820f825d5f",
    "esp_native_gameplay_monster_attack_visual.c": "5efa3429e745d68ef319927a6c7a9ea19097b821",
    "esp_native_gameplay_monster_combat.c": "9337b170163d43f6b16efc8b1fc17f409d9ab223",
    "esp_native_gameplay_monster_retaliation.c": "7693d7c031fe1df985a9f844befa6df810e971ba"
}
HOST_GUARD = ("#if !defined(DOOMRPG_ESP32) || "
              "defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\n"
              "#include <SDL.h>\n"
              "#endif\n")


def git_blob(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode() + b"\0" + data
    ).hexdigest()


def source_gate() -> None:
    for name, sha in BEFORE.items():
        data = (SRC / name).read_bytes()
        prefix = HOST_GUARD.encode("ascii")
        assert data.startswith(prefix), f"{name}: lost host/geometry include"
        assert data.count(b"#include <SDL.h>") == 1, name
        original = b"#include <SDL.h>\n" + data[len(prefix):]
        assert git_blob(original) == sha, f"{name}: C implementation changed"
        body = data[len(prefix):].decode("utf-8")
        # A removal of SDL.h is only safe while the actual compiled body
        # imports neither SDL-specific types nor SDL function/memory aliases.
        active = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", body)
        assert not re.search(
            r"\b(?:SDL_[A-Za-z0-9_]+|SDLCALL|SDLK_[A-Za-z0-9_]+|Uint(?:8|16|32))\b",
            active), name
        assert '#include "DoomRPG.h"' in body, name
    print("SDL-F25 PASS sources: 4 exact pre-F25 Git blobs restored; "
          "native-only SDL.h imports absent, real C bodies identical")


def header_probe() -> None:
    # The true first-file header prefix is compiled in all three modes.
    # Native must compile the original DoomRPG/Render types without SDL.h;
    # host and geometry-oracle retain their historic SDL ABI.
    code = HOST_GUARD + r"""
#include <stdint.h>
#include "DoomRPG.h"
#include "Render.h"
#if EXPECT_SDL
#ifndef DOOMRPG_ESP32_SDL_H
#error "host/geometry lost its original SDL.h ABI"
#endif
#else
#ifdef DOOMRPG_ESP32_SDL_H
#error "native path imported SDL.h transitively"
#endif
_Static_assert(sizeof(EspNativeRect) == sizeof(int) * 4,
               "native rectangle layout changed");
#endif
int main(void) {
    Image_t image = {0};
    Render_t render = {0};
    (void)image;
    (void)render;
    return 0;
}
"""
    with tempfile.TemporaryDirectory(prefix="sdl-f25-monster-") as d:
        directory = Path(d)
        source = directory / "probe.c"
        exe = directory / "probe"
        source.write_text(code, encoding="utf-8")
        base = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "ESP32/include"),
                "-I", str(ROOT / "src")]
        variants = (
            ("native", ["-DDOOMRPG_ESP32=1", "-DEXPECT_SDL=0"]),
            ("host", ["-DEXPECT_SDL=1"]),
            ("geometry", ["-DDOOMRPG_ESP32=1",
                          "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1",
                          "-DEXPECT_SDL=1"]),
        )
        for label, flags in variants:
            subprocess.run(base + flags + [str(source), "-o", str(exe)],
                           check=True)
            subprocess.run([str(exe)], check=True)
            print(f"SDL-F25 PASS C11 {label}: real DoomRPG/Render headers, "
                  "expected SDL ABI boundary")


def compiled_profile(profile: str) -> None:
    elf = ROOT / "ESP32/.pio/build" / profile / "firmware.elf"
    db = ROOT / "ESP32/compile_commands.json"
    assert elf.is_file(), f"{profile}: no firmware ELF"
    assert db.is_file(), f"{profile}: no PlatformIO compiledb"
    units = {Path(x["file"]).name for x in
             json.loads(db.read_text(encoding="utf-8"))}
    for name in BEFORE:
        assert name in units, f"{profile}: missing compiled {name}"
    assert "esp32_sdl.cpp" not in units, profile
    assert "esp32_bmp.cpp" not in units, profile
    print(f"SDL-F25 PASS compiledb {profile}: 4 true C TUs, "
          "no legacy SDL shim/BMP TUs; existing ELF audits mandatory")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=(
        "esp32-cyd", "esp32-cyd-bringup", "esp32-cyd-trace"))
    opts = parser.parse_args()
    source_gate()
    if opts.profile:
        compiled_profile(opts.profile)
    else:
        header_probe()


if __name__ == "__main__":
    main()
