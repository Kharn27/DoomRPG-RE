#!/usr/bin/env python3
"""SDL-F32: keep historical SDL_RWops BMP oracles OUTSIDE production trees.

F22 stopped compiling the old decoder; F32 moves the exact .cpp/.h Git blobs
to ESP32/test/oracles, and asserts that src/include no longer contain them.
The firmware continues to compile the PAK-backed native indexed BMP reader.
Real PlatformIO compiledb/ELF checks remain the ultimate build/link guard.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
ORACLES = ROOT / "ESP32/test/oracles"
LEGACY = ORACLES / "esp32_bmp.cpp"
LEGACY_HEADER = ORACLES / "esp32_bmp.h"
NATIVE = SRC / "esp_native_indexed_bmp.c"
RESOURCE_OWNER = SRC / "esp_doomrpg_image_resources.c"
INI = ROOT / "ESP32/platformio.ini"
LEGACY_GIT_BLOB = "88a62dfdd3d4fbb2c991dba4d480d4596834563c"
HEADER_GIT_BLOB = "f896aa522c6c1cbf3d38852cadc086331f98405a"


def git_blob(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


def active(source: str) -> str:
    # Ignore historical references in comments, but not active calls or includes.
    return re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", source)


def source_gate() -> None:
    assert not (SRC / "esp32_bmp.cpp").exists(), (
        "host-only SDL BMP decoder returned to production sources")
    assert not (ROOT / "ESP32/include/esp32_bmp.h").exists(), (
        "host-only SDL BMP header returned to production includes")
    assert git_blob(LEGACY.read_bytes()) == LEGACY_GIT_BLOB, "legacy BMP oracle changed"
    assert git_blob(LEGACY_HEADER.read_bytes()) == HEADER_GIT_BLOB, "legacy BMP ABI changed"
    old = LEGACY.read_text(encoding="utf-8")
    assert 'extern "C" SDL_Surface* Esp32Bmp_LoadRW(SDL_RWops* source, int freeSource)' in old
    for required in ("SDL_RWread(", "SDL_RWseek(", "SDL_RWclose(",
                     "SDL_FreeSurface(", "SDL_Palette"):
        assert required in old, required

    ini = INI.read_text(encoding="utf-8")
    assert len(re.findall(r"^\s*-\<esp32_sdl\.cpp\>\s*$", ini, re.M)) == 1
    assert len(re.findall(r"^\s*-\<esp32_bmp\.cpp\>\s*$", ini, re.M)) == 1
    assert "extends = env:esp32-cyd" in ini
    assert "[env:esp32-cyd-bringup]" in ini
    assert "[env:esp32-cyd-trace]" in ini

    native = NATIVE.read_text(encoding="utf-8")
    image = RESOURCE_OWNER.read_text(encoding="utf-8")
    assert "EspNativeIndexedBmp_open(" in native
    assert 'EspAssetPack_readRange(' in native
    assert 'EspNativeImage_loadPack(' in image
    assert 'EspNativeTexture_adoptIndexed(&decoded)' in image
    # Other production TUs must not call this decommissioned SDL decoder.
    checked = 0
    for path in SRC.iterdir():
        if path.suffix not in (".cpp", ".c", ".h", ".hpp"):
            continue
        if path.name in ("esp32_bmp.cpp", "esp32_sdl.cpp"):
            continue
        s = active(path.read_text(encoding="utf-8"))
        assert not re.search(r"\bEsp32Bmp_LoadRW\s*\(", s), path
        assert '#include "esp32_bmp.h"' not in s, path
        checked += 1
    assert checked > 125, checked
    print(f"SDL-F32 PASS source: {checked} ESP32/src TUs free from SDL BMP; "
          "CPP/header moved to host-only test/oracles with exact Git blobs; "
          "native PAK reader intact")


def compiled_profile(profile: str) -> None:
    build = ROOT / "ESP32/.pio/build" / profile
    elf = build / "firmware.elf"
    compdb = ROOT / "ESP32/compile_commands.json"
    assert elf.is_file(), elf
    assert compdb.is_file(), compdb
    entries = json.loads(compdb.read_text(encoding="utf-8"))
    assert entries, "empty PlatformIO compiledb"
    names = {Path(row["file"]).name for row in entries}
    assert "esp32_bmp.cpp" not in names, f"{profile}: compiled legacy BMP TU"
    assert "esp32_sdl.cpp" not in names, f"{profile}: compiled SDL shim"
    assert "esp_native_indexed_bmp.c" in names, f"{profile}: lost native BMP reader"
    assert not (build / "src/esp32_bmp.cpp.o").exists(), f"{profile}: old BMP object exists"

    nm = (shutil.which("xtensa-esp32-elf-nm") or
          str(Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm"))
    assert Path(nm).is_file(), "Xtensa nm tool missing"
    symbols = subprocess.check_output([nm, "--defined-only", str(elf)],
                                      text=True).splitlines()
    names = {line.split()[-1] for line in symbols if line.split()}
    assert "Esp32Bmp_LoadRW" not in names, f"{profile}: legacy BMP decoder linked"
    assert "EspNativeIndexedBmp_open" in names, f"{profile}: native BMP reader missing"
    assert not any(name.startswith("SDL_") for name in names), (
        f"{profile}: unexpected SDL-linked symbols")
    print(f"SDL-F22 PASS {profile}: compiledb excludes SDL RWops BMP and shim; "
          "ELF links native PAK BMP reader, no obsolete SDL decoder/functions")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=("esp32-cyd", "esp32-cyd-bringup",
                                               "esp32-cyd-trace"))
    args = parser.parse_args()
    source_gate()
    if args.profile:
        compiled_profile(args.profile)


if __name__ == "__main__":
    main()
