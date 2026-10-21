#!/usr/bin/env python3
"""Exercise REAL private Render destructor + runtime cleanup with bounded mocks.

The source-body FNV pins the pre-retirement behavior. Test both framebuffer
ownership paths, both freePtr paths and partially populated compatibility
objects. This is a host C test, not a physical CYD fatal-teardown claim.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
owner = (ROOT / "ESP32/src/engine_stubs.c").read_text(encoding="utf-8")
bridge = (ROOT / "ESP32/src/render_startup_bridge.c").read_text(encoding="utf-8")
header = (ROOT / "src/Render.h").read_text(encoding="utf-8")
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text(encoding="utf-8")

def extract(source, signature):
    anchor = signature + "\n{"
    assert source.count(anchor) == 1, signature
    start = source.index(anchor)
    depth = 0
    for i in range(source.index("{", start), len(source)):
        depth += (source[i] == "{") - (source[i] == "}")
        if depth == 0:
            return source[start:i + 1]
    raise AssertionError("Unterminated " + signature)

name = "EspNativeEngineRoot_releaseRender"
body = extract(owner, "static void " + name +
               "(Render_t* render, boolean freePtr)")
runtime = extract(bridge, "void Render_freeRuntime(Render_t* render)")
normalized = body.replace("static void " + name + "(", "void Render_free(")
hash_value = 2166136261
for b in normalized.encode("utf-8"):
    hash_value = ((hash_value ^ b) * 16777619) & 0xffffffff
assert hash_value == 0x7e988863, f"Render_free body drift {hash_value:08x}"
assert owner.count(name + "(") == 2
assert "void Render_free(" not in bridge
assert "nativeRenderTeardown=private" in builder
assert re.search(r"#ifndef DOOMRPG_ESP32\s+void Render_free\(", header)
assert "Render_freeRuntime(render);" in body
assert "EspNativeEngineRoot_releaseRender(doomrpg->render, true);" in owner
# No linked ESP32 C/C++ caller or implementation may resurrect this ABI.
for p in sorted((ROOT / "ESP32/src").rglob("*")):
    if p.suffix not in (".c", ".cpp"):
        continue
    clean = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ",
                   p.read_text(encoding="latin-1"), flags=re.S)
    assert not re.search(r"\bRender_free\s*\(", clean), p

shim = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char byte;
typedef int boolean;
typedef struct Render_s {
    void *mediaTexels, *shapeData, *mapSprites, *nodes, *lines;
    void *tileEvents, *mapByteCode;
    int numMapSprites, linesLength, mapStringCount;
    void **mapStringsIDs;
    void *mediaBitShapeOffsets, *mediaTexelOffsets, *mediaTexturesIds;
    void *mediaSpriteIds, *mapTextureTexels, *mapSpriteTexels;
    void *ceilingColor, *floorColor, *columnScale, *mediaPalettes;
    byte *framebuffer;
    void *piDIB;
} Render_t;
static byte platformFramebuffer[160 * 120 * 2];
static int outstanding, freeCalls, runtimeCalls, textureDestroys;
static void* owned(size_t n) {
    void* p = malloc(n);
    assert(p);
    ++outstanding;
    return p;
}
static void trackedFree(void* p) {
    if (!p) return;
    assert(p != (void*)platformFramebuffer);
    assert(outstanding > 0);
    --outstanding;
    ++freeCalls;
    free(p);
}
static void trackedDestroyTexture(void* p) {
    assert(p);
    ++textureDestroys;
    trackedFree(p);
}
static byte* Esp32PlatformVideo_framebuffer(void) {
    return platformFramebuffer;
}
#define SDL_free trackedFree
#define SDL_DestroyTexture trackedDestroyTexture
#define printf(...) ((void)0)
"""
# A macro keeps every legacy compatibility slot populated on full cycles.
main = r"""
#define ALLOC_SLOT(field) (render->field = owned(8))
static void populate(Render_t* render, int full, int shared) {
    if (full) {
        ALLOC_SLOT(mediaTexels);
        ALLOC_SLOT(shapeData);
        ALLOC_SLOT(mapSprites);
        ALLOC_SLOT(nodes);
        ALLOC_SLOT(lines);
        ALLOC_SLOT(tileEvents);
        ALLOC_SLOT(mapByteCode);
        render->mapStringCount = 2;
        render->mapStringsIDs = (void**)owned(2 * sizeof(void*));
        render->mapStringsIDs[0] = owned(8);
        render->mapStringsIDs[1] = owned(8);
        render->numMapSprites = 12;
        render->linesLength = 34;
        ALLOC_SLOT(mediaBitShapeOffsets);
        ALLOC_SLOT(mediaTexelOffsets);
        ALLOC_SLOT(mediaTexturesIds);
        ALLOC_SLOT(mediaSpriteIds);
        ALLOC_SLOT(mapTextureTexels);
        ALLOC_SLOT(mapSpriteTexels);
        ALLOC_SLOT(ceilingColor);
        ALLOC_SLOT(floorColor);
        ALLOC_SLOT(columnScale);
        ALLOC_SLOT(mediaPalettes);
        render->piDIB = owned(8);
    }
    render->framebuffer = shared ? platformFramebuffer : (byte*)owned(160*120*2);
}
int main(void) {
    for (int i = 0; i < 4096; ++i) {
        Render_t stack = {0};
        const int freePtr = i & 1;
        const int shared = (i >> 1) & 1;
        const int full = (i % 3) != 0;
        Render_t* render = freePtr ? (Render_t*)owned(sizeof(Render_t)) : &stack;
        if (freePtr) memset(render, 0, sizeof(*render));
        const int destroysBefore = textureDestroys;
        populate(render, full, shared);
        EspNativeEngineRoot_releaseRender(render, freePtr);
        assert(runtimeCalls == i + 1);
        assert(outstanding == 0);
        assert(textureDestroys == destroysBefore + full);
        if (!freePtr) {
            assert(render->framebuffer == NULL && render->piDIB == NULL);
            assert(render->mediaTexels == NULL && render->shapeData == NULL);
            assert(render->mapSprites == NULL && render->numMapSprites == 0);
            assert(render->lines == NULL && render->linesLength == 0);
            assert(render->mapStringsIDs == NULL && render->mapStringCount == 0);
            assert(render->mediaTexelOffsets == NULL);
        }
        /* PlatformVideo memory is never touched or released by this cleanup. */
        platformFramebuffer[0] = (byte)i;
        assert(platformFramebuffer[0] == (byte)i);
    }
    assert(freeCalls > 4096 && runtimeCalls == 4096);
    puts("PASS Render destructor: 4096 full/partial, shared/owned FB and freePtr cycles");
    return 0;
}
"""
# Add an observing wrapper to count REAL runtime calls, without modifying it.
runtime_instrumented = runtime.replace(
    "void Render_freeRuntime(Render_t* render)\n{",
    "void Render_freeRuntime(Render_t* render)\n{\n    ++runtimeCalls;",
    1,
)
with tempfile.TemporaryDirectory(prefix="private-render-free-") as temp:
    p = Path(temp) / "test.c"
    p.write_text(shim + "\n" + runtime_instrumented + "\n" +
                 body + "\n" + main, encoding="utf-8")
    exe = Path(temp) / "run"
    subprocess.run([shutil.which("cc") or "cc", "-std=c11", "-O2",
                    "-Wall", "-Wextra", "-Werror", str(p), "-o", str(exe)],
                   check=True)
    subprocess.run([str(exe)], check=True)
print("PASS Render_free=retired private native root bodyFNV=7e988863")
