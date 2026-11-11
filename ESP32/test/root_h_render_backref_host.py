#!/usr/bin/env python3
"""Root-H: renderer has no DoomRPG_t back-pointer on classic CYD.

Scan production sources and test actual header field visibility on each ABI.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/Render.h").read_text(encoding="latin-1")
STARTUP = (ROOT / "ESP32/src/render_startup_bridge.c").read_text()
ENGINE = (ROOT / "ESP32/src/engine_stubs.c").read_text()
assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)"
    r"\s+struct DoomRPG_s\* doomRpg;\s+#endif", HEADER)
assert "_Static_assert(sizeof(Render_t) == 108U" in STARTUP
assert "render->doomRpg" not in STARTUP
assert "render->doomRpg" not in ENGINE
assert "EspNativeEngineRoot_initRender(Render_t* render)" in ENGINE
assert "EspNativeEngineRoot_initRender(NULL)" in ENGINE
assert "DoomRPG_fileOpenRead(" not in STARTUP
assert 'EspLegacyAssetSource_readAlloc("sintable.bin", NULL)' in STARTUP
assert 'EspLegacyAssetSource_readAlloc("palettes.bin", NULL)' in STARTUP
assert 'EspNativeRootOwners_live()->doomCanvas == NULL' in STARTUP
assert "render->framebuffer = sharedFramebuffer;" in STARTUP

files = 0
for base in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for path in sorted(base.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        files += 1
        source = path.read_text(encoding="latin-1")
        source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ",
                        source, flags=re.S)
        assert re.search(r"\b(?:render|renderer)\s*(?:->|\.)\s*doomRpg\b",
                         source) is None, path
assert files >= 300, files

cc = shutil.which("cc") or "cc"
with tempfile.TemporaryDirectory(prefix="root-h-render-") as directory:
    probe = Path(directory) / "probe.c"
    probe.write_text(
        '#include <SDL.h>\n#include "DoomRPG.h"\n#include "Render.h"\n'
        'int probe(Render_t* r) { return r->doomRpg != 0; }\n')
    for name, flags, visible in (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("oracle", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], True),
        ("desktop", [], True),
    ):
        result = subprocess.run(
            [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
             "-IESP32/include", "-Isrc", *flags, str(probe)],
            cwd=ROOT, capture_output=True, text=True)
        assert (result.returncode == 0) == visible, (name, result.stderr)
        if not visible:
            assert "doomRpg" in result.stderr, (name, result.stderr)
print(f"Root-H PASS: Render_t backref retired (normal/bringup/trace), "
      f"oracle/desktop preserved; {files} sources scanned")
