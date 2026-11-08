#!/usr/bin/env python3
"""Guard the retirement of the desktop Render skipCull toggle on classic CYD.

The native first-frame and visibility paths always enable BSP frustum culling.
The extracted desktop geometry oracle retains the optional developer toggle
and continues exercising both values in its differential test.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "ESP32"
HEADER = (ROOT / "src/Render.h").read_text(encoding="latin-1")
BRIDGE = (NATIVE / "src/render_startup_bridge.c").read_text()
OWNER = (NATIVE / "src/engine_stubs.c").read_text()
GEOMETRY_H = (NATIVE / "include/esp_render_geometry_primitives.h").read_text()
GEOMETRY_C = (NATIVE / "src/esp_render_geometry_primitives.c").read_text()
ORACLE = (NATIVE / "test/render_geometry_host.c").read_text()

assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| "
    r"defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)\s+"
    r"boolean skipCull;\s+#endif", HEADER
), "desktop/oracle must preserve the original debug toggle"
assert "sizeof(Render_t) == 120U" in BRIDGE
assert "render->skipCull" not in OWNER
assert re.search(r"\bESP_NATIVE_BSP_CULL_STANDARD\s*=\s*0\b", GEOMETRY_H)
assert "if (view->skipCull)" in GEOMETRY_C
assert "native.skipCull = reference.skipCull = (i % 13 == 0);" in ORACLE

# Reject any persistent Render_t culling access from native C/C++ sources.
# The separate view->skipCull parameter in the pure culler is deliberately
# retained for host differential tests, not connected to the Render_t layout.
for path in sorted((NATIVE / "src").rglob("*")):
    if path.suffix not in (".c", ".cpp", ".h"):
        continue
    clean = re.sub(
        r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
        " ", path.read_text(encoding="latin-1"), flags=re.S
    )
    assert not re.search(
        r"\b(?:render|renderBase|rootRender|r)\s*->\s*skipCull\b|"
        r"\bwork\s*->\s*render\s*->\s*skipCull\b", clean
    ), ("retired Render field accessed by native source", path)

for name in ("esp_native_first_frame.c", "esp_native_bsp_visibility.c"):
    code = (NATIVE / "src" / name).read_text()
    assert code.count(".skipCull = ESP_NATIVE_BSP_CULL_STANDARD,") == 1, name
    assignments = re.findall(r"\.skipCull\s*=\s*([A-Za-z_][A-Za-z0-9_]*)", code)
    assert assignments == ["ESP_NATIVE_BSP_CULL_STANDARD"], (name, assignments)

# Compile the actual public header, rather than trusting only a textual
# guard. Negative compilations are expected: the field must not exist in any
# of the three firmware profiles, but must exist in the desktop/oracle ABI.
probe = (
    '#include <SDL.h>\n'
    '#include "DoomRPG.h"\n'
    '#include "Render.h"\n'
    'int read_skip_cull(Render_t* r) { return r->skipCull; }\n'
)
cc = shutil.which("cc") or "cc"
with tempfile.TemporaryDirectory(prefix="doomrpg-skipcull-host-") as tmp:
    path = Path(tmp) / "probe.c"
    path.write_text(probe, encoding="utf-8")
    base = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
            "-IESP32/include", "-Isrc"]
    profiles = (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("oracle", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], True),
        ("desktop", [], True),
    )
    for name, flags, expected in profiles:
        result = subprocess.run(
            base + flags + [str(path)], cwd=ROOT,
            text=True, capture_output=True, check=False
        )
        assert (result.returncode == 0) == expected, (
            name, result.stdout, result.stderr
        )
        if not expected:
            assert "skipCull" in result.stderr, (name, result.stderr)

print("PASS skipCull=desktop/oracle-only Render_t=140B native: "
      "three profile field guards, two explicit immutable BSP cull policies; "
      "10k-case geometry differential retained")
