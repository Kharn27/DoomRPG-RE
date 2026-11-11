#!/usr/bin/env python3
"""Reject the obsolete desktop Canvas string entry; preserve native glyph loop."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
bridge = (SRC / "esp_native_canvas_text.c").read_text(encoding="utf-8")
header = (ROOT / "ESP32/include/esp_native_canvas_text.h").read_text(encoding="utf-8")
legacy = (ROOT / "src/DoomCanvas.h").read_text(encoding="utf-8")
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text(encoding="utf-8")
menu = (SRC / "native_main_menu_actions.c").read_text(encoding="utf-8")
layout = (SRC / "native_main_menu_touch_layout.c").read_text(encoding="utf-8")

assert re.search(r"void\s+EspNativeCanvas_drawText\s*\(\s*DoomCanvas_t\s*\*\s*\w+\s*,\s*const char\s*\*\s*text", header)
assert "void EspNativeCanvas_drawText(DoomCanvas_t* doomCanvas,\n                              const char* text," in bridge
assert "DoomCanvas_free(" not in bridge
assert "DoomCanvas_drawString1" not in bridge
assert '"DoomCanvas_drawString1",' not in builder
assert 'INIT exports=0 nativeText=1 desktopTU=no' in (SRC / "engine_stubs.c").read_text(encoding="utf-8")
assert re.search(r"#ifndef DOOMRPG_ESP32\s+void DoomCanvas_drawString1\(", legacy)
assert 'doomCanvas, "Loading...", DOOMRPG_CANVAS_CENTER_X' in (SRC / "native_main_menu_model.c").read_text(encoding="utf-8")
assert "(char*)line" not in menu and "(char*)label" not in layout
assert "(char*)names[item]" not in layout
assert menu.count("EspNativeCanvas_drawText(") == 13  # removed obsolete WEAPON SELECT footer; Canvas ABI unchanged
# All child cards now use the same clipped industrial glyph painter.
# Native Canvas ABI itself remains byte-for-byte unchanged and call-free here.
assert layout.count("EspNativeCanvas_drawText(") == 0
assert "EspMainMenuSkin_paintText(" in layout
assert "EspMainMenuSkin_paintPlate(" in layout

# Same body as the validated legacy-compatibility glyph routine, byte-for-byte.
a = bridge.index("void EspNativeCanvas_drawText(")
a = bridge.index("\n{", a)
b = bridge.index("\n#if defined(DOOMRPG_ESP32_BRINGUP_PROBES)", a)
body = bridge[a:b]
# SDL-D1: the only intentional difference is the native owner of font tint.
# Normalize that call before retaining the legacy glyph-loop FNV gate.
assert body.count("EspNativeTexture_setColorMod(imgFont->imgBitmap, r, g, b);") == 1
body = body.replace("EspNativeTexture_setColorMod(imgFont->imgBitmap, r, g, b);",
                    "SDL_SetTextureColorMod(imgFont->imgBitmap, r, g, b);")
# F7: SDL_strlen was libc strlen; retain the unchanged glyph-loop FNV.
body = body.replace("len = (int)strlen(text);", "len = (int)SDL_strlen(text);")
h = 2166136261
for byte in body.encode("utf-8"):
    h = ((h ^ byte) * 16777619) & 0xFFFFFFFF
assert h == 0xb4296a8f, "Canvas text raster changed"

# Fail closed on any leftover C or C++ caller, including currently inactive TUs.
for path in sorted(SRC.rglob("*")):
    if path.suffix not in (".c", ".cpp"):
        continue
    source = path.read_text(encoding="latin-1")
    source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", source, flags=re.S)
    assert not re.search(r"\bDoomCanvas_drawString1\s*\(", source), (
        "desktop text ABI reintroduced in " + str(path.relative_to(ROOT)))

print("PASS canvasExports=0 nativeText=const retiredText=0 glyphLoopFNV=b4296a8f")
