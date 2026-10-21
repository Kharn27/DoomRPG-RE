#!/usr/bin/env python3
"""Exercise production MENU_MAIN START cleanup and the real native runtime slot body.

The two-lifetime cleanup must remain callable outside the fatal process root.
This mocks the legacy compatibility slots, not CYD pixels or SD data.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
bridge = (ROOT / "ESP32/src/render_startup_bridge.c").read_text(encoding="utf-8")
start = (ROOT / "ESP32/src/native_main_menu_start_action.c").read_text(encoding="utf-8")
root = (ROOT / "ESP32/src/engine_stubs.c").read_text(encoding="utf-8")
header = (ROOT / "ESP32/include/esp_render_startup_bridge.h").read_text(encoding="utf-8")

def extract(source, signature):
    anchor = signature + "\n{"
    assert source.count(anchor) == 1, signature
    begin = source.index(anchor)
    depth = 0
    for offset in range(source.index("{", begin), len(source)):
        depth += (source[offset] == "{") - (source[offset] == "}")
        if depth == 0:
            return source[begin:offset + 1]
    raise AssertionError("Unterminated " + signature)

signature = "void EspNativeRenderRuntime_releaseCompatibility(Render_t* render)"
runtime = extract(bridge, signature)
menu = extract(start, "int DoomRPG_esp32ReleaseMainMenuMemory(struct DoomRPG_s* doomRpgBase)")
assert "EspNativeRenderRuntime_releaseCompatibility(render);" in menu
assert "EspNativeRenderRuntime_releaseCompatibility(render);" in root
assert signature + ";" not in bridge
assert "EspNativeRenderRuntime_releaseCompatibility(struct Render_s* render);" in header
assert "Render_freeRuntime(render);" not in menu
assert "void Render_freeRuntime(Render_t* render)" not in bridge
assert all(token in menu for token in ("render->shapeData == NULL",
                                       "render->mediaTexels == NULL",
                                       "!EspNativeWallCache_isActive()",
                                       "!EspNativeSpriteCache_isActive()"))
body = runtime.replace("EspNativeRenderRuntime_releaseCompatibility(",
                       "Render_freeRuntime(")
fnv = 2166136261
for b in body.encode("utf-8"):
    fnv = ((fnv ^ b) * 16777619) & 0xffffffff
assert fnv == 0x8042d34c, f"Runtime parity drift {fnv:08x}"

shim = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char byte;
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
typedef struct DoomCanvas_s { int alive; } DoomCanvas_t;
typedef struct DoomRPG_s {
    DoomCanvas_t *doomCanvas;
    Render_t *render;
    void *game;
} DoomRPG_t;
static int allocations, runtimeCalls, wallActive, spriteActive;
static uint8_t sharedFB[38400];
static uint8_t sharedPalette[8];
static void* owned(void) {
    void* p=malloc(8);
    assert(p);
    ++allocations;
    return p;
}
static void tracedFree(void* p) {
    if (!p) return;
    assert(p!=(void*)sharedFB && p!=(void*)sharedPalette);
    assert(allocations>0);
    --allocations;
    free(p);
}
#define SDL_free tracedFree
#define printf(...) ((void)0)
static uint32_t heap8Free(void) { return 113768U; }
static uint32_t largest8Block(void) { return 86004U; }
static int EspNativeWallCache_isActive(void) { return wallActive; }
static int EspNativeSpriteCache_isActive(void) { return spriteActive; }
"""
harness = r"""
#define OWN(field) (r->field=owned())
static void populate(Render_t* r, int variant) {
    r->framebuffer=sharedFB;
    r->mediaPalettes=sharedPalette;
    if (variant==0) return;
    OWN(shapeData);
    OWN(mediaTexels);
    OWN(nodes);
    OWN(lines);
    r->linesLength=123;
    if (variant==1) return;
    OWN(mapSprites);
    r->numMapSprites=12;
    OWN(tileEvents);
    OWN(mapByteCode);
    OWN(mediaBitShapeOffsets);
    OWN(mediaTexelOffsets);
    OWN(mediaTexturesIds);
    OWN(mediaSpriteIds);
    OWN(mapTextureTexels);
    OWN(mapSpriteTexels);
    r->mapStringCount=2;
    r->mapStringsIDs=(void**)malloc(sizeof(void*)*2);
    assert(r->mapStringsIDs);++allocations;
    r->mapStringsIDs[0]=owned();
    r->mapStringsIDs[1]=owned();
}
static void testCleanup(void) {
    DoomCanvas_t canvas={1};
    for (int i=0;i<4096;++i) {
        Render_t render={0};
        DoomRPG_t root={&canvas,&render,NULL};
        populate(&render,i%3);
        assert(DoomRPG_esp32ReleaseMainMenuMemory(&root)==1);
        assert(runtimeCalls==i+1);
        assert(allocations==0);
        assert(root.render==&render && root.doomCanvas==&canvas);
        assert(render.framebuffer==sharedFB && render.mediaPalettes==sharedPalette);
        assert(render.nodes==NULL && render.lines==NULL && render.mapSprites==NULL);
        assert(render.mediaTexels==NULL && render.shapeData==NULL);
        assert(render.mediaTexelOffsets==NULL && render.mediaBitShapeOffsets==NULL);
        assert(render.mapStringsIDs==NULL && render.mapStringCount==0);
        assert(render.numMapSprites==0 && render.linesLength==0);
    }
    Render_t render={0};
    DoomRPG_t root={&canvas,&render,NULL};
    int before=runtimeCalls;
    assert(DoomRPG_esp32ReleaseMainMenuMemory(NULL)==0);
    root.render=NULL;
    assert(DoomRPG_esp32ReleaseMainMenuMemory(&root)==0);
    root.render=&render;root.doomCanvas=NULL;
    assert(DoomRPG_esp32ReleaseMainMenuMemory(&root)==0);
    root.doomCanvas=&canvas;root.game=(void*)1;
    assert(DoomRPG_esp32ReleaseMainMenuMemory(&root)==0);
    assert(runtimeCalls==before);
    root.game=NULL;
    wallActive=1;
    assert(DoomRPG_esp32ReleaseMainMenuMemory(&root)==0);
    wallActive=0;
    spriteActive=1;
    assert(DoomRPG_esp32ReleaseMainMenuMemory(&root)==0);
    spriteActive=0;
    assert(runtimeCalls==before+2);
    assert(allocations==0);
}
int main(void) {
    testCleanup();
    puts("PASS MENU_MAIN runtime cleanup 4096 full/partial cycles + fail-closed guards");
    return 0;
}
"""
runtime_instrumented = runtime.replace(
    signature + "\n{", signature + "\n{\n    ++runtimeCalls;", 1
)
assert "++runtimeCalls;" in runtime_instrumented
with tempfile.TemporaryDirectory(prefix="render-runtime-start-") as temp:
    p = Path(temp) / "test.c"
    p.write_text(shim + "\n" + runtime_instrumented +
                 "\n" + menu + "\n" + harness, encoding="utf-8")
    exe = Path(temp) / "run"
    subprocess.run([shutil.which("cc") or "cc", "-std=c11", "-O2",
                    "-Wall", "-Wextra", "-Werror", "-Wno-unused-but-set-variable",
                    str(p), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS Render_freeRuntime=retired MENU_MAIN START native call ABI=explicit")
