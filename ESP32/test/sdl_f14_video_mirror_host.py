#!/usr/bin/env python3
"""SDL-F14: retire the inert SDLVideo_t mirror, no-op init and getter on CYD.

Production PlatformVideo owns the fixed screen geometry / RGB565 framebuffer.
Root fatal still owns SDL_Close and SDL_CloseAudio separately; neither changes.
This is a whole-source Git inverse, not a heap or speed optimization claim.
"""
from pathlib import Path
import hashlib
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "ESP32/src/engine_stubs.c"
PRE_F14_BLOB = "de49f01a5883b6c3d8cc2616e0902fe28716b161"
VIDEO = "SDLVideo_t sdlVideo = {\n    NULL,\n    (SDL_Renderer*)1,\n    DOOMRPG_LOGICAL_WIDTH,\n    DOOMRPG_LOGICAL_HEIGHT,\n    false,\n    false,\n    true,\n    false,\n    0,\n};\n\n"
INIT = "void SDL_InitVideo(void) {}\n"
GET = "SDLVideo_t* SDL_GetVideo(void) { return &sdlVideo; }\n"
RETIRED = ("sdlVideo", "SDL_GetVideo", "SDL_InitVideo")

def restore_pre_f14(current: str) -> str:
    fluid = "FluidSynth_t fluidSynth = {NULL, NULL, NULL};\n"
    assert current.count(fluid) == 1
    restored = current.replace(fluid, VIDEO + fluid, 1)
    close = "void SDL_Close(void) {}\n"
    assert restored.count(close) == 1
    return restored.replace(close, INIT + close + GET, 1)

def git_blob(text: str) -> str:
    payload = text.encode("utf-8")
    return hashlib.sha1(b"blob " + str(len(payload)).encode() + b"\0" + payload).hexdigest()

def check_sources():
    native = OWNER.read_text(encoding="utf-8")
    inverse = restore_pre_f14(native)
    sha = git_blob(inverse)
    assert sha == PRE_F14_BLOB, (sha, PRE_F14_BLOB)
    for literal in (
        "FluidSynth_t fluidSynth = {NULL, NULL, NULL};",
        "void SDL_Close(void) {}", "void SDL_InitAudio(void) {}",
        "void SDL_CloseAudio(void) {}", "SDL_CloseAudio();", "SDL_Close();",
        "doomrpg == NULL || doomrpg != doomRpg",
        "Esp32PlatformVideo_framebuffer()",
        "SDL_Rect screenRect;", "SDL_Rect* windowRect",
    ):
        assert literal in native, literal
    old = (ROOT / "src/SDL_Video.h").read_text(encoding="latin-1")
    for symbol in RETIRED:
        assert re.search(r"\b" + symbol + r"\b", old), symbol
    video_native = (ROOT / "ESP32/src/platform_video.cpp").read_text()
    assert "PlatformVideo_begin(" in video_native
    assert "PlatformVideo_present(" in video_native
    assert "-<esp32_sdl.cpp>" in (ROOT / "ESP32/platformio.ini").read_text()
    count=0
    for base in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
        for path in base.rglob("*"):
            if path.suffix not in (".c", ".cpp", ".h", ".hpp") or path.name == "esp32_sdl.cpp":
                continue
            count += 1
            code = path.read_text(encoding="latin-1")
            active = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*|\"(?:\\.|[^\"\\])*\"", " ", code)
            for symbol in RETIRED:
                assert not re.search(r"\b" + symbol + r"\b", active), (path, symbol)
    assert count >= 300, count
    print(f"PASS SDL-F14 inert SDLVideo mirror retired from {count} native C/C++/headers, "
          f"complete pre-F14 Git blob {PRE_F14_BLOB}; PlatformVideo and fatal close unchanged")

def check_elfs():
    nm = shutil.which("xtensa-esp32-elf-nm")
    nm = nm or str(Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm")
    assert Path(nm).exists(), "Xtensa nm not installed (CI after PlatformIO builds)"
    for profile in ("esp32-cyd", "esp32-cyd-bringup", "esp32-cyd-trace"):
        elf = ROOT / "ESP32/.pio/build" / profile / "firmware.elf"
        assert elf.is_file(), elf
        r = subprocess.run([str(nm), "--defined-only", str(elf)],
                           capture_output=True, check=True, text=True)
        symbols = {line.split()[-1] for line in r.stdout.splitlines() if line.split()}
        assert not symbols.intersection(RETIRED), (profile, symbols.intersection(RETIRED))
        print(f"PASS SDL-F14 {profile} ELF: no SDL video mirror, no noop init/getter")

if __name__ == "__main__":
    check_sources()
    if len(sys.argv) > 1:
        assert sys.argv[1:] == ["--elf"], sys.argv
        check_elfs()
