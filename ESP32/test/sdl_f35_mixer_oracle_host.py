#!/usr/bin/env python3
"""SDL-F35: immutable mixer header lives in host oracles, never native includes."""
from pathlib import Path
import hashlib
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / "ESP32/include"
ORACLES = ROOT / "ESP32/test/oracles"
LEGACY = ROOT / "src"
MIXER = ORACLES / "SDL_mixer.h"
EXPECTED_BLOB = "59fe49318cb550277f999b5c6307c746eb7f6ad8"

data = MIXER.read_bytes()
blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
assert blob == EXPECTED_BLOB, ("mixer oracle bytes changed", blob)
assert not (INC / "SDL_mixer.h").exists(), "mixer header returned to production"
assert (INC / "SDL.h").exists(), "SDL.h remains for desktop/geometry ABI"
assert b"#include <SDL.h>" in data
sound = (LEGACY / "Sound.h").read_bytes()
assert b"typedef struct Mix_Chunk Mix_Chunk;" in sound
assert b"#include <SDL_mixer.h>" in sound
assert b"#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)" in sound
for directory in (ROOT / "ESP32/src", INC):
    for source in directory.rglob("*"):
        if source.suffix not in (".c", ".cpp", ".h", ".hpp"):
            continue
        code = source.read_text(encoding="utf-8", errors="replace")
        assert not re.search(r'(?m)^\s*#\s*include\s*[<"]SDL_mixer\.h[>"]', code), (
            "native source imports host-only SDL mixer: " + str(source))
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text(encoding="utf-8")
assert "Retired SDL mixer header returned to production includes" in builder
assert "Missing host-only SDL mixer header oracle" in builder

PROBE = r"""
#include <SDL_mixer.h>
#if EXPECT_SDL
#ifndef DOOMRPG_ESP32_SDL_H
#error "desktop/geometry mixer lost SDL types"
#endif
#else
#ifdef DOOMRPG_ESP32_SDL_H
#error "native mixer imported SDL.h"
#endif
#endif
#include "DoomRPG.h"
#include "Sound.h"
#include <stddef.h>
#include <stdio.h>
_Static_assert(sizeof(((SoundChannel_t*)0)->mediaAudioSound) == sizeof(void*),
               "Mix_Chunk pointer size drift");
_Static_assert(offsetof(SoundChannel_t, mediaAudioSound) == 0,
               "SoundChannel pointer ABI drift");
int main(void) {
    printf("%zu/%zu/%zu/%zu\n", sizeof(Sound_t), sizeof(SoundChannel_t),
           offsetof(SoundChannel_t, mediaAudioMusic),
           offsetof(SoundChannel_t, flags));
    return 0;
}
"""
cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
assert cc, "host C compiler required"
with tempfile.TemporaryDirectory(prefix="sdl-f35-mixer-") as temp:
    work = Path(temp)
    source = work / "mixer_abi.c"
    source.write_text(PROBE)
    outputs = []
    for name, flags, expect in (
        ("native", ["-DDOOMRPG_ESP32=1"], 0),
        ("desktop", [], 1),
        ("geometry", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], 1),
    ):
        exe = work / name
        cmd = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-I", str(ORACLES), "-I", str(INC), "-I", str(LEGACY),
               "-DEXPECT_SDL=" + str(expect), *flags, str(source), "-o", str(exe)]
        subprocess.run(cmd, check=True, capture_output=True, text=True)
        output = subprocess.check_output([str(exe)], text=True).strip()
        outputs.append(output)
        print("SDL-F35 PASS " + name + ": preserved C11 ABI " + output)
    assert len(set(outputs)) == 1, ("layout mismatch", outputs)
print("SDL-F35 PASS immutable host mixer oracle, zero native imports, three C11 modes")
