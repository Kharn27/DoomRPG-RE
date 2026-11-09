#!/usr/bin/env python3
"""Retire desktop plane-texture flag from the ESP32 canvas ABI, preserving oracle."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/DoomCanvas.h").read_text(encoding="latin-1")
DESKTOP = (ROOT / "src/Render.c").read_text(encoding="latin-1")
CANVAS = (ROOT / "src/DoomCanvas.c").read_text(encoding="latin-1")
NATIVE = (ROOT / "ESP32/src/engine_stubs.c").read_text()
METRICS = (ROOT / "ESP32/include/engine_metrics.h").read_text()
assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)"
    r"\s+boolean renderFloorCeilingTextures;\s*// New\s+#endif", HEADER)
assert "doomCanvas->renderFloorCeilingTextures = true;" in CANVAS
assert DESKTOP.count("renderFloorCeilingTextures") == 2
assert "renderFloorCeilingTextures" not in NATIVE
assert "ESP32_DOOMCANVAS_RETIRED_PLANE_GATE_BYTES 4U" in METRICS
assert "ESP32_DOOMCANVAS_RETIRED_PLANE_GATE_BYTES)" in METRICS
assert "sizeof(DoomCanvas_t) == ESP32_DOOMCANVAS_COMPACT_BYTES" in NATIVE
assert "floorRgb565" in (ROOT / "ESP32/src/esp_native_first_frame.c").read_text()
assert "EspMapRuntime" in (ROOT / "ESP32/src/esp_native_plane_renderer.c").read_text()

for root in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for file in root.rglob("*"):
        if file.suffix not in (".c", ".cpp", ".h"):
            continue
        src = file.read_text(encoding="latin-1")
        src = re.sub(r"#if 0\b[\s\S]*?#endif", " ", src)
        src = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", src, flags=re.S)
        assert not re.search(r"\brenderFloorCeilingTextures\b", src), file

cc = shutil.which("cc") or "cc"
with tempfile.TemporaryDirectory(prefix="canvas-plane-abi-") as tmp:
    probe = Path(tmp) / "probe.c"
    probe.write_text(
        '#include <SDL.h>\n#include "DoomRPG.h"\n#include "DoomCanvas.h"\n'
        'int flag(DoomCanvas_t* c) { return c->renderFloorCeilingTextures; }\n')
    flags = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
             "-IESP32/include", "-Isrc"]
    for name, defs, accepted in (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("oracle", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], True),
        ("desktop", [], True),
    ):
        result = subprocess.run(flags + defs + [str(probe)], cwd=ROOT,
                                capture_output=True, text=True)
        assert (result.returncode == 0) == accepted, (name, result.stderr)
        if not accepted:
            assert "renderFloorCeilingTextures" in result.stderr

# Execute the unchanged native constructor except for the removed dead write.
begin = NATIVE.index("static DoomCanvas_t* EspNativeEngineRoot_initCanvas(")
end = NATIVE.index("/* Native Render constructor owned by the process root;", begin)
actual = NATIVE[begin:end]
assert "SDL_memset(doomCanvas, 0, sizeof(DoomCanvas_t));" in actual
assert "doomCanvas->fontColor = 0xffffffff;" in actual
stub = r"""
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine_metrics.h"
typedef struct DoomRPG_s {int id;} DoomRPG_t;
typedef struct Image_s {void* imgBitmap;} Image_t;
typedef struct DoomCanvas_s {
    Image_t imgFont;
    int time, state;
    short startupMap;
    int skipIntro, fontColor;
    DoomRPG_t* doomRpg;
} DoomCanvas_t;
static int fail_alloc, mallocs, zeros;
static void* tracked_malloc(size_t n) { ++mallocs; return fail_alloc ? NULL : malloc(n); }
static void* tracked_memset(void* p, int v, size_t n) { ++zeros; return memset(p,v,n); }
#define SDL_malloc tracked_malloc
#define SDL_memset tracked_memset
#define DOOMRPG_CANVAS_WIDTH 160
#define DOOMRPG_CANVAS_HEIGHT 120
#define printf(...) ((void)0)
"""
main = r"""
int main(void) {
    DoomRPG_t root = {42};
    fail_alloc = 1;
    assert(EspNativeEngineRoot_initCanvas(NULL, &root) == NULL);
    assert(mallocs == 1 && zeros == 0);
    fail_alloc = 0;
    for (int i=0; i<4096; ++i) {
        DoomCanvas_t given;
        memset(&given, 0xa5, sizeof(given));
        mallocs = zeros = 0;
        assert(EspNativeEngineRoot_initCanvas(&given, &root) == &given);
        assert(mallocs == 0 && zeros == 1);
        DoomCanvas_t expected = {0};
        expected.doomRpg = &root;
        expected.fontColor = -1;
        assert(memcmp(&given, &expected, sizeof(expected)) == 0);
        mallocs = zeros = 0;
        DoomCanvas_t* allocated = EspNativeEngineRoot_initCanvas(NULL, &root);
        assert(allocated && mallocs == 1 && zeros == 1);
        assert(memcmp(allocated, &expected, sizeof(expected)) == 0);
        free(allocated);
    }
    puts("PASS native Canvas 4096 allocation/zeroing and OOM cases");
}
"""
with tempfile.TemporaryDirectory(prefix="canvas-plane-ctor-") as tmp:
    test = Path(tmp) / "test.c"
    test.write_text(stub + "\n" + actual + "\n" + main)
    binary = Path(tmp) / "test"
    subprocess.run([cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-IESP32/include", str(test), "-o", str(binary)],
                   cwd=ROOT, check=True)
    subprocess.run([str(binary)], check=True)
print("PASS Canvas native plane gate retired ABI=40B; desktop/oracle retained")
