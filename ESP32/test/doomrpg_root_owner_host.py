#!/usr/bin/env python3
"""Exercise extracted REAL native root release / metric functions with bounded mocks.

This is a mocked fatal/partial-teardown contract, not CYD SD/graphics testing.
CI verifies final ELF ownership and zero compiled desktop C roots separately.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
native=(ROOT/"ESP32/src/engine_stubs.c").read_text()
legacy=(ROOT/"src/DoomRPG.c").read_text(encoding="latin-1")
build=(ROOT/"ESP32/scripts/build_engine.py").read_text()

def extract(src,signature):
    anchor=signature+"\n{"
    assert src.count(anchor)==1, "missing or ambiguous "+signature
    start=src.index(anchor)
    depth=0
    for i in range(src.index("{",start),len(src)):
        depth+=(src[i]=="{")-(src[i]=="}")
        if depth==0: return src[start:i+1]
    raise AssertionError("unterminated "+signature)

teardown=extract(native, "void DoomRPG_FreeAppData(DoomRPG_t* doomrpg)")
fatal=extract(native, "void DoomRPG_Error(const char* fmt, ...)")
metric_start=native.index("int DoomRPG_freeMemory(void) {")
metric_end=native.index("\n}",metric_start)+2
metric=native[metric_start:metric_end]
assert "DoomRPG_t* doomRpg = NULL;" in native
assert "DoomRPG_t* doomRpg = NULL;" in legacy
assert "Z_FreeMemory()" in metric
assert "closeZipFile" not in fatal and "DoomRPG_FreeAppData(doomRpg)" in fatal
assert "SDL_ShowMessageBox" in fatal and "exit(0)" in fatal
assert '"doomrpg_engine_patched"' not in build[build.index("Desktop original source registration retired;"):]
assert '"+<DoomRPG.c>"' not in build
assert "compiledPatchedC=0" in build
# Fatal callback deliberately stays off the positive-path test harness.
# Capture every teardown side effect in the *real* native production body.
assert teardown.count("SDL_free(")==2
assert teardown.count("EspNativeMenuStorage_free(")==1
assert teardown.count("Render_free(")==1
assert teardown.count("EspNativeEngineRoot_releaseCanvas(")==1
assert all(x in teardown for x in (
    "doomrpg->combat = NULL", "doomrpg->particleSystem = NULL",
    "doomrpg->player = NULL", "doomrpg->entityDef = NULL",
    "doomrpg->sound = NULL", "doomrpg->hud = NULL",
    "doomrpg->menu = NULL", "doomrpg->game = NULL"))

shim=r"""
#pragma once
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int boolean;
#define true 1
typedef struct DoomRPG_s {
    void *combat,*particleSystem,*player,*game,*entityDef,*sound,*hud;
    void *menuSystem,*menu,*render,*doomCanvas;
} DoomRPG_t;
extern DoomRPG_t* doomRpg;
void traced_free(void*);
#define SDL_free traced_free
int Z_FreeMemory(void);
void EspNativeMenuStorage_free(void*,DoomRPG_t*,boolean);
void Render_free(void*,boolean);
void EspNativeEngineRoot_releaseCanvas(void*);
"""
harness=r"""
#include "shim.h"
extern int DoomRPG_freeMemory(void);
extern void DoomRPG_FreeAppData(DoomRPG_t*);
static int trace[8],n,sdlfrees, menuCalls,renderCalls,canvasCalls;
void traced_free(void* p) {
    assert(p);assert(n<8);trace[n++]=1;sdlfrees++;free(p);
}
int Z_FreeMemory(void) { return 73456; }
void EspNativeMenuStorage_free(void* p,DoomRPG_t* root,boolean freePtr) {
    assert(p && root==doomRpg && freePtr); assert(n<8);
    trace[n++]=2;menuCalls++;free(p);
}
void Render_free(void* p, boolean freePtr) {
    assert(p && freePtr); assert(n<8);
    trace[n++]=3;renderCalls++;free(p);
}
void EspNativeEngineRoot_releaseCanvas(void* p) {
    assert(p); assert(n<8);
    trace[n++]=4;canvasCalls++;free(p);
}
int main(void) {
    assert(DoomRPG_freeMemory()==73456);
    DoomRPG_FreeAppData(0); /* legacy early-return when root is NULL */
    assert(n==0);
    for(int i=0;i<4096;i++) {
        DoomRPG_t* p=calloc(1,sizeof(*p));
        assert(p);doomRpg=p;n=0;sdlfrees=menuCalls=renderCalls=canvasCalls=0;
        int full=(i%3)==0, core=(i%3)==1;
        if(full||core) {
            p->menuSystem=malloc(1);
            p->menu=malloc(1);p->render=malloc(1);p->doomCanvas=malloc(1);
            assert(p->game==NULL&&p->menuSystem&&p->menu&&p->render&&p->doomCanvas);
        }
        DoomRPG_FreeAppData(p);
        assert(sdlfrees==(full||core ? 2 : 1));
        assert(menuCalls==(full||core ? 1 : 0));
        assert(renderCalls==(full||core ? 1 : 0));
        assert(canvasCalls==(full||core ? 1 : 0));
        if(full||core) {
            /* Native MenuStorage, Menu, Render, Canvas, root; NO Game object. */
            int expected[]={2,1,3,4,1};
            assert(n==5 && memcmp(trace,expected,sizeof(expected))==0);
        } else assert(n==1 && trace[0]==1);
        doomRpg=0;
    }
    puts("PASS native DoomRPG root=process cleanupCalls=4096 Game=NULL partial+full metric=exact");
}
"""
with tempfile.TemporaryDirectory(prefix="root-owner-host-") as temp:
    d=Path(temp)
    (d/"shim.h").write_text(shim)
    (d/"native.c").write_text('#include "shim.h"\n'
         + 'DoomRPG_t* doomRpg = NULL;\n' + metric+'\n'+teardown+'\n')
    (d/"test.c").write_text(harness)
    exe=d/"run"
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-I",str(d),
                    str(d/"native.c"),str(d/"test.c"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
