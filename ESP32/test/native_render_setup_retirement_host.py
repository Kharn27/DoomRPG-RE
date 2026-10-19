#!/usr/bin/env python3
"""Test the actual native layout Render setup body with mocks and parity FNV."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
owner = (ROOT / "ESP32/src/engine_stubs.c").read_text(encoding="utf-8")
bridge = (ROOT / "ESP32/src/render_startup_bridge.c").read_text(encoding="utf-8")
header = (ROOT / "src/Render.h").read_text(encoding="utf-8")
name = "EspNativeEngineLayout_setupRender"
anchor = "static void " + name + "(Render_t* render, SDL_Rect* windowRect)\n{"
assert owner.count(anchor) == 1
assert owner.count(name + "(") == 2
assert "void Render_setup(" not in bridge
assert re.search(r"#ifndef DOOMRPG_ESP32\s+void Render_setup\(", header)
first = owner.index(anchor)
last = owner.index("\n/* Bounded CYD layout and font initialization", first)
body = owner[first:last]
normalized = body.replace("static void " + name + "(", "void Render_setup(").rstrip()
h = 2166136261
for b in normalized.encode("utf-8"):
    h = ((h ^ b) * 16777619) & 0xffffffff
assert h == 0xec9d1151, f"Render_setup source drift {h:08x}"
for p in sorted((ROOT / "ESP32/src").rglob("*")):
    if p.suffix not in (".c", ".cpp"):
        continue
    clean = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ",
                   p.read_text(encoding="latin-1"), flags=re.S)
    assert not re.search(r"\bRender_setup\s*\(", clean), p

shim = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FRACBITS 16
typedef bool boolean;
typedef struct SDL_Rect { int x,y,w,h; } SDL_Rect;
typedef struct Render_s {
    int screenWidth,screenHeight,screenX,screenY;
    int halfScreenWidth,halfScreenHeight;
    int fracHalfScreenWidth,fracHalfScreenHeight;
    short *ceilingColor,*floorColor;
    int *columnScale;
} Render_t;
static int mallocs,frees,errors,fail_at;
void* tracked_malloc(size_t n) {
    ++mallocs;
    return mallocs == fail_at ? NULL : malloc(n);
}
void tracked_free(void* p) { if(p) ++frees; free(p); }
void DoomRPG_Error(const char* str) {
    assert(strcmp(str,"Render: Insufficient memory for allocation")==0);
    ++errors;
}
#define SDL_malloc tracked_malloc
#define SDL_free tracked_free
#define printf(...) ((void)0)
"""
main = r"""
int main(void) {
    for (int i=0; i<4096; ++i) {
        Render_t r={0};
        SDL_Rect window={5,20,160,80+(i&1)};
        mallocs=frees=errors=fail_at=0;
        EspNativeEngineLayout_setupRender(&r,&window);
        assert(mallocs==3 && frees==0 && errors==0);
        assert(r.screenWidth==160 && r.screenHeight==80);
        assert(r.screenX==5 && r.screenY==20);
        assert(r.halfScreenWidth==80 && r.halfScreenHeight==40);
        assert(r.fracHalfScreenWidth==(80<<16)-0x8000);
        assert(r.fracHalfScreenHeight==(40<<16)-0x8000);
        assert(r.ceilingColor && r.floorColor && r.columnScale);
        mallocs=frees=0;
        EspNativeEngineLayout_setupRender(&r,&window);
        assert(mallocs==3 && frees==3 && errors==0);
        tracked_free(r.ceilingColor);
        tracked_free(r.floorColor);
        tracked_free(r.columnScale);
    }
    for(int bad=1;bad<=3;bad++) {
        Render_t r={0};
        SDL_Rect window={0,20,160,80};
        mallocs=frees=errors=0;fail_at=bad;
        EspNativeEngineLayout_setupRender(&r,&window);
        assert(mallocs==3 && errors==1);
        tracked_free(r.ceilingColor);
        tracked_free(r.floorColor);
        tracked_free(r.columnScale);
    }
    puts("PASS Render setup 4096 normal/even-odd/reentry and 3 OOM cases");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="render-setup-retirement-") as d:
    source = Path(d) / "test.c"
    source.write_text(shim + "\n" + body + "\n" + main, encoding="utf-8")
    exe = str(Path(d) / "run")
    subprocess.run([shutil.which("cc") or "cc", "-std=c11", "-O2", "-Wall",
                    "-Wextra", "-Werror", str(source), "-o", exe], check=True)
    subprocess.run([exe], check=True)
print("PASS Render_setup=retired owner=layout FNV=ec9d1151")
