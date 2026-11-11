#!/usr/bin/env python3
"""Prevent Render_t::clipRect from re-entering any CYD firmware ABI.

The legacy Render_startup wrote a full-screen SDL_Rect and checked that
the same cached rectangle still held the constants it had just written.
Native startup checks the *actual* SDL renderer dimensions instead.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "ESP32"
HEADER = (ROOT / "src/Render.h").read_text(encoding="latin-1")
DESKTOP = (ROOT / "src/Render.c").read_text(encoding="latin-1")
BRIDGE = (NATIVE / "src/render_startup_bridge.c").read_text(encoding="utf-8")
STARTUP = (NATIVE / "test/native_render_startup_retirement_host.py").read_text(encoding="utf-8")

assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| "
    r"defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)\s+"
    r"SDL_Rect clipRect;\s+#endif", HEADER
), "desktop/oracle keep original clipping member"
assert "render->clipRect.x" in DESKTOP
assert "render->clipRect.w" in DESKTOP
assert "sizeof(Render_t) == 108U" in BRIDGE
assert "sdlVideo.rendererW != DOOMRPG_CANVAS_WIDTH" in BRIDGE
assert "sdlVideo.rendererH != DOOMRPG_CANVAS_HEIGHT" in BRIDGE
assert "Esp32PlatformVideo_framebufferSizeBytes() < framebufferBytes" in BRIDGE
assert "sdlVideo.rendererW=159" in STARTUP
assert "sdlVideo.rendererH=119" in STARTUP

# Forbid direct field access from the entire native firmware, not only from
# the startup bridge. Strip comments and strings but keep active code.
for tree in (NATIVE / "src", NATIVE / "include"):
    for path in sorted(tree.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        code = re.sub(r"#if 0\b[\s\S]*?#endif", " ",
                      path.read_text(encoding="latin-1"))
        code = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                      " ", code, flags=re.S)
        assert not re.search(
            r"\b(?:render|r|rootRender|renderBase)\s*->\s*clipRect\b",
            code
        ), ("retired Render clipping member accessed", path)

cc = shutil.which("cc") or "cc"
probe = (
    '#include <SDL.h>\n'
    '#include "DoomRPG.h"\n'
    '#include "Render.h"\n'
    'int read_rect(Render_t* r) { return r->clipRect.w; }\n'
)
with tempfile.TemporaryDirectory(prefix="doomrpg-render-clip-") as tmp:
    path = Path(tmp) / "probe.c"
    path.write_text(probe, encoding="utf-8")
    flags = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
             "-fsyntax-only", "-IESP32/include", "-Isrc"]
    for name, extra, allowed in (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1",
                     "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1",
                   "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("oracle", ["-DDOOMRPG_ESP32=1",
                    "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], True),
        ("desktop", [], True),
    ):
        result = subprocess.run(flags + extra + [str(path)],
                                cwd=ROOT, capture_output=True, text=True)
        assert (result.returncode == 0) == allowed, (
            name, result.stdout, result.stderr
        )
        if not allowed:
            assert "clipRect" in result.stderr, (name, result.stderr)

print("PASS Render_t clipRect absent from normal/bringup/trace, retained "
      "desktop/oracle; real canvas dimensions guarded, size=112B")
