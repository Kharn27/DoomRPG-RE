#!/usr/bin/env python3
"""Root-C: retire only the dormant desktop memoryBeg metric from CYD ABI.

Keep imageMemory live: native image acquire/release still adjusts that metric.
The desktop and ESP32 geometry oracle retain their legacy root structure.
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

assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)"
    r"\s+int\s+memoryBeg;\s+#endif", HEADER), "desktop-only ABI guard"
assert "doomRpg->memoryBeg = DoomRPG_freeMemory();" in DESKTOP
assert "doomRpg->memoryBeg" not in CORE
assert "_Static_assert(sizeof(DoomRPG_t) == 28U," in CORE
assert "doomRpg->imageMemory = 0;" in CORE
assert "doomrpg->imageMemory +=" in IMAGES
assert "doomrpg->imageMemory -=" in IMAGES
assert not re.search(r"\bdoomRpg\s*->\s*errorID\b", CORE)
assert "doomRpg->graphSetCliping = false;" in CORE
assert "char errMsg[256];" in CORE
assert "SDL_calloc(1, sizeof(DoomRPG_t))" in CORE

# Fail closed for all native source/header files, even currently unlinked TUs.
count = 0
for root in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for path in sorted(root.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        count += 1
        source = path.read_text(encoding="latin-1")
        source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ",
                        source, flags=re.S)
        assert not re.search(r"\bmemoryBeg\b", source), path
assert count >= 300, "native inventory incomplete"

# Use the real header and compile each preprocessor ABI, not a fake layout.
cc = shutil.which("cc") or "cc"
with tempfile.TemporaryDirectory(prefix="root-memorybeg-abi-") as tmp:
    path = Path(tmp) / "probe.c"
    path.write_text(
        '#include <SDL.h>\n#include "DoomRPG.h"\n'
        'int root_metric(DoomRPG_t* r) { return r->memoryBeg; }\n')
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
            assert "memoryBeg" in result.stderr, name

print(f"PASS Root-C memoryBeg desktop/oracle only; "
      f"{count} native sources/headers guarded; imageMemory remains live; "
      "root expected 28B")
