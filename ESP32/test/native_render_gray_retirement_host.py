#!/usr/bin/env python3
"""Retire unused Render grayscale mutator from ESP32 without silent replacements.

The old desktop Render_setGrayPalettes modifies a shared palette plus legacy
map-wide plane texture arrays; these are not native ESP32 ownership. Assert no
native C/C++ call (including address-taking or bare references) survives. Keep
desktop body untouched for behavior reference, and allow only a future native
small-state grayscale effect in a separate milestone.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
native = ROOT / "ESP32"
render = (native / "src/render_startup_bridge.c").read_text(encoding="utf-8")
header = (ROOT / "src/Render.h").read_text(encoding="utf-8")
desktop = (ROOT / "src/Render.c").read_text(encoding="latin-1")
builder = (native / "scripts/build_engine.py").read_text(encoding="utf-8")
assert "void Render_setGrayPalettes(Render_t* render)" not in render
assert re.search(r"#ifndef DOOMRPG_ESP32\s+void Render_setGrayPalettes\(Render_t\* render\);\s+#endif", header)
assert "nativeGrayPaletteOwner=none" in builder
assert "void Render_setGrayPalettes(Render_t* render)" in desktop

def extract(source, signature):
    anchor = signature + "\n{"
    assert source.count(anchor) == 1
    start = source.index(anchor)
    depth = 0
    for i in range(source.index("{", start), len(source)):
        depth += (source[i] == "{") - (source[i] == "}")
        if depth == 0:
            return source[start:i + 1]
    raise AssertionError("Unterminated grayscale desktop body")

legacy = extract(desktop, "void Render_setGrayPalettes(Render_t* render)")
fnv = 2166136261
for b in legacy.encode("utf-8"):
    fnv = ((fnv ^ b) * 16777619) & 0xffffffff
assert fnv == 0x6ff4291e, f"Desktop grayscale body drift {fnv:08x}"
assert "render->mediaPalettes" in legacy
assert "render->floorColor[0]" in legacy
assert "render->ceilingColor[0]" in legacy
assert "mediaPlanes" in legacy and "#ifndef DOOMRPG_ESP32" in legacy
# strip comments and strings so only live references count; check all native
# source/header trees, not just currently linked component translation units.
for subtree in (native / "src", native / "include"):
    for source in sorted(subtree.rglob("*")):
        if source.suffix not in (".c", ".cpp", ".h"):
            continue
        raw = source.read_text(encoding="latin-1")
        clean = re.sub(
            r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
            " ", raw, flags=re.S)
        assert not re.search(r"\bRender_setGrayPalettes\b", clean), source

# Prevent a misleading replacement in any public native bridge API.
assert "EspNativeRenderStartup_setGrayPalettes(" not in render
print("PASS Render_setGrayPalettes retired; zero native references; desktop grayscale FNV=6ff4291e")
