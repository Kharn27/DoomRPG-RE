#!/usr/bin/env python3
"""SDL-F24: three *compiled* native BSP/first-frame/script C units do not
import the legacy SDL.h themselves. Preserve host/geometry-oracle legacy
prefixes, and use exact original Git blobs to prevent ANY behavioral edits.

PlatformIO CI builds all 3 Xtensa profiles; compiledb verifies these actual
translation units were compiled, and the same ELF audit retains zero SDL_*
functions linked. The genuine old C body beyond the guard is bit-identical.
"""
import argparse
from pathlib import Path
import hashlib
import json
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
BEFORE = {
    "esp_native_bsp_visibility.c": "1ad4039d8bdf39afec5e9d502a4228a69201cafb",
    "esp_native_first_frame.c": "2cf08c3317336b95c947a71bb9823ba179b96f0e",
    "esp_native_gameplay_event_chain.c": "3052b6bb4a1134252e4f75fe70e5902b1ca1276f",
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
    for name, old_sha in BEFORE.items():
        current = (SRC / name).read_bytes()
        prefix = HOST_GUARD.encode()
        assert current.startswith(prefix), f"{name}: no host/geometry guard"
        assert current.count(b"#include <SDL.h>") == 1, name
        old = b"#include <SDL.h>\n" + current[len(prefix):]
        assert git_blob(old) == old_sha, f"{name}: real C body or include changed"
        body = current[len(prefix):].decode("utf-8")
        active = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", body)
        assert not re.search(r"\bSDL_[A-Za-z0-9_]+\b|\bSDLCALL\b|\bUint(?:8|16|32)\b", active), (
            f"{name}: still actively uses SDL API/types")
        assert '#include "DoomRPG.h"' in body, name
    print("SDL-F24 PASS source: three historical Git blobs exact; "
          "BSP / first-frame / event chain direct SDL.h only for host/geometry; "
          "all real C implementation bodies byte-identical")


def header_probe() -> None:
    """Check that the real native Render/DoomRPG headers compile without SDL.h."""
    code = r"""
#include <stdint.h>
#include "DoomRPG.h"
#include "Render.h"
#ifdef DOOMRPG_ESP32_SDL_H
#error "direct SDL.h returned unexpectedly to native header probe"
#endif
_Static_assert(sizeof(EspNativeRect) == sizeof(int) * 4,
               "native rect layout changed");
int main(void) {
    Image_t image = {0};
    Render_t render = {0};
    EspNativeRect clip = {0, 20, 160, 80};
    return image.imgBitmap == 0 && render.framebuffer == 0
           && clip.w == 160 && clip.h == 80 ? 0 : 1;
}
"""
    with tempfile.TemporaryDirectory(prefix="sdl_f24_native_") as d:
        p = Path(d)
        (p / "probe.c").write_text(code, encoding="utf-8")
        command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                   "-DDOOMRPG_ESP32=1", "-I", str(ROOT / "ESP32/include"),
                   "-I", str(ROOT / "src"), str(p / "probe.c"),
                   "-o", str(p / "probe")]
        subprocess.run(command, check=True)
        subprocess.run([str(p / "probe")], check=True)
    print("SDL-F24 PASS C11: real native DoomRPG/Render ABI headers compile "
          "and execute without SDL.h")


def compiled_profile(profile: str) -> None:
    elf = ROOT / "ESP32/.pio/build" / profile / "firmware.elf"
    compdb = ROOT / "ESP32/compile_commands.json"
    assert elf.is_file(), f"{profile}: no firmware ELF"
    assert compdb.is_file(), f"{profile}: compiledb missing"
    files = {Path(entry["file"]).name for entry in
             json.loads(compdb.read_text(encoding="utf-8"))}
    for unit in BEFORE:
        assert unit in files, f"{profile}: {unit} not compiled"
    assert "esp32_sdl.cpp" not in files, f"{profile}: SDL shim compiled"
    assert "esp32_bmp.cpp" not in files, f"{profile}: SDL BMP compiled"
    print(f"SDL-F24 PASS {profile}: compiledb includes all 3 native sources, "
          "excludes SDL shim/BMP; existing F3/F22 ELF audits verify no SDL symbols")


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
