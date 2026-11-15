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


def strip_ra2b_probe(source: bytes) -> bytes:
    """The R-A2b instrumentation is an explicitly opt-in, small diagnostic.
    Remove EXACTLY its four compiler-gated blocks before comparing against the
    immutable pre-F27 Git C body. Do not weaken any other 9 body oracles.
    """
    flag = b"#if defined(DOOMRPG_RA2B_CAPTURE) && DOOMRPG_RA2B_CAPTURE"
    assert source.count(flag) == 4, "R-A2b probe blocks missing or unexpected"
    for witness in (
        b"RA2B_MAX_FRAME_WITNESSES 48U",
        b"EspAssetPack_ra2bSdRangeMicros()",
        b"ra2bFlashBefore = EspAssetPack_mapFlashRuntimeReadMicros()",
        b'printf("[RA2B] FRAME v=1',
    ):
        assert witness in source, "R-A2b diagnostic contract changed"
    stripped, count = re.subn(
        rb"\n#if defined\(DOOMRPG_RA2B_CAPTURE\) && DOOMRPG_RA2B_CAPTURE\n"
        rb".*?^#endif\n",
        b"\n", source, flags=re.MULTILINE | re.DOTALL)
    assert count == 4, "Unexpected R-A2b probe extraction count"
    # The first diagnostic block was inserted after an intentionally empty
    # separator line. Restore the exact historical adjacency for this ONE
    # original C body, leaving every other line byte-identical.
    separator = (b"static GameplayFrameScratch frameScratch;\n\n"
                 b"/* PR236 B probe")
    assert stripped.count(separator) == 1, "R-A2b frame separator changed"
    stripped = stripped.replace(separator,
                                b"static GameplayFrameScratch frameScratch;\n"
                                b"/* PR236 B probe", 1)
    assert b"RA2B" not in stripped, "R-A2b diagnostic escaped its guards"
    return stripped


def original_body_parity() -> None:
    marker = GUARD.encode("ascii")
    for filename, blob in BEFORE.items():
        current = (SOURCES / filename).read_bytes()
        assert current.startswith(marker), (
            f"{filename}: expected bounded host/geometry-only SDL guard")
        if filename == "esp_native_gameplay_frame.c":
            # AUDIO-R2d adds a single synchronous SD pump after a successful
            # frame. Reverse ONLY these two additions before checking the
            # immutable pre-F27 Git blob. No weakened hashes or skipped TU.
            audio_import = (b"#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n"
                            b'#include "esp_audio_entrance_music.h"\n'
                            b"#endif\n")
            audio_hook = (
                b"#if defined(ARDUINO) && defined(DOOMRPG_ESP32)\n"
                b"    /* AUDIO-R2d cooperative main-task inter-frame refill. Renderer and PAK\n"
                b"     * have completed/released ownership above. Never read SD during raster\n"
                b"     * or the TFT present, and never in the timer ISR. Metrics were frozen\n"
                b"     * above before servicing audio; gameplay state and frame FNV untouched. */\n"
                b"    if (ok && publish) EspEntranceMusic_pumpGameplay();\n"
                b"#endif\n"
            )
            assert current.count(audio_import) == 1
            assert current.count(audio_hook) == 1
            current = current.replace(audio_import, b"", 1)
            current = current.replace(audio_hook, b"", 1)
            current = strip_ra2b_probe(current)
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
               "-I", str(ROOT / "ESP32/test/oracles"),
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
