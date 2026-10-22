#!/usr/bin/env python3
"""Retire three desktop-only plane test entrypoints from ESP32 production.

The ESP32 renderer uses compact PAK-backed native plane owners. The desktop
_Test entrypoints dereference map-wide mediaTexels and must not be exposed or
linked in classic CYD firmware. Desktop Render.c remains the behavior oracle.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/Render.h").read_text(encoding="latin-1")
DESKTOP = (ROOT / "src/Render.c").read_text(encoding="latin-1")
NATIVE = ROOT / "ESP32"
REJECT_PATH = NATIVE / "src/esp_legacy_render_reject.c"
assert not REJECT_PATH.exists(), "retired legacy Render reject translation unit revived"
NAMES = (
    "Render_renderFloorAndCeilingBG_Test",
    "Render_drawPlane_Test",
    "Render_spanPlane_Test",
)

for name in NAMES:
    assert re.search(r"\bvoid\s+" + name + r"\s*\(", DESKTOP), name

# All three declarations must be in the same desktop-only guard.
declaration_block = re.search(
    r"#ifndef DOOMRPG_ESP32\s+"
    r"void Render_renderFloorAndCeilingBG_Test\([^;]+;\s+"
    r"void Render_drawPlane_Test\([^;]+;\s+"
    r"void Render_spanPlane_Test\([^;]+;\s+"
    r"#endif", HEADER
)
assert declaration_block, "plane test ABI must be desktop-only"

# Search all production source/header code, including bare function references,
# ignoring comments and string literals but not preprocessor directives.
for subtree in (NATIVE / "src", NATIVE / "include"):
    for path in sorted(subtree.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        raw = path.read_text(encoding="latin-1")
        clean = re.sub(
            r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
            " ", raw, flags=re.S
        )
        for name in NAMES:
            assert not re.search(r"\b" + name + r"\b", clean), (path, name)

print("PASS legacy Render plane _Test ABI retired: 3 symbols; desktop reference intact")
