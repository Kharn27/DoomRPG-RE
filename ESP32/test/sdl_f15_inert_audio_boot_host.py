#!/usr/bin/env python3
"""SDL-F15: remove inert FluidSynth global and no-op SDL audio init, nothing else.

The playback backend remains native audio intent (silent). The fatal path still
calls SDL_CloseAudio and SDL_Close in historical Root-J6 order. No BT/audio
backend implementation, renderer, input or PAK asset lifecycle is modified.
"""
from pathlib import Path
from sdl_f16_fatal_close_facade_host import restore_pre_f16
import hashlib
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "ESP32/src/engine_stubs.c"
PRE_F15_BLOB = "8cf67bc7c31c5982e1f343b30663df9dba1dd594"
FLUID = "FluidSynth_t fluidSynth = {NULL, NULL, NULL};\n\n"
INIT = "void SDL_InitAudio(void) {}\n"
RETIRED = ("fluidSynth", "SDL_InitAudio")

def restore_pre_f15(current: str) -> str:
    current = restore_pre_f16(current)  # reconstruct source before subsequent F16
    close = "void SDL_Close(void) {}\n"
    assert current.count(close) == 1
    restored = current.replace(close, FLUID + close, 1)
    close_audio = "void SDL_CloseAudio(void) {}\n"
    assert restored.count(close_audio) == 1
    return restored.replace(close_audio, INIT + close_audio, 1)

def git_blob(text: str) -> str:
    payload = text.encode("utf-8")
    return hashlib.sha1(b"blob " + str(len(payload)).encode() + b"\0" + payload).hexdigest()

def check_sources() -> None:
    native = OWNER.read_text(encoding="utf-8")
    pre_f16 = restore_pre_f16(native)
    old = restore_pre_f15(native)
    sha = git_blob(old)
    assert sha == PRE_F15_BLOB, (sha, PRE_F15_BLOB)
    for contract in (
        "void DoomRPG_Error(const char* fmt, ...)",
        "DoomRPG_FreeAppData(doomRpg);",
        "SDL_CloseAudio();", "SDL_Close();", "exit(0);",
        "void SDL_Close(void) {}", "void SDL_CloseAudio(void) {}",
        "FluidSynth_t", "Sound_stopSounds(Sound_t* sound)",
        "Esp32PlatformVideo_framebuffer()",
    ):
        if contract == "FluidSynth_t":
            assert contract not in native, "FluidSynth_t source-global should be absent"
        elif contract in ("SDL_CloseAudio();", "SDL_Close();",
                          "void SDL_Close(void) {}", "void SDL_CloseAudio(void) {}"):
            assert contract in pre_f16, contract  # F15 historic contract before F16
        else:
            assert contract in native, contract
    legacy = (ROOT / "src/SDL_Video.h").read_text(encoding="latin-1")
    for contract in ("FluidSynth_t", "extern FluidSynth_t fluidSynth;",
                     "void SDL_InitAudio(void);", "void SDL_CloseAudio(void);",
                     "void SDL_Close(void);"):
        assert contract in legacy, contract
    assert "-<esp32_sdl.cpp>" in (ROOT / "ESP32/platformio.ini").read_text()
    count = 0
    for folder in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
        for path in folder.rglob("*"):
            if path.suffix not in (".c", ".cpp", ".h", ".hpp") or path.name == "esp32_sdl.cpp":
                continue
            count += 1
            code = path.read_text(encoding="latin-1")
            cleaned = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*|\"(?:\\.|[^\"\\])*\"", " ", code)
            for symbol in RETIRED:
                assert not re.search(r"\b" + symbol + r"\b", cleaned), (path, symbol)
    assert count >= 300, count
    print(f"PASS SDL-F15 inert audio init/global retired from {count} native sources, "
          f"whole pre-F15 Git blob {PRE_F15_BLOB}; fatal closes and audio intent untouched")

def check_elfs() -> None:
    nm = shutil.which("xtensa-esp32-elf-nm")
    nm = nm or str(Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm")
    assert Path(nm).exists(), "xtensa-esp32-elf-nm unavailable"
    for profile in ("esp32-cyd", "esp32-cyd-bringup", "esp32-cyd-trace"):
        elf = ROOT / "ESP32/.pio/build" / profile / "firmware.elf"
        assert elf.is_file(), elf
        result = subprocess.run([str(nm), "--defined-only", str(elf)],
                                capture_output=True, text=True, check=True)
        symbols = {line.split()[-1] for line in result.stdout.splitlines() if line.split()}
        remaining = sorted(symbols.intersection(RETIRED))
        assert not remaining, (profile, remaining)
        print(f"PASS SDL-F15 {profile} ELF: no obsolete FluidSynth object or audio init")

if __name__ == "__main__":
    check_sources()
    if len(sys.argv) > 1:
        assert sys.argv[1:] == ["--elf"], sys.argv
        check_elfs()
