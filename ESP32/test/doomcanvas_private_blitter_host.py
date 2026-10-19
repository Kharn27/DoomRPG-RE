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
bridge = (SRC / "esp_native_canvas_text.c").read_text(encoding="utf-8")
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
assert 'INIT exports=0 nativeText=1 desktopTU=no' in (SRC / "engine_stubs.c").read_text(encoding="utf-8")
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

# The text entry is native. No desktop Canvas lifecycle ABI survives.
assert "DoomCanvas_free(" not in bridge
assert "doom_canvas_bridge_exports" not in builder
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
assert (SRC / "native_main_menu_touch.c").read_text().count("EspNativeMainMenu_transitionCanvasState(") == 1
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


# The remaining one-shot layout startup is owned directly by the process
# layout stage. Pin its COMPLETE source body after name normalization to the
# hardware-tested predecessor, not merely its call-site or rendered pixels.
root_owner = (SRC / "engine_stubs.c").read_text(encoding="utf-8")
legacy_canvas_h = (ROOT / "src/DoomCanvas.h").read_text(encoding="utf-8")
assert '"DoomCanvas_startup",' not in builder
assert "DoomCanvas_startup(" not in bridge
assert "static void EspNativeEngineLayout_startupCanvas(" in root_owner
assert root_owner.count("EspNativeEngineLayout_startupCanvas(") == 2  # definition + single call
assert re.search(r"#ifndef DOOMRPG_ESP32\s+void DoomCanvas_startup\(", legacy_canvas_h)
start = root_owner.index("static void EspNativeEngineLayout_startupCanvas(")
end = root_owner.index("\nint DoomRPG_startEngineLayout(", start)
layout = root_owner[start:end]
layout = layout.replace("static void EspNativeEngineLayout_startupCanvas(", "void DoomCanvas_startup(")
layout_fnv = 2166136261
for byte in layout.encode("utf-8"):
    layout_fnv = ((layout_fnv ^ byte) * 16777619) & 0xFFFFFFFF
assert layout_fnv == 0x9166e429, f"Canvas layout startup body drift: {layout_fnv:08x}"

# No CYD C/C++ unit may revive a direct call to the retired Canvas layout ABI.
for path in sorted(SRC.rglob("*")):
    if path.suffix not in (".c", ".cpp"):
        continue
    source = path.read_text(encoding="latin-1")
    source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", source, flags=re.S)
    assert not re.search(r"\bDoomCanvas_startup\s*\(", source), (
        "retired Canvas startup ABI revived in " + str(path.relative_to(ROOT)))


# Native root now owns the only linked Canvas allocation/zeroing function.
# Preserve the exact allocation path, field defaults and diagnostic payload
# after normalizing the private function name and ABI export count.
assert "DoomCanvas_init(" not in bridge
assert '"DoomCanvas_init",' not in builder
assert "DoomCanvas_t* DoomCanvas_init(" not in root_owner
assert root_owner.count("EspNativeEngineRoot_initCanvas(") == 2  # definition + one call
assert re.search(r"#ifndef DOOMRPG_ESP32\s+DoomCanvas_t\* DoomCanvas_init\(", legacy_canvas_h)
assert "ESP32_DOOMCANVAS_COMPACT_BYTES" in (ROOT / "ESP32/include/engine_metrics.h").read_text()
first = root_owner.index("static DoomCanvas_t* EspNativeEngineRoot_initCanvas(")
last = root_owner.index("int DoomRPG_initEngineCore(", first)
normalized = root_owner[first:last].replace(
    "static DoomCanvas_t* EspNativeEngineRoot_initCanvas(", "DoomCanvas_t* DoomCanvas_init(").replace(
    "[DOOMCANVASBRIDGE] INIT exports=0", "[DOOMCANVASBRIDGE] INIT exports=2")
init_fnv = 2166136261
for byte in normalized.encode("utf-8"):
    init_fnv = ((init_fnv ^ byte) * 16777619) & 0xFFFFFFFF
assert init_fnv == 0xb621a796, f"Canvas root init body drift: {init_fnv:08x}"

for path in sorted(SRC.rglob("*")):
    if path.suffix not in (".c", ".cpp"):
        continue
    source = path.read_text(encoding="latin-1")
    source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", source, flags=re.S)
    assert not re.search(r"\bDoomCanvas_init\s*\(", source), (
        "retired Canvas init ABI revived in " + str(path.relative_to(ROOT)))

print("PASS Canvas ABI=0 native const text glyphBlitterFNV=4c4a9a93")
