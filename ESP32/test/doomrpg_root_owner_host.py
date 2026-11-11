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
assert "[DOOM ERROR] DoomRPG Error:" in fatal and "SDL_ShowMessageBox" not in fatal and "exit(0)" in fatal
assert '"doomrpg_engine_patched"' not in build[build.index("Desktop original source registration retired;"):]
assert '"+<DoomRPG.c>"' not in build
assert "compiledPatchedC=0" in build
# Fatal callback deliberately stays off the positive-path test harness.
# Capture every teardown side effect in the *real* native production body.
assert teardown.count("SDL_free(")==0
assert teardown.count("EspNativeMenuStorage_free(")==1
assert teardown.count("EspNativeEngineRoot_releaseRender(")==1
assert teardown.count("EspNativeEngineRoot_releaseCanvas(")==1
assert all(x in teardown for x in (
    "EspNativeMenuStorage_free(", "EspNativeEngineRoot_releaseRender(",
    "EspNativeEngineRoot_releaseCanvas(", "SDL_memset(&nativeRootIdentity"))
assert all(not __import__("re").search(r"doomrpg->" + field + r"\b", teardown)
           for field in ("combat", "particleSystem", "entityDef",
                         "sound", "hud", "menu"))

shim=r"""
#pragma once
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int boolean;
#define true 1
typedef struct DoomRPG_s {
    uint8_t nativeProcessIdentity;
} DoomRPG_t;
typedef struct EspNativeRootOwners_s {
    void *doomCanvas,*render,*menuSystem;
} EspNativeRootOwners_t;
extern DoomRPG_t* doomRpg;
#define SDL_memset memset
int Z_FreeMemory(void);
EspNativeRootOwners_t* EspNativeRootOwners_live(void);
DoomRPG_t* root_identity(void);
void EspNativeMenuStorage_free(void*,DoomRPG_t*,boolean);
void EspNativeEngineRoot_releaseRender(void*,boolean);
void EspNativeEngineRoot_releaseCanvas(void*,DoomRPG_t*);
"""
harness=r"""
#include "shim.h"
extern int DoomRPG_freeMemory(void);
extern void DoomRPG_FreeAppData(DoomRPG_t*);
static int trace[8],n,menuCalls,renderCalls,canvasCalls;
int Z_FreeMemory(void) { return 73456; }
void EspNativeMenuStorage_free(void* p,DoomRPG_t* root,boolean freePtr) {
    assert(p && root==doomRpg && freePtr); assert(n<8);
    trace[n++]=2;menuCalls++;free(p);
}
void EspNativeEngineRoot_releaseRender(void* p, boolean freePtr) {
    assert(p && freePtr); assert(n<8);
    trace[n++]=3;renderCalls++;free(p);
}
void EspNativeEngineRoot_releaseCanvas(void* p,DoomRPG_t* root) {
    assert(p && root==doomRpg); assert(n<8);
    trace[n++]=4;canvasCalls++;free(p);
}
int main(void) {
    assert(DoomRPG_freeMemory()==73456);
    DoomRPG_FreeAppData(0); /* no process root active */
    assert(n==0);
    for(int i=0;i<4096;i++) {
        DoomRPG_t* p=root_identity();
        EspNativeRootOwners_t* owners=EspNativeRootOwners_live();
        assert(p && owners);doomRpg=p;
        p->nativeProcessIdentity=0xa5;
        memset(owners,0,sizeof(*owners));
        n=0;menuCalls=renderCalls=canvasCalls=0;
        int full=(i%3)==0, partial=(i%3)==1;
        if(full||partial) {
            owners->menuSystem=malloc(1);
            owners->render=malloc(1);
            owners->doomCanvas=malloc(1);
            assert(owners->menuSystem&&owners->render&&owners->doomCanvas);
        }
        DoomRPG_FreeAppData(p);
        assert(doomRpg==NULL && p->nativeProcessIdentity==0);
        assert(owners->menuSystem==NULL && owners->render==NULL &&
               owners->doomCanvas==NULL);
        assert(menuCalls==(full||partial ? 1 : 0));
        assert(renderCalls==(full||partial ? 1 : 0));
        assert(canvasCalls==(full||partial ? 1 : 0));
        if(full||partial) {
            /* Native MenuStorage, Render and Canvas; no root heap free. */
            int expected[]={2,3,4};
            assert(n==3 && memcmp(trace,expected,sizeof(expected))==0);
        } else assert(n==0);
    }
    puts("PASS Root-G static root identity + native 3-owner teardown 4096 cycles; no root free");
}
"""
with tempfile.TemporaryDirectory(prefix="root-owner-host-") as temp:
    d=Path(temp)
    (d/"shim.h").write_text(shim)
    (d/"native.c").write_text('#include "shim.h"\n'
        + 'DoomRPG_t* doomRpg = NULL;\n'
        + 'static DoomRPG_t nativeRootIdentity;\n'
        + 'static EspNativeRootOwners_t nativeRootOwners;\n'
        + 'EspNativeRootOwners_t* EspNativeRootOwners_live(void) { return &nativeRootOwners; }\n'
        + 'DoomRPG_t* root_identity(void) { return &nativeRootIdentity; }\n'
        + metric+'\n'+teardown+'\n')
    (d/"test.c").write_text(harness)
    exe=d/"run"
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-I",str(d),
                    str(d/"native.c"),str(d/"test.c"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
