#!/usr/bin/env python3
"""SDL-F36: original SDL.h lives in host oracles, never native includes."""
from pathlib import Path
import hashlib
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ORACLES = ROOT / "ESP32/test/oracles"
INC = ROOT / "ESP32/include"
HEADER = ORACLES / "SDL.h"
EXPECTED = "da1040d4fa36f70cafe1141f0d6e573741a3d575"

assert HEADER.is_file(), "SDL.h host oracle missing"
assert not (INC / "SDL.h").exists(), "SDL.h returned to production includes"
raw = HEADER.read_bytes()
blob = hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()
assert blob == EXPECTED, ("SDL.h historical Git blob changed", blob)
assert b"typedef EspNativeRect SDL_Rect;" in raw
assert b"typedef EspNativeTexture SDL_Texture;" in raw
assert b"typedef EspNativeImageColor SDL_Color;" in raw
assert b"SDL_RWops* SDL_RWFromMem(" in raw
assert b'#include "esp_native_texture.h"' in raw
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text()
assert "Retired SDL core header returned to production includes" in builder
assert "Missing host-only SDL core header oracle" in builder

PROBE = r"""
#include <SDL.h>
#include <stddef.h>
#include <stdio.h>
_Static_assert(sizeof(SDL_Rect) == sizeof(EspNativeRect), "rectangle ABI");
_Static_assert(offsetof(SDL_Rect, x) == offsetof(EspNativeRect, x), "rect x");
_Static_assert(offsetof(SDL_Rect, h) == offsetof(EspNativeRect, h), "rect h");
_Static_assert(sizeof(SDL_Texture*) == sizeof(EspNativeTexture*), "texture pointer");
_Static_assert(__builtin_types_compatible_p(SDL_Texture, EspNativeTexture),
               "SDL texture aliases original native owner");
_Static_assert(__builtin_types_compatible_p(SDL_Color, EspNativeImageColor),
               "SDL color preserves palette owner");
int main(void) {
    printf("%zu/%zu/%zu/%zu/%zu\n",
        sizeof(SDL_Rect), sizeof(SDL_Surface), sizeof(SDL_PixelFormat),
        sizeof(SDL_Palette), sizeof(SDL_Color));
    return 0;
}
"""
cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
assert cc, "C11 host compiler missing"
with tempfile.TemporaryDirectory(prefix="sdl-f36-header-") as t:
    tmp = Path(t)
    source = tmp / "abi.c"
    source.write_text(PROBE)
    results = []
    for name, flags in (("desktop", []),
                        ("geometry", ["-DDOOMRPG_ESP32=1",
                                      "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"])):
        exe = tmp / name
        cmd = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-I", str(ORACLES), "-I", str(INC), *flags,
               str(source), "-o", str(exe)]
        run = subprocess.run(cmd, capture_output=True, text=True)
        assert run.returncode == 0, (name, run.stderr)
        output = subprocess.check_output([str(exe)], text=True).strip()
        results.append(output)
        print("SDL-F36 PASS " + name + ": original SDL header C11 ABI " + output)
    assert len(set(results)) == 1, ("host/geometry ABI drift", results)
print("SDL-F36 PASS byte-exact SDL.h host oracle, no production header, C11 ABI intact")
