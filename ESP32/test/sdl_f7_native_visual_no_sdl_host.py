#!/usr/bin/env python3
"""SDL-F7: four real native visual/primitive C units have no SDL dependency.

Reverse only the type/libc alias replacements and removed SDL headers to
compare against their pre-F7 source FNVs. Compile actual C units in native
mode, never mocks. Pixel and glyph FNV oracles remain in the existing CI.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ROOT / "ESP32/src"
REFERENCE_FNV = {
    "esp_native_canvas_text.c": 0x0AD3415D,
    "native_story_fit.c": 0x525176C0,
    "esp_doomrpg_primitives.c": 0xD1D001D1,
    "esp_doomrpg_presentation.c": 0x1B2BBDBA,
}


def fnv(source: str) -> int:
    h = 2166136261
    for value in source.encode("utf-8"):
        h = ((h ^ value) * 16777619) & 0xFFFFFFFF
    return h


def restore_reference(file: str, content: str) -> str:
    if file == "esp_native_canvas_text.c":
        content = "#include <SDL.h>\n" + content
        content = content.replace(
            '#include "Render.h"\n',
            '#include "Render.h"\n#include "SDL_Video.h"\n', 1)
        content = content.replace("EspNativeRect renderQuad;", "SDL_Rect renderQuad;")
        content = content.replace("EspNativeRect clip;", "SDL_Rect clip;")
        content = content.replace("len = (int)strlen(text);", "len = (int)SDL_strlen(text);")
    elif file == "native_story_fit.c":
        content = "#include <SDL.h>\n" + content
        content = content.replace("#include <string.h>\n", "", 1)
        content = content.replace(
            '#include "MenuSystem.h"\n',
            '#include "MenuSystem.h"\n#include "SDL_Video.h"\n', 1)
        content = re.sub(r"\bmemset\(", "SDL_memset(", content)
        content = re.sub(r"\bEspNativeRect\b", "SDL_Rect", content)
        content = re.sub(r"\bstrlen\(", "SDL_strlen(", content)
    elif file == "esp_doomrpg_primitives.c":
        content = content.replace(
            "#include <stdint.h>\n",
            "#include <stdint.h>\n#include <SDL.h>\n", 1)
    elif file == "esp_doomrpg_presentation.c":
        content = (
            "#include <SDL.h> /* transitional DoomRPG.h type ABI; "
            "not a runtime SDL call */\n" + content
        )
    return content


for file, expected in REFERENCE_FNV.items():
    content = (SOURCES / file).read_text(encoding="utf-8")
    assert "#include <SDL.h>" not in content, file
    assert '#include "SDL_Video.h"' not in content, file
    active = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", content)
    assert not re.search(r"\bSDL_\w+", active), file
    actual = fnv(restore_reference(file, content))
    assert actual == expected, (
        f"unexpected change outside SDL-F7 replacements in {file}:"
        f" {actual:08x} != {expected:08x}"
    )
    subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
         "-Wno-unused-parameter", "-fsyntax-only",
         "-DDOOMRPG_ESP32=1", "-IESP32/include", "-Isrc",
         str(SOURCES / file)],
        cwd=ROOT, check=True,
    )

print("SDL-F7 PASS four native visual/primitive units SDL-free,"
      " syntax compiled and four source FNVs exact after alias normalization")
