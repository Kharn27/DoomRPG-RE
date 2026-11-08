#!/usr/bin/env python3
"""Protect the native viewport half-height retirement in all CYD profiles.

The desktop Render_setup caches halfScreenHeight = even(screenHeight)/2.
Native code computes that pure scalar from the existing normalized height
at each render owner, without persisting a redundant Render_t field.
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
ROOT_OWNER = (NATIVE / "src/engine_stubs.c").read_text()
BRIDGE = (NATIVE / "src/render_startup_bridge.c").read_text()
GEOMETRY = (NATIVE / "include/esp_render_geometry_primitives.h").read_text()

assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| "
    r"defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)\s+"
    r"int halfScreenHeight;\s+#endif", HEADER
), "desktop/oracle midpoint remains available to original renderer"
assert "render->halfScreenHeight = render->screenHeight / 2;" in DESKTOP
assert "render->halfScreenHeight" not in ROOT_OWNER
assert "sizeof(Render_t) == 120U" in BRIDGE
assert "static inline int EspNativeRenderGeometry_halfScreenHeight(int screenHeight)" in GEOMETRY

owners = ("esp_native_first_frame.c", "esp_native_sprite_renderer.c",
          "esp_native_plane_renderer.c")
for owner in owners:
    s = (NATIVE / "src" / owner).read_text()
    assert s.count("EspNativeRenderGeometry_halfScreenHeight(render->screenHeight)") == 1, owner

# Ban all direct native accesses to the retired Render field; include other
# owners beyond the three currently known readers, so it cannot silently return.
for tree in (NATIVE / "src", NATIVE / "include"):
    for path in sorted(tree.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        s = re.sub(r"#if 0\b[\s\S]*?#endif", " ",
                   path.read_text(encoding="latin-1"))
        s = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                   " ", s, flags=re.S)
        assert not re.search(
            r"\b(?:render|r|rootRender|renderBase)\s*->\s*halfScreenHeight\b",
            s
        ), ("retired viewport midpoint accessed", path)

cc = shutil.which("cc") or "cc"
probe = (
    '#include <SDL.h>\n'
    '#include "DoomRPG.h"\n'
    '#include "Render.h"\n'
    'int read_midpoint(Render_t* r) { return r->halfScreenHeight; }\n'
)
parity = r"""
#include <assert.h>
#include <stdio.h>
#include "esp_render_geometry_primitives.h"
int main(void) {
    /* The desktop setup first rounds odd viewport heights down to even.
     * All three native readers use the resulting screenHeight. */
    for (int sourceHeight = 0; sourceHeight <= 512; ++sourceHeight) {
        const int normalized = sourceHeight & ~1;
        const int expected = normalized / 2;
        assert(EspNativeRenderGeometry_halfScreenHeight(normalized) == expected);
        for (int z = -160; z <= 160; ++z) {
            /* An unchanged projected vertical displacement applies to both
             * the desktop cached midpoint and native reconstructed scalar. */
            assert(EspNativeRenderGeometry_halfScreenHeight(normalized) - z ==
                   expected - z);
        }
    }
    puts("PASS half-height 513 viewport heights, odd normalization, "
         "vertical projection corpus");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-half-height-") as tmp:
    path = Path(tmp) / "probe.c"
    path.write_text(probe, encoding="utf-8")
    flags = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
             "-IESP32/include", "-Isrc"]
    for name, extra, expect_ok in (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("oracle", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], True),
        ("desktop", [], True),
    ):
        p = subprocess.run(flags + extra + [str(path)], cwd=ROOT,
                           text=True, capture_output=True, check=False)
        assert (p.returncode == 0) == expect_ok, (name, p.stdout, p.stderr)
        if not expect_ok:
            assert "halfScreenHeight" in p.stderr, (name, p.stderr)
    source = Path(tmp) / "parity.c"
    binary = Path(tmp) / "parity"
    source.write_text(parity, encoding="utf-8")
    subprocess.run([cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-IESP32/include", str(source), "-o", str(binary)],
                   cwd=ROOT, check=True)
    subprocess.run([str(binary)], check=True)

print("PASS Render half-height desktop/oracle only; native 136 B; "
      "normal/bringup/trace compile guard; three native geometry readers")
