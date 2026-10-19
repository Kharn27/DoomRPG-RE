#!/usr/bin/env python3
"""Audit retirement of public DoomCanvas image blitter without drawing drift.

The former exported DoomCanvas_drawImageSpecial has only one permanent
caller, EspNativeCanvas_drawText, in the native bridge. Its complete body is
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
assert '"DoomCanvas_drawString1",' not in builder
assert '"DoomCanvas_setState",' not in builder
assert "glyph blitter must be private" in builder
assert "void EspNativeCanvas_drawText(" in bridge
assert 'INIT exports=3 nativeText=1 desktopTU=no' in bridge
assert 'INIT exports=6 desktopTU=no' not in bridge
assert 'doomCanvas, "Loading...", DOOMRPG_CANVAS_CENTER_X' in (SRC / "native_main_menu_model.c").read_text(encoding="utf-8")
assert "SDL_RenderCopy(sdlVideo.renderer, img->imgBitmap, &clip, &renderQuad);" in bridge

# Extract identical actual source body, after the renamed function signature.
a = bridge.index("static void " + internal + "(")
a = bridge.index("\n{", a)
b = bridge.index("\nvoid EspNativeCanvas_drawText(", a)
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

# The text entry is native; only three compatibility roots remain.
for name in ("init", "free", "startup"):
    assert "DoomCanvas_" + name + "(" in bridge
assert "DoomCanvas_drawString1(" not in bridge
assert re.search(r"\bEspNativeCanvas_drawText\s*\([^;]+\)\s*\{", bridge, re.S)


# Transition byte bodies are unchanged; only ownership and symbol are native.
owner = (SRC / "native_main_menu_model.c").read_text(encoding="utf-8")
model_header = (ROOT / "ESP32/include/native_main_menu_model.h").read_text(encoding="utf-8")
assert "void EspNativeMainMenu_transitionCanvasState(" in owner
assert "void EspNativeMainMenu_transitionCanvasState(" in model_header
assert "DoomCanvas_setState(" not in bridge
assert owner.count("EspNativeMainMenu_transitionCanvasState(") == 2  # definition + MENU
assert (SRC / "native_main_menu_start_action.c").read_text().count("EspNativeMainMenu_transitionCanvasState(") == 1
assert (SRC / "native_main_menu_actions.c").read_text().count("EspNativeMainMenu_transitionCanvasState(") == 1
begin = owner.index("static int doomCanvasBeginIntro(")
end = owner.index("\n#define MAIN_HELP_ASSET_NAME", begin)
state_body = owner[begin:end].replace("EspNativeMainMenu_transitionCanvasState", "DoomCanvas_setState")
state_fnv = 2166136261
for byte in state_body.encode("utf-8"):
    state_fnv = ((state_fnv ^ byte) * 16777619) & 0xFFFFFFFF
assert state_fnv == 0x2f165f3f, f"Native state handoff body drift: {state_fnv:08x}"

# Reject retired desktop state callers even if the TU is currently unlinked.
for path in sorted(SRC.rglob("*")):
    if path.suffix not in (".c", ".cpp"):
        continue
    source = path.read_text(encoding="latin-1")
    source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", source, flags=re.S)
    assert not re.search(r"\b" + "DoomCanvas_setState" + r"\s*\(", source), (
        "desktop Canvas state ABI revived in " + str(path.relative_to(ROOT)))

print("PASS Canvas ABI=3 native const text glyphBlitterFNV=4c4a9a93")
