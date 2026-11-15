#!/usr/bin/env python3
"""SDL-F31: no SDL_mixer.h in native Sound.h; exact desktop/geometry ABI.

Undo one guarded include to restore the complete pre-F31 Git blob. Compile
and run the unchanged real Sound_t/SoundChannel_t layout in native, host and
geometry-oracle modes; compare old/new prints. No audio playback is enabled.
"""
from pathlib import Path
import hashlib
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOUND = ROOT / "src/Sound.h"
INC = ROOT / "ESP32/include"
LEGACY = ROOT / "src"
BEFORE = "c66ec813e06e8cb37b682bd08f5749088113ddc7"
OLD = b"#include <SDL_mixer.h>\r\n"
GUARD = (
    b"/* ESP32-native keeps only an opaque sound pointer; desktop and geometry\r\n"
    b" * oracles retain the original SDL_mixer.h ABI unchanged. */\r\n"
    b"#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\r\n"
    b"typedef struct Mix_Chunk Mix_Chunk;\r\n"
    b"#else\r\n"
    + OLD
    + b"#endif\r\n"
)


def git_blob(source: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(source)).encode("ascii") + b"\0" + source
    ).hexdigest()


def original():
    current = SOUND.read_bytes()
    assert current.count(GUARD) == 1, "F31 Sound.h guard missing"
    restored = current.replace(GUARD, OLD)
    assert git_blob(restored) == BEFORE, "Sound.h changed outside mixer guard"
    assert current.count(OLD) == 1, "desktop/oracle SDL_mixer import drift"
    assert current.count(b"Mix_Chunk* mediaAudioSound;") == 1
    assert b"typedef struct Mix_Chunk Mix_Chunk;" in (INC / "SDL_mixer.h").read_bytes()
    print("SDL-F31 PASS Sound.h: entire exact pre-F31 CRLF Git blob recovered, "
          "native Mix_Chunk opaque; host/geometry mixer include retained")
    return current, restored


PROBE = r"""
#include <stddef.h>
#include <stdio.h>
#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif
#include "DoomRPG.h"
#include "Sound.h"
#if EXPECT_MIXER
#ifndef DOOMRPG_ESP32_SDL_MIXER_H
#error "desktop/geometry and old native lost mixer shim"
#endif
#else
#ifdef DOOMRPG_ESP32_SDL_MIXER_H
#error "native Sound.h still imports SDL_mixer.h"
#endif
#endif
#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#ifdef DOOMRPG_ESP32_SDL_H
#error "native Sound.h imported SDL.h"
#endif
#endif
_Static_assert(__builtin_types_compatible_p(
    __typeof__(((SoundChannel_t*)0)->mediaAudioSound), Mix_Chunk*),
    "opaque pointer type must stay Mix_Chunk*");
_Static_assert(sizeof(((SoundChannel_t*)0)->mediaAudioSound) == sizeof(void*),
    "audio pointer ABI changed");
_Static_assert(offsetof(SoundChannel_t, mediaAudioSound) == 0,
    "SoundChannel ABI changed");
int main(void) {
    printf("%zu/%zu/%zu/%zu/%zu/%zu/%zu/%zu\n",
        sizeof(SoundChannel_t), _Alignof(SoundChannel_t),
        offsetof(SoundChannel_t, mediaAudioMusic),
        offsetof(SoundChannel_t, size),
        offsetof(SoundChannel_t, flags),
        sizeof(Sound_t), _Alignof(Sound_t),
        offsetof(Sound_t, volume));
    return 0;
}
"""


def abi_before_after(current: bytes, restored: bytes):
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    assert cc, "host C11 compiler required"
    with tempfile.TemporaryDirectory(prefix="sdl-f31-sound-") as d:
        folder = Path(d)
        code = folder / "probe.c"
        code.write_text(PROBE, encoding="utf-8")
        for name, defines in (
            ("native", ["-DDOOMRPG_ESP32=1"]),
            ("desktop", []),
            ("geometry", ["-DDOOMRPG_ESP32=1",
                          "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"]),
        ):
            outputs = []
            for stage, sound in (("before", restored), ("after", current)):
                directory = folder / stage
                directory.mkdir(exist_ok=True)
                (directory / "Sound.h").write_bytes(sound)
                expect = int(stage == "before" or name != "native")
                binary = folder / f"{name}-{stage}"
                cmd = [cc, "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
                       *defines, f"-DEXPECT_MIXER={expect}",
                       "-I", str(directory), "-I", str(INC), "-I", str(LEGACY),
                       str(code), "-o", str(binary)]
                build = subprocess.run(cmd, capture_output=True, text=True)
                assert build.returncode == 0, (name, stage, build.stderr)
                run = subprocess.run([str(binary)], capture_output=True, text=True)
                assert run.returncode == 0, (name, stage, run.stderr)
                outputs.append(run.stdout.strip())
            assert outputs[0] == outputs[1], (name, outputs)
            print(f"SDL-F31 PASS C11 {name}: real Sound_t/SoundChannel_t "
                  f"ABI before=after={outputs[0]}")


def main():
    current, restored = original()
    abi_before_after(current, restored)


if __name__ == "__main__":
    main()
