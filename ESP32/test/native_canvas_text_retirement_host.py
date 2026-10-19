#!/usr/bin/env python3
"""Reject the obsolete desktop Canvas string entry; preserve native glyph loop."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
bridge = (SRC / "esp_legacy_doomcanvas_bridge.c").read_text(encoding="utf-8")
header = (ROOT / "ESP32/include/esp_native_canvas_text.h").read_text(encoding="utf-8")
legacy = (ROOT / "src/DoomCanvas.h").read_text(encoding="utf-8")
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text(encoding="utf-8")
menu = (SRC / "native_main_menu_actions.c").read_text(encoding="utf-8")
layout = (SRC / "native_main_menu_touch_layout.c").read_text(encoding="utf-8")

assert re.search(r"void\s+EspNativeCanvas_drawText\s*\(\s*DoomCanvas_t\s*\*\s*\w+\s*,\s*const char\s*\*\s*text", header)
assert "void EspNativeCanvas_drawText(DoomCanvas_t* doomCanvas,\n                              const char* text," in bridge
assert "DoomCanvas_drawString1" not in bridge
assert '"DoomCanvas_drawString1",' not in builder
assert 'INIT exports=4 nativeText=1 desktopTU=no' in bridge
assert re.search(r"#ifndef DOOMRPG_ESP32\s+void DoomCanvas_drawString1\(", legacy)
assert 'doomCanvas, "Loading...", DOOMRPG_CANVAS_CENTER_X' in bridge
assert "(char*)line" not in menu and "(char*)label" not in layout
assert "(char*)names[item]" not in layout
assert menu.count("EspNativeCanvas_drawText(") == 5
assert layout.count("EspNativeCanvas_drawText(") == 2

# Same body as the validated legacy-compatibility glyph routine, byte-for-byte.
a = bridge.index("void EspNativeCanvas_drawText(")
a = bridge.index("\n{", a)
b = bridge.index("\nstatic int doomCanvasBeginIntro(", a)
body = bridge[a:b]
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

print("PASS canvasExports=4 nativeText=const retiredText=0 glyphLoopFNV=b4296a8f")
