#!/usr/bin/env python3
"""Root-D/B: remove 6 desktop subsystem pointers and 2 dormant root scalars.

Keep desktop/oracle ABI intact and refuse any source-level CYD owner re-entry,
including unlinked TUs. Verify native size through the actual shared header.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/DoomRPG.h").read_text(encoding="latin-1")
CORE = (ROOT / "ESP32/src/engine_stubs.c").read_text()
IMAGES = (ROOT / "ESP32/src/esp_doomrpg_image_resources.c").read_text()
INTRO = (ROOT / "ESP32/src/native_intro_dispose.c").read_text()
RETIRED = ("entityDef", "combat", "hud", "menu", "particleSystem",
           "sound", "errorID", "upTimeMs")
LIVE = ("random", "doomCanvas", "render", "player", "game",
        "menuSystem", "graphSetCliping", "imageMemory")
GUARD = r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)"

assert "_Static_assert(sizeof(DoomRPG_t) == 160U," in CORE
assert "SDL_calloc(1, sizeof(DoomRPG_t))" in CORE
assert "DoomRPG_FreeAppData(DoomRPG_t* doomrpg)" in CORE
assert "char errMsg[256];" in CORE
assert "doomRpg->graphSetCliping = false;" in CORE
assert "!doomRpg->graphSetCliping" in INTRO
assert "doomRpg->imageMemory = 0;" in CORE
assert "doomrpg->imageMemory +=" in IMAGES
assert "doomrpg->imageMemory -=" in IMAGES
assert "doomRpg->player != NULL" in CORE
assert "doomRpg->game != NULL" in CORE
assert "EspNativeMenuStorage_free(doomrpg->menuSystem" in CORE
assert "EspNativeEngineRoot_releaseRender(doomrpg->render" in CORE
assert "EspNativeEngineRoot_releaseCanvas(doomrpg->doomCanvas" in CORE
for field in RETIRED:
    assert re.search(
        GUARD + r"(?:(?!#endif).)*\b" + field +
        r"\s*;(?:(?!#endif).)*#endif", HEADER, re.S), (
        "desktop/oracle guard missing", field)

# Source scan deliberately examines 350+ native files, not just link-selected
# translation units; string and comment matches are not executable accesses.
file_count = 0
for base in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for path in sorted(base.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        file_count += 1
        source = path.read_text(encoding="latin-1")
        source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                        " ", source, flags=re.S)
        for field in RETIRED:
            # Native menu model still uses menuSystem->menu; only the
            # DoomRPG_t::menu legacy pointer is retired in this milestone.
            if field in ("menu", "combat"):
                # menuSystem->menu and CoreMetrics->combat are live fields
                # unrelated to the retired DoomRPG_t root pointers.
                access = re.search(
                    r"\b(?:doomRpg|doomrpg)\s*->\s*" + field + r"\b",
                    source)
            else:
                access = re.search(r"->\s*" + field + r"\b", source)
            assert access is None, (path.relative_to(ROOT), field)
assert file_count >= 300, "source/header inventory incomplete"

cc = shutil.which("cc") or "cc"
with tempfile.TemporaryDirectory(prefix="native-root-group-") as tmp:
    probe = Path(tmp) / "probe.c"
    cmd = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
           "-IESP32/include", "-Isrc"]
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
        if not expected:
            probe.write_text(
                '#include <SDL.h>\n#include "DoomRPG.h"\n'
                '_Static_assert(sizeof(DoomRPG_t) == 160, "bad root ABI");\n')
            result = subprocess.run(cmd + flags + [str(probe)], cwd=ROOT,
                                    capture_output=True, text=True)
            assert result.returncode == 0, (name, result.stderr)
        for field in RETIRED:
            probe.write_text(
                '#include <SDL.h>\n#include "DoomRPG.h"\n'
                'int root_probe(DoomRPG_t* r) { return (int)(r->' + field
                + ' != 0); }\n')
            result = subprocess.run(cmd + flags + [str(probe)], cwd=ROOT,
                                    capture_output=True, text=True)
            assert (result.returncode == 0) == expected, (
                name, field, result.stderr)
            if not expected:
                assert field in result.stderr, (name, field)
        for field in LIVE:
            probe.write_text(
                '#include <SDL.h>\n#include "DoomRPG.h"\n'
                'int keep_probe(DoomRPG_t* r) { return (int)sizeof(r->'
                + field + '); }\n')
            result = subprocess.run(cmd + flags + [str(probe)], cwd=ROOT,
                                    capture_output=True, text=True)
            assert result.returncode == 0, (name, field, result.stderr)

print(f"PASS Root-D/B retired={','.join(RETIRED)} freed=32B "
      f"nativeRoot=160B nativeFiles={file_count} "
      "desktop/oracle=unchanged liveRoots=protected")
