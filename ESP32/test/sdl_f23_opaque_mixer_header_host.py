#!/usr/bin/env python3
"""SDL-F23: break the actual Sound.h -> SDL_mixer.h -> SDL.h native header edge.

Use the unchanged *real* Sound.h, DoomRPG.h, SDL.h, fluidsynth shim and mixer
header; compile and run the C ABI probe with the exact pre-F23 mixer Git
blob and the new header. Host/geometry must still import SDL.h; native CYD
must not import it via the mixer shim. Xtensa PlatformIO compiles all three
firmwares and prior ELF checks still enforce zero linked SDL functions.
"""
from pathlib import Path
import hashlib
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / "ESP32/include"
LEGACY = ROOT / "src"
MIXER = INC / "SDL_mixer.h"
SOUND = LEGACY / "Sound.h"
ENGINE = ROOT / "ESP32/src/engine_stubs.c"
PRE_F23_MIXER_BLOB = "0f41248cbbb6ecd27e7de86305bb51b479fcc76a"
OLD_LINE = "#include <SDL.h>\n"
NEW_GUARD = """/* On the CYD, Sound.h only needs this opaque pointer type, not SDL's
 * entire compatibility header. Preserve the host/geometry-oracle SDL ABI. */
#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
#include <SDL.h>
#endif
"""


def git_blob(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode() + b"\0" + data
    ).hexdigest()


def source_gate() -> str:
    current = MIXER.read_text(encoding="utf-8")
    assert current.count(NEW_GUARD) == 1
    assert current.count(OLD_LINE) == 1
    old = current.replace(NEW_GUARD, OLD_LINE, 1)
    assert git_blob(old.encode("utf-8")) == PRE_F23_MIXER_BLOB, (
        "SDL-F23 mixer differs outside the explicitly gated include")
    assert "typedef struct Mix_Chunk Mix_Chunk;" in current
    assert '#include <SDL_mixer.h>' in SOUND.read_text(encoding="utf-8")
    assert 'Mix_Chunk* mediaAudioSound;' in SOUND.read_text(encoding="utf-8")
    engine = ENGINE.read_text(encoding="utf-8")
    assert engine.startswith(
        "#if !defined(DOOMRPG_ESP32) || defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\n"
        "#include <SDL.h>\n#endif\n")
    assert '#include "Sound.h"' in engine
    print("SDL-F23 PASS source: mixer old Git blob exact, native Sound.h SDL "
          "transitive include gated, desktop/geometry include preserved")
    return old


# Both inputs are the real Sound.h and DoomRPG.h from the repository, rather
# than a surrogate Mix_Chunk declaration. In non-native modes DoomRPG.h needs
# SDL_Texture and Uint8 from the preserved SDL.h include.
PROBE = r"""
#include <SDL_mixer.h>
#if EXPECT_SDL_HEADER
#ifndef DOOMRPG_ESP32_SDL_H
#error "old or desktop/geometry mixer lost its required SDL header"
#endif
#else
#ifdef DOOMRPG_ESP32_SDL_H
#error "CYD Sound.h still imports SDL.h transitively through SDL_mixer"
#endif
#endif
#include "DoomRPG.h"
#include "Sound.h"
#include <stddef.h>
#include <stdio.h>
_Static_assert(sizeof(((SoundChannel_t*)0)->mediaAudioSound) == sizeof(void*),
               "opaque Mix_Chunk pointer ABI drift");
_Static_assert(sizeof(((SoundChannel_t*)0)->mediaAudioMusic) == sizeof(void*),
               "opaque fluid player pointer ABI drift");
_Static_assert(offsetof(SoundChannel_t, mediaAudioSound) == 0,
               "SoundChannel layout drift");
int main(void) {
    printf("%zu/%zu/%zu/%zu/%zu/%zu/%zu\n",
           sizeof(SoundChannel_t), _Alignof(SoundChannel_t),
           offsetof(SoundChannel_t, mediaAudioMusic),
           offsetof(SoundChannel_t, size),
           offsetof(SoundChannel_t, flags),
           sizeof(Sound_t), offsetof(Sound_t, volume));
    return 0;
}
"""


def compile_and_run(cc: str, old: str) -> None:
    with tempfile.TemporaryDirectory(prefix="sdl_f23_") as root:
        temp = Path(root)
        old_header_dir = temp / "before"
        old_header_dir.mkdir()
        (old_header_dir / "SDL_mixer.h").write_text(old, encoding="utf-8")
        source = temp / "sound_layout.c"
        source.write_text(PROBE, encoding="utf-8")
        for name, macros in (
            ("native", ["-DDOOMRPG_ESP32=1"]),
            ("geometry", ["-DDOOMRPG_ESP32=1",
                          "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"]),
            ("host", []),
        ):
            results = []
            for which, directory in (("before", old_header_dir),
                                     ("after", INC)):
                expect = 0 if (name == "native" and which == "after") else 1
                binary = temp / (name + "_" + which)
                cmd = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O0",
                       "-I", str(directory), "-I", str(INC), "-I", str(LEGACY),
                       "-DEXPECT_SDL_HEADER=" + str(expect), *macros,
                       str(source), "-o", str(binary)]
                subprocess.run(cmd, check=True, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
                result = subprocess.check_output([str(binary)], text=True).strip()
                results.append(result)
            assert results[0] == results[1], (
                f"{name}: Sound_t/SoundChannel_t ABI mismatch: {results}")
            print(f"SDL-F23 PASS {name}: real Sound.h C layout "
                  f"before=after={results[0]}, SDL.h include policy checked")


def main() -> None:
    old = source_gate()
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    assert cc, "host C compiler required"
    compile_and_run(cc, old)


if __name__ == "__main__":
    main()
