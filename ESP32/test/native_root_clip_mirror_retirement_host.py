#!/usr/bin/env python3
"""Root clip mirror retirement: real header ABI and SDL-owned clipping.

Actual mixed RGB565 pixel parity is exercised independently by
doomrpg_presentation_host.py, with 1600 reference/native operations.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/DoomRPG.h").read_text(encoding="latin-1")
DESKTOP = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
PRESENTATION = (ROOT / "ESP32/src/esp_doomrpg_presentation.c").read_text()
ROOT_OWNER = (ROOT / "ESP32/src/engine_stubs.c").read_text()

assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)"
    r"\s+boolean\s+graphSetCliping;\s+#endif", HEADER)
assert DESKTOP.count("graphSetCliping") == 3, "desktop clip state must not change"
assert "graphSetCliping" not in PRESENTATION
assert "doomRpg->graphSetCliping" not in ROOT_OWNER
assert '_Static_assert(sizeof(DoomRPG_t) == 260U,' in ROOT_OWNER
assert "SDL_RenderSetClipRect(sdlVideo.renderer, NULL);" in PRESENTATION
assert "SDL_RenderSetClipRect(sdlVideo.renderer, &clip);" in PRESENTATION
assert PRESENTATION.count("(void)doomrpg;") == 2

# Forbid revival in any native production/diagnostic source or header.
for root in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for path in root.rglob("*"):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        text = path.read_text(encoding="latin-1")
        text = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", text,
                      flags=re.S)
        assert not re.search(r"\bgraphSetCliping\b", text), path

cc = shutil.which("cc") or "cc"
with tempfile.TemporaryDirectory(prefix="native-root-clip-abi-") as tmp:
    path = Path(tmp) / "probe.c"
    path.write_text(
        '#include <SDL.h>\n#include "DoomRPG.h"\n'
        'int read_clip(DoomRPG_t* root) {return root->graphSetCliping;}\n')
    base = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fsyntax-only", "-IESP32/include", "-Isrc"]
    for name, flags, accepted in (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1",
                     "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1",
                   "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("geometry-oracle", ["-DDOOMRPG_ESP32=1",
                             "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], True),
        ("desktop", [], True),
    ):
        result = subprocess.run(base + flags + [str(path)], cwd=ROOT,
                                capture_output=True, text=True)
        assert (result.returncode == 0) == accepted, (name, result.stderr)
        if not accepted:
            assert "graphSetCliping" in result.stderr, name

print("PASS root graphSetCliping retired on CYD; desktop/oracle ABI retained; "
      "SDL clip calls untouched; root size=260B on 32-bit ESP32")
