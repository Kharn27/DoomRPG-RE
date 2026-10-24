#!/usr/bin/env python3
"""Retirement guard for seventeen desktop-only raster/background/geometry ABIs.

Classic CYD uses the compact native BSP/world/plane renderers. The old raster
reject-only TU has no remaining legitimate symbol, and must not be revived.
Desktop Render.c continues to define the original behavior/reference.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "ESP32"
HEADER = (ROOT / "src/Render.h").read_text(encoding="latin-1")
DESKTOP = (ROOT / "src/Render.c").read_text(encoding="latin-1")
REJECT_PATH = NATIVE / "src/esp_legacy_render_reject.c"

RETIRED = (
    "Render_render",
    "Render_renderFloorAndCeilingBG",
    "Render_drawplane",
    "Render_spanPlane",
    "Render_renderFloorAndCeilingSolidBG",
    "Render_getSpanMode",
    "Render_SpanMode0",
    "Render_SpanMode1",
    "Render_SpanMode2",
    "Render_SpanMode3",
    "Render_SpanMode4",
    "Render_SpanMode5",
    "Render_SpanMode6",
    "Render_SpanMode7",
    "Render_SpanMode8",
    "Render_SpanMode9",
    "Render_occludeClippedLine",
)
LIVE_GEOMETRY = (
    "Render_initColumnScale",
    "Render_cullBoundingBox",
    "Render_transform2DVerts",
    "Render_clipLine",
    "Render_clipVertex",
    "Render_projectVertex",
)

assert not REJECT_PATH.exists(), "obsolete reject-only translation unit returned"
assert (NATIVE / "src/esp_native_plane_renderer.c").is_file()
geometry = (NATIVE / "src/esp_render_geometry_primitives.c").read_text(
    encoding="utf-8"
)

for name in RETIRED:
    # The original desktop implementation must remain authoritative.
    assert re.search(r"\bvoid\s+" + name + r"\s*\(", DESKTOP), name
    declaration = re.search(
        r"(?m)^[^\n]*\b" + name + r"\s*\([^;]*;", HEADER
    )
    assert declaration, "desktop declaration missing: " + name
    # The last still-open preprocessing guard is #ifndef DOOMRPG_ESP32.
    open_guard = HEADER.rfind("#ifndef DOOMRPG_ESP32", 0, declaration.start())
    last_close = HEADER.rfind("#endif", 0, declaration.start())
    assert open_guard > last_close, "ESP32-visible retired raster/span ABI: " + name
    assert HEADER.find("#endif", declaration.end()) >= 0, name

# The retired Render_t occlusion entrypoint is replaced by an explicit
# bounded column-scratch contract, consumed at both BSP render sites.
native_header = (NATIVE / "include/esp_render_geometry_primitives.h").read_text()
assert "Render_t" not in native_header and "Line_t" not in native_header
assert "EspNativeRenderGeometry_occludeProjectedSpan" in native_header
assert re.search(r"\\bEspNativeRenderGeometry_occludeProjectedSpan\\s*\\(", geometry)
for owner in ("esp_native_first_frame.c", "esp_native_bsp_visibility.c"):
    assert "EspNativeRenderGeometry_occludeProjectedSpan" in (NATIVE / "src" / owner).read_text()

# Permanent native geometry ownership must survive this source retirement.
for name in LIVE_GEOMETRY:
    assert re.search(r"\b" + name + r"\b", HEADER), name
    assert re.search(r"\b" + name + r"\s*\(", geometry), name

# Check every production/bringup/trace C/C++ translation unit and header.
# Bare address-taking, macro expansions and calls are all forbidden.
for subtree in (NATIVE / "src", NATIVE / "include"):
    for path in sorted(subtree.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        source = path.read_text(encoding="latin-1")
        code = re.sub(
            r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
            " ", source, flags=re.S,
        )
        for name in RETIRED:
            assert not re.search(r"\b" + name + r"\b", code), (
                "retired desktop raster/span reference", path, name
            )

print("PASS 17 retired Render ABIs; six legacy geometry primitives and native column occlusion preserved")
