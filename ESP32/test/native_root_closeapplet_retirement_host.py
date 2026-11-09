#!/usr/bin/env python3
"""Root-D: keep legacy closeApplet only for desktop/geometry oracle.

The CYD menu/session transitions do not use this desktop process flag. Scan
even unlinked native sources, then compile the actual shared-header ABI.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/DoomRPG.h").read_text(encoding="latin-1")
DESKTOP = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
CORE = (ROOT / "ESP32/src/engine_stubs.c").read_text()
IMAGES = (ROOT / "ESP32/src/esp_doomrpg_image_resources.c").read_text()
INTRO = (ROOT / "ESP32/src/native_intro_dispose.c").read_text()

assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)"
    r"\s+boolean\s+closeApplet;\s+#endif", HEADER), "desktop-only ABI guard"
assert "doomRpg->closeApplet = false;" in DESKTOP
assert "doomrpg->closeApplet = true;" in DESKTOP
assert "doomRpg->closeApplet = true;" in DESKTOP
assert "closeApplet" not in CORE, "native process root must not own legacy flag"
assert "_Static_assert(sizeof(DoomRPG_t) == 192U," in CORE
assert "doomRpg->errorID = 0;" in CORE
assert "doomRpg->imageMemory = 0;" in CORE
assert "doomrpg->imageMemory +=" in IMAGES
assert "doomrpg->imageMemory -=" in IMAGES
assert "doomRpg->graphSetCliping = false;" in CORE
assert "!doomRpg->graphSetCliping" in INTRO
assert "char errMsg[256];" in CORE
assert "SDL_calloc(1, sizeof(DoomRPG_t))" in CORE

# Guard all currently linked and unlinked C/C++ TUs and native headers.
count = 0
for root in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for path in sorted(root.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        count += 1
        source = path.read_text(encoding="latin-1")
        source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ",
                        source, flags=re.S)
        assert not re.search(r"\bcloseApplet\b", source), path
assert count >= 300, "incomplete native inventory"

cc = shutil.which("cc") or "cc"
with tempfile.TemporaryDirectory(prefix="root-closeapplet-abi-") as tmp:
    path = Path(tmp) / "probe.c"
    path.write_text(
        '#include <SDL.h>\n#include "DoomRPG.h"\n'
        'int root_flag(DoomRPG_t* r) { return r->closeApplet; }\n')
    cmd = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
           "-fsyntax-only", "-IESP32/include", "-Isrc"]
    for name, flags, expected in (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1",
                     "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1",
                   "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("oracle", ["-DDOOMRPG_ESP32=1",
                    "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], True),
        ("desktop", [], True),
    ):
        result = subprocess.run(cmd + flags + [str(path)], cwd=ROOT,
                                capture_output=True, text=True)
        assert (result.returncode == 0) == expected, (name, result.stderr)
        if not expected:
            assert "closeApplet" in result.stderr, name

print(f"PASS Root-D closeApplet desktop/oracle only; "
      f"{count} native files guarded; root expected 192B")
