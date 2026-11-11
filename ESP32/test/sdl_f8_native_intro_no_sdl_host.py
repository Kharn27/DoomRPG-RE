#!/usr/bin/env python3
"""SDL-F8: preserve real native intro source exactly after SDL alias removal.

No desktop/J2ME translation unit is compiled into ESP32. The four native
intro files are compared to their pre-F8 source fingerprints after reversing
only the removed SDL.h include and documented libc aliases. All executable
C is otherwise identical; PlatformIO compiles the real four units.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
INTRO = ROOT / "ESP32/src"
SDL_H = (ROOT / "ESP32/include/SDL.h").read_text(encoding="utf-8")
REFERENCE_FNV = {
    "native_intro_clock.c": 0x20832EDC,
    "native_intro_input.c": 0x8A0525B1,
    "native_intro_first_frame.c": 0xAF1263C4,
    "native_intro_dispose.c": 0x31A9A312,
}

# Actual original CYD SDL.h mapped these four macros to standard libc.
for macro, target in (
    ("SDL_strcmp", "strcmp"), ("SDL_memset", "memset"),
    ("SDL_strlen", "strlen"), ("SDL_free", "free"),
):
    assert re.search(
        r"^#define[ \t]+" + macro + r"[ \t]+" + target + r"[ \t]*$",
        SDL_H, re.M
    ), "old SDL alias was not libc-equivalent: " + macro


def fnv1a(source: str) -> int:
    value = 2166136261
    for octet in source.encode("utf-8"):
        value = ((value ^ octet) * 16777619) & 0xFFFFFFFF
    return value


def restore_original(file: str, content: str) -> str:
    if file == "native_intro_clock.c":
        assert "#include <string.h>\n" in content
        content = content.replace("#include <string.h>\n", "", 1)
        content = content.replace(
            'strcmp(reason, "intro-exit-ready")',
            'SDL_strcmp(reason, "intro-exit-ready")', 1)
        content = content.replace(
            "memset(&clockState, 0, sizeof(clockState));",
            "SDL_memset(&clockState, 0, sizeof(clockState));", 1)
    elif file == "native_intro_dispose.c":
        assert "#include <stdlib.h>\n#include <string.h>\n" in content
        content = content.replace(
            "#include <stdlib.h>\n#include <string.h>\n", "", 1)
        content = content.replace("strlen(*text)", "SDL_strlen(*text)", 1)
        content = content.replace("free(*text)", "SDL_free(*text)", 1)
    return "#include <SDL.h>\n" + content


for file, expected in REFERENCE_FNV.items():
    content = (INTRO / file).read_text(encoding="utf-8")
    assert "#include <SDL.h>" not in content, file
    assert '#include "SDL_Video.h"' not in content, file
    uncommented = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", content)
    assert not re.search(r"\bSDL_\w+\b", uncommented), file
    actual = fnv1a(restore_original(file, content))
    assert actual == expected, (
        f"{file} changed outside the SDL-F8 alias/include boundary: "
        f"{actual:08x} != {expected:08x}"
    )

print("SDL-F8 PASS four native intro C sources SDL-free;"
      " four original FNVs exact; former SDL memory/string aliases equal libc")
