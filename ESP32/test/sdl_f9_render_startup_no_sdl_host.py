#!/usr/bin/env python3
"""SDL-F9: native Render startup no longer depends on SDLVideo or SDL libc aliases.

Restore only the explicitly retired aliases, geometry mirror, and includes
against the exact pre-F9 main FNV. Keep older exact legacy-source oracles alive.
Three actual PlatformIO firmware builds remain the Xtensa compile authority.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "ESP32/src/render_startup_bridge.c"
REFERENCE_FNV = 0xBD5D5BED

OLD_GEOMETRY = """    /* Production canvas is a fixed native 160x120 owner. The legacy
     * Render_t::clipRect was only written here and compared to itself;
     * check the actual attached renderer dimensions before any I/O. */
    if (sdlVideo.rendererW != DOOMRPG_CANVAS_WIDTH ||
        sdlVideo.rendererH != DOOMRPG_CANVAS_HEIGHT) {
        printf("[RENDER] ERROR native canvas geometry %dx%d expected=%dx%d\\n",
               sdlVideo.rendererW, sdlVideo.rendererH,
               DOOMRPG_CANVAS_WIDTH, DOOMRPG_CANVAS_HEIGHT);
        return 0;
    }
"""
NEW_GEOMETRY = """    /* Classic CYD geometry is platform-owned and constant, not a mutable
     * SDLVideo_t mirror. The shared framebuffer capacity and pitch remain
     * validated below before writing any pixel. */
"""
ASSERT = """_Static_assert(DOOMRPG_CANVAS_WIDTH == DOOMRPG_LOGICAL_WIDTH &&
               DOOMRPG_CANVAS_HEIGHT == DOOMRPG_LOGICAL_HEIGHT,
               "render startup must use the native fixed-size CYD canvas");
"""


def replace_exact(text: str, old: str, new: str) -> str:
    assert text.count(old) == 1, (repr(old[:70]), text.count(old))
    return text.replace(old, new, 1)


def fnv1a(text: str) -> int:
    value = 2166136261
    for octet in text.encode("utf-8"):
        value = ((value ^ octet) * 16777619) & 0xFFFFFFFF
    return value


def restore_pre_f9(content: str) -> str:
    text = replace_exact(content, ASSERT, "")
    text = replace_exact(
        text, "#include \"esp_native_root_state.h\"\n"
              "#include <stdio.h>\n#include <stdint.h>\n"
              "#include <stdlib.h>\n#include <string.h>\n",
        "#include <SDL.h>\n#include \"esp_native_root_state.h\"\n"
        "#include <stdio.h>\n#include <stdint.h>\n#include <string.h>\n")
    text = replace_exact(
        text, "#include \"Render.h\"\n",
        "#include \"Render.h\"\n#include \"SDL_Video.h\"\n")
    text = replace_exact(text, NEW_GEOMETRY, OLD_GEOMETRY)
    text = replace_exact(
        text, "    width = DOOMRPG_CANVAS_WIDTH;\n"
              "    height = DOOMRPG_CANVAS_HEIGHT;\n",
        "    width = sdlVideo.rendererW;\n    height = sdlVideo.rendererH;\n")
    text = replace_exact(
        text,
        "    expectedPitch = ((DOOMRPG_CANVAS_WIDTH * (int)sizeof(uint16_t)) + 3) & ~3;",
        "    expectedPitch = ((sdlVideo.rendererW * (int)sizeof(uint16_t)) + 3) & ~3;")
    text = replace_exact(
        text, "               DOOMRPG_CANVAS_WIDTH, DOOMRPG_CANVAS_HEIGHT,\n"
              "               (void*)render->mediaPalettes,",
        "               sdlVideo.rendererW, sdlVideo.rendererH,\n"
        "               (void*)render->mediaPalettes,")
    text = re.sub(r"\bfree\(", "SDL_free(", text)
    text = re.sub(r"\bmalloc\(", "SDL_malloc(", text)
    return text


if __name__ == "__main__":
    src = SOURCE.read_text(encoding="utf-8")
    old = (ROOT / "ESP32/include/SDL.h").read_text(encoding="utf-8")
    for alias in ("free", "malloc"):
        assert re.search(r"^#define\s+SDL_" + alias + r"\s+" + alias + r"\s*$",
                         old, re.M), alias
    active = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", src)
    assert not re.search(r"\bSDL_\w+\b", active)
    assert "#include <SDL.h>" not in src
    assert '#include "SDL_Video.h"' not in src
    assert "sdlVideo." not in active
    assert "Esp32PlatformVideo_framebufferSizeBytes() < framebufferBytes" in src
    assert "render->framebuffer != expectedFramebuffer" in src
    assert "render->pitch != expectedPitch" in src
    assert "width = DOOMRPG_CANVAS_WIDTH;" in src
    assert "height = DOOMRPG_CANVAS_HEIGHT;" in src
    h = fnv1a(restore_pre_f9(src))
    assert h == REFERENCE_FNV, f"SDL-F9 drift outside exact boundary: {h:08x}"
    print("SDL-F9 PASS Render bridge SDL-free; fixed native CYD geometry;"
          " SDL macro aliases=libc; original FNV exact; shared FB guards remain")
