#!/usr/bin/env python3
"""Execute the native process-root Render constructor using bounded mocks."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
owner = (ROOT / "ESP32/src/engine_stubs.c").read_text(encoding="utf-8")
bridge = (ROOT / "ESP32/src/render_startup_bridge.c").read_text(encoding="utf-8")
header = (ROOT / "src/Render.h").read_text(encoding="utf-8")
name = "EspNativeEngineRoot_initRender"
anchor = "static Render_t* " + name + "(Render_t* render, DoomRPG_t* doomRpg)\n{"
assert owner.count(anchor) == 1
assert owner.count(name + "(") == 2
assert "Render_init(" not in bridge
assert re.search(r"#ifndef DOOMRPG_ESP32\s+Render_t\* Render_init\(", header)
start = owner.index(anchor)
end = owner.index("\nint DoomRPG_initEngineCore(", start)
body = owner[start:end]
assert "SDL_memset(render, 0, sizeof(Render_t));" in body
normalized = body.replace("static Render_t* " + name + "(", "Render_t* Render_init(")
h = 2166136261
for byte in normalized.rstrip().encode("utf-8"):
    h = ((h ^ byte) * 16777619) & 0xffffffff
assert h == 0x09ca8937, f"Render ctor body drift {h:08x}"
for source in sorted((ROOT / "ESP32/src").rglob("*")):
    if source.suffix not in (".c", ".cpp"):
        continue
    clean = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ",
                   source.read_text(encoding="latin-1"), flags=re.S)
    assert not re.search(r"\bRender_init\s*\(", clean), source

shim = r"""
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct DoomRPG_s { unsigned id; } DoomRPG_t;
typedef struct Render_s {
    DoomRPG_t *doomRpg;
    int skipCull,ioBufferPos,mapCameraSpawnIndex;
    int ceilingTex,floorTex,animFrameTime,mapStringCount;
    void *lines,*nodes,*mapSprites,*floorColor,*ceilingColor,*columnScale,*mapStringsIDs;
    unsigned char untouched[13];
} Render_t;
static int fail_alloc,allocs,zeros;
void* tracked_malloc(size_t n) { ++allocs; return fail_alloc ? NULL : malloc(n); }
void* tracked_memset(void* p,int b,size_t n) { ++zeros; return memset(p,b,n); }
#define SDL_malloc tracked_malloc
#define SDL_memset tracked_memset
"""
harness = r"""
int main(void) {
    DoomRPG_t root = {12345};
    fail_alloc=1;allocs=zeros=0;
    assert(EspNativeEngineRoot_initRender(NULL,&root)==NULL);
    assert(allocs==1 && zeros==0);
    fail_alloc=0;
    for(int i=0;i<4096;i++) {
        Render_t provided;memset(&provided,0xa5,sizeof(provided));
        allocs=zeros=0;
        assert(EspNativeEngineRoot_initRender(&provided,&root)==&provided);
        assert(allocs==0 && zeros==1 && provided.doomRpg==&root);
        Render_t zero={0};zero.doomRpg=&root;
        assert(memcmp(&provided,&zero,sizeof(zero))==0);
        allocs=zeros=0;
        Render_t* allocated=EspNativeEngineRoot_initRender(NULL,&root);
        assert(allocated && allocs==1 && zeros==1);
        assert(memcmp(allocated,&zero,sizeof(zero))==0);
        free(allocated);
    }
    puts("PASS Render constructor 4096 cycles allocation/preallocated/OOM");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="native-render-init-") as d:
    p = Path(d) / "test.c"
    p.write_text(shim + "\n" + body + "\n" + harness, encoding="utf-8")
    exe = str(Path(d) / "run")
    subprocess.run([shutil.which("cc") or "cc", "-std=c11", "-O2",
                    "-Wall", "-Wextra", "-Werror", str(p), "-o", exe], check=True)
    subprocess.run([exe], check=True)
print("PASS Render_init=retired nativeRoot=yes bodyFNV=09ca8937 raster-flags=desktop-only")
