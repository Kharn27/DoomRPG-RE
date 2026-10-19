#!/usr/bin/env python3
"""Test the real native Canvas destructor with bounded mocks.

The only previous CYD DoomCanvas_free caller used freePtr=true. This checks
the order of all four cleanup actions, with NULL/full cases and repeat cycles.
Host behavior does not substitute for a real CYD boot and LOAD test.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
owner = (SRC / "engine_stubs.c").read_text(encoding="utf-8")
glyph = (SRC / "esp_native_canvas_text.c").read_text(encoding="utf-8")
header = (ROOT / "src/DoomCanvas.h").read_text(encoding="utf-8")
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text(encoding="utf-8")

name = "EspNativeEngineRoot_releaseCanvas"
anchor = "static void " + name + "(DoomCanvas_t* doomCanvas)\n{"
assert owner.count(anchor) == 1
start = owner.index(anchor)
depth = 0
end = None
for i in range(owner.index("{", start), len(owner)):
    depth += (owner[i] == "{") - (owner[i] == "}")
    if depth == 0:
        end = i + 1
        break
assert end is not None
body = owner[start:end]
for required in ("Esp32StoryFit_release(doomCanvas);",
                 "EspNativeIntroState_release(doomCanvas->doomRpg);",
                 "DoomRPG_freeImage(doomCanvas->doomRpg, &doomCanvas->imgFont);",
                 "SDL_free(doomCanvas);"):
    assert required in body
assert "DoomCanvas_free(" not in owner + glyph
assert "doom_canvas_bridge_exports" not in builder
assert not (SRC / "esp_legacy_doomcanvas_bridge.c").exists()
assert re.search(r"#ifndef DOOMRPG_ESP32\s+void DoomCanvas_free\(", header)
assert owner.count(name + "(") == 2  # definition + root caller

shim = r"""
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct DoomRPG_s { unsigned magic; } DoomRPG_t;
typedef struct Image_s { unsigned magic; } Image_t;
typedef struct DoomCanvas_s { Image_t imgFont; DoomRPG_t* doomRpg; } DoomCanvas_t;
static DoomCanvas_t* expectedCanvas;
static DoomRPG_t* expectedRoot;
static int trace[4], n, frees;
void Esp32StoryFit_release(DoomCanvas_t* p) {
    assert(p == expectedCanvas && n == 0); trace[n++] = 1;
}
void EspNativeIntroState_release(DoomRPG_t* p) {
    assert(p == expectedRoot && n == 1); trace[n++] = 2;
}
void DoomRPG_freeImage(DoomRPG_t* p, Image_t* image) {
    assert(p == expectedRoot && image == &expectedCanvas->imgFont &&
           image->magic == 0x12345678 && n == 2);
    trace[n++] = 3;
}
void traced_free(void* p) {
    assert(p == expectedCanvas && n == 3);
    trace[n++] = 4; frees++; free(p);
}
#define SDL_free traced_free
"""
harness = r"""
int main(void) {
    DoomRPG_t root = { 0x87654321 };
    expectedRoot = &root;
    for (int i = 0; i < 4096; ++i) {
        n = 0; frees = 0;
        expectedCanvas = NULL;
        EspNativeEngineRoot_releaseCanvas(NULL);
        assert(n == 0 && frees == 0);
        expectedCanvas = malloc(sizeof(*expectedCanvas));
        assert(expectedCanvas);
        expectedCanvas->doomRpg = expectedRoot;
        expectedCanvas->imgFont.magic = 0x12345678;
        EspNativeEngineRoot_releaseCanvas(expectedCanvas);
        assert(n == 4 && frees == 1);
        const int expected[] = { 1, 2, 3, 4 };
        for (int j = 0; j < 4; ++j) assert(trace[j] == expected[j]);
    }
    puts("PASS native Canvas release 4096 full+NULL cases order=story,intro,font,free");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="canvas-release-host-") as tmp:
    path = Path(tmp)
    unit = path / "canvas_release.c"
    unit.write_text(shim + "\n" + body + "\n" + harness, encoding="utf-8")
    exe = path / "run"
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(unit), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
