#!/usr/bin/env python3
"""SDL-F13: exact Git inverse of dead CYD controller & video-mode facades.

No native input, touch, renderer, audio, process-root or PAK behavior changed.
The desktop SDL_Video.h declarations survive as a reference/ABI oracle.
"""
from pathlib import Path
import hashlib
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
OWNER = SRC / "engine_stubs.c"
PRE_F13_BLOB = "13b622cff53cfe20d4368d0739b1e492ae88ebc4"
CONTROLLER = "SDLController_t sdlController = {NULL, NULL, NULL, 0, 0};\n"
MODES = "SDLVidModes_t sdlVideoModes[14] = {\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n    {DOOMRPG_LOGICAL_WIDTH, DOOMRPG_LOGICAL_HEIGHT},\n};\n\n"
FUNCTIONS = "int SDL_GameControllerGetButtonID(void) { return -1; }\nchar* SDL_GameControllerGetNameButton(int id) {\n    static char name[] = \"Controller\";\n    (void)id;\n    return name;\n}\nchar* SDL_MouseGetNameButton(int id) {\n    static char name[] = \"Touch\";\n    (void)id;\n    return name;\n}\nint SDL_JoystickGetButtonID(void) { return -1; }\n"
RETIRED = (
    "sdlController", "sdlVideoModes",
    "SDL_GameControllerGetButtonID", "SDL_GameControllerGetNameButton",
    "SDL_MouseGetNameButton", "SDL_JoystickGetButtonID",
)

def restore_pre_f13(current: str) -> str:
    """Reconstruct the complete actual main/F12 C file before F13."""
    a = "FluidSynth_t fluidSynth = {NULL, NULL, NULL};\n\n"
    assert current.count(a) == 1
    restored = current.replace(a, a[:-1] + CONTROLLER + "\n" + MODES, 1)
    b = "void SDL_CloseAudio(void) {}\n\n"
    assert restored.count(b) == 1
    return restored.replace(b, "void SDL_CloseAudio(void) {}\n" + FUNCTIONS + "\n", 1)

def git_sha(s: str) -> str:
    raw = s.encode("utf-8")
    return hashlib.sha1(b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw).hexdigest()

def check_sources() -> None:
    native = OWNER.read_text(encoding="utf-8")
    old = restore_pre_f13(native)
    assert git_sha(old) == PRE_F13_BLOB, (git_sha(old), PRE_F13_BLOB)
    for preserved in (
        "SDLVideo_t sdlVideo = {", "SDLVideo_t* SDL_GetVideo(void)",
        "FluidSynth_t fluidSynth = {NULL, NULL, NULL};",
        "void SDL_InitAudio(void) {}", "void SDL_CloseAudio(void) {}",
        "void SDL_InitVideo(void) {}", "void SDL_Close(void) {}",
        "SDL_CloseAudio();", "SDL_Close();",
        "doomrpg == NULL || doomrpg != doomRpg",
        "Esp32PlatformVideo_framebuffer()",
    ):
        assert preserved in native, preserved
    header = (ROOT / "src/SDL_Video.h").read_text(encoding="latin-1")
    for proto in RETIRED:
        assert re.search(r"\b" + proto + r"\b", header), proto
    ini = (ROOT / "ESP32/platformio.ini").read_text()
    assert "-<esp32_sdl.cpp>" in ini
    count = 0
    for folder in (SRC, ROOT / "ESP32/include"):
        for path in folder.rglob("*"):
            if path.suffix not in (".c", ".cpp", ".h", ".hpp") or path.name == "esp32_sdl.cpp":
                continue
            count += 1
            text = path.read_text(encoding="latin-1")
            clean = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*|\"(?:\\.|[^\"\\])*\"", " ", text)
            for name in RETIRED:
                assert not re.search(r"\b" + name + r"\b", clean), (path, name)
    assert count >= 300, count
    print(f"PASS SDL-F13 six dead facade symbols absent from {count} native sources, "
          f"original Git blob {PRE_F13_BLOB}, SDL video/audio ABI unchanged")

def check_elfs() -> None:
    nm = shutil.which("xtensa-esp32-elf-nm")
    nm = nm or str(Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm")
    assert Path(nm).exists(), "Xtensa nm missing (must run after PlatformIO builds)"
    for name in ("esp32-cyd", "esp32-cyd-bringup", "esp32-cyd-trace"):
        elf = ROOT / "ESP32/.pio/build" / name / "firmware.elf"
        assert elf.is_file(), elf
        result = subprocess.run([str(nm), "--defined-only", str(elf)],
                                check=True, capture_output=True, text=True)
        linked = {line.split()[-1] for line in result.stdout.splitlines() if line.split()}
        remaining = sorted(linked.intersection(RETIRED))
        assert not remaining, (name, remaining)
        print(f"PASS SDL-F13 {name} ELF: no retired input/video-mode data or functions")

if __name__ == "__main__":
    check_sources()
    if len(sys.argv) > 1:
        assert sys.argv[1:] == ["--elf"], sys.argv
        check_elfs()
