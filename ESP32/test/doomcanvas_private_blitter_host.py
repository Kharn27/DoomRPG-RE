#!/usr/bin/env python3
"""Audit retirement of public DoomCanvas image blitter without drawing drift.

The former exported DoomCanvas_drawImageSpecial has only one permanent
caller, DoomCanvas_drawString1, in the native bridge. Its complete body is
locked to the previous CYD hardware-proven implementation by FNV-1a.
A new external source caller is rejected at host test and ELF link time.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
bridge = (SRC / "esp_legacy_doomcanvas_bridge.c").read_text(encoding="utf-8")
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text(encoding="utf-8")

internal = "EspDoomCanvas_drawImageSpecial"
retired = "DoomCanvas_drawImageSpecial"
assert bridge.count("static void " + internal + "(") == 1
assert bridge.count(internal + "(") == 2, "one definition, one glyph caller"
assert "void " + retired + "(" not in bridge
assert retired + '",' not in builder  # absent from source ABI export inventory
assert '"DoomCanvas_drawString1",' in builder
assert "glyph blitter must be private" in builder
assert "void DoomCanvas_drawString1(" in bridge
assert 'INIT exports=5 desktopTU=no' in bridge
assert 'INIT exports=6 desktopTU=no' not in bridge
assert 'char loadingText[] = "Loading...";' in bridge
assert 'doomCanvas, loadingText, DOOMRPG_CANVAS_CENTER_X' in bridge
assert 'doomCanvas, "Loading...", DOOMRPG_CANVAS_CENTER_X' not in bridge
assert "SDL_RenderCopy(sdlVideo.renderer, img->imgBitmap, &clip, &renderQuad);" in bridge

# Extract identical actual source body, after the renamed function signature.
a = bridge.index("static void " + internal + "(")
a = bridge.index("\n{", a)
b = bridge.index("\nvoid DoomCanvas_drawString1(", a)
body = bridge[a:b]
assert body.startswith("\n{\n") and body.endswith("}\n")
h = 2166136261
for byte in body.encode("utf-8"):
    h = ((h ^ byte) * 16777619) & 0xFFFFFFFF
assert h == 0x4C4A9A93, "private blitter draw/clipping logic changed"

# No other production TU may call the retired desktop export, not even one
# that today's linker might discard. Descriptive comments don't count.
for path in sorted(SRC.rglob("*")):
    if path.suffix not in (".c", ".cpp"):
        continue
    text = path.read_text(encoding="latin-1")
    text = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", text, flags=re.S)
    assert not re.search(r"\b" + retired + r"\s*\(", text), (
        "retired DoomCanvas image ABI revived in " + str(path.relative_to(ROOT)))

# Avoid changing the externally-called text routine or the active
# init/free/state/startup roots as part of this tiny ABI boundary.
for name in ("init", "free", "drawString1", "setState", "startup"):
    assert "DoomCanvas_" + name + "(" in bridge
assert "DoomCanvas_drawString1(" in bridge
assert re.search(r"\bDoomCanvas_drawString1\s*\([^;]+\)\s*\{", bridge, re.S)

print("PASS Canvas public ABI=5, internal indexed glyph blitter bodyFNV=4c4a9a93, external calls=0")
