#!/usr/bin/env python3
"""Retire the seven unused desktop Render BSP/map/event reject ABIs on ESP32.

The native map runtime, sprite topology, event lookup and renderer are
permanent owners. No legacy BSP traversal, map loader, or map-wide event array
may be revived by resurrecting these compatibility entrypoints.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "ESP32"
HEADER = (ROOT / "src/Render.h").read_text(encoding="latin-1")
DESKTOP = (ROOT / "src/Render.c").read_text(encoding="latin-1")
REJECT_PATH = NATIVE / "src/esp_legacy_render_reject.c"
assert not REJECT_PATH.exists(), "retired legacy Render reject translation unit revived"
RETIRED = (
    "Render_relinkSprite",
    "Render_renderBSPNoclip",
    "Render_findEventIndex",
    "Render_beginLoadMap",
    "Render_beginLoadMapData",
    "Render_renderBSP",
    "Render_walkNode",
)
REMAINING = (
    "Render_render",
    "Render_renderFloorAndCeilingBG",
    "Render_drawplane",
    "Render_spanPlane",
)

# All removed symbols keep their original desktop source implementations.
for name in RETIRED:
    assert re.search(r"\b(?:void|boolean|int)\s+" + name + r"\s*\(", DESKTOP), name

# C preprocessor exposure: each retired prototype must be behind its own or
# a group #ifndef DOOMRPG_ESP32 guard, not just absent from the native stubs.
for name in RETIRED:
    declaration = re.search(
        r"(?m)^[^\n]*\b" + name + r"\s*\([^;]*;", HEADER
    )
    assert declaration, "missing desktop prototype: " + name
    guard_open = HEADER.rfind("#ifndef DOOMRPG_ESP32", 0, declaration.start())
    guard_close = HEADER.rfind("#endif", 0, declaration.start())
    assert guard_open >= 0 and guard_open > guard_close, (
        "ESP32-visible retired prototype", name
    )
    assert HEADER.find("#endif", declaration.end()) >= 0, name

# The last four raster rejects have now been retired as a separate milestone.
# Their own host test checks desktop-only declarations and native absence.

# Check every production/bringup/trace C/C++ source + native header, including
# function pointers (not merely calls). Drop comments and strings so diagnostic
# wording never masquerades as a linker reference.
for subtree in (NATIVE / "src", NATIVE / "include"):
    for path in sorted(subtree.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        content = path.read_text(encoding="latin-1")
        clean = re.sub(
            r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
            " ", content, flags=re.S
        )
        for name in RETIRED:
            assert not re.search(r"\b" + name + r"\b", clean), (path, name)

print("PASS 7 legacy Render BSP/map/event reject ABIs retired, desktop oracle retained")
