#!/usr/bin/env python3
"""Root-J6: exact whole-file inverse parity and 4096 compiled real C teardown cases."""
from pathlib import Path
from sdl_f12_engine_stubs_libc_host import restore_pre_f12
import hashlib
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = (ROOT / "ESP32/src/engine_stubs.c").read_text(encoding="utf-8")
BASE_SHA = "cb5105f498161d2ce4f4679f4edb84435e7cb68a"
OLD_GUARD = "    if (doomRpg == NULL)\n        return;"
NEW_GUARD = ("    /* Only the live process identity may release process-owned shells. */\n"
             "    if (doomrpg == NULL || doomrpg != doomRpg)\n        return;")
OLD_CANVAS = "EspNativeEngineRoot_releaseCanvas(EspNativeRootOwners_live()->doomCanvas, doomRpg);"
NEW_CANVAS = "EspNativeEngineRoot_releaseCanvas(EspNativeRootOwners_live()->doomCanvas, doomrpg);"
assert SRC.count(NEW_GUARD) == 1
assert SRC.count(NEW_CANVAS) == 1
restored = restore_pre_f12(SRC).replace(NEW_GUARD, OLD_GUARD, 1).replace(NEW_CANVAS, OLD_CANVAS, 1)
payload = restored.encode("utf-8")
sha = hashlib.sha1(b"blob " + str(len(payload)).encode() + b"\0" + payload).hexdigest()
assert sha == BASE_SHA, (sha, BASE_SHA)

signature = "void DoomRPG_FreeAppData(DoomRPG_t* doomrpg)"
start = SRC.index(signature)
open_brace = SRC.index("{", start)
depth = 0
end = None
for pos in range(open_brace, len(SRC)):
    if SRC[pos] == "{":
        depth += 1
    elif SRC[pos] == "}":
        depth -= 1
        if depth == 0:
            end = pos + 1
            break
assert end is not None
function = SRC[start:end]
assert "doomrpg == NULL || doomrpg != doomRpg" in function
assert "releaseCanvas(EspNativeRootOwners_live()->doomCanvas, doomrpg)" in function
assert "memset(&nativeRootIdentity, 0, sizeof(nativeRootIdentity))" in function
assert "doomRpg = NULL;" in function

fixture = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Preserve DoomRPG.h legacy boolean values. */
enum { false, true };
#define SDL_memset memset
typedef struct DoomRPG_s { uint8_t nativeProcessIdentity; } DoomRPG_t;
typedef struct MenuSystem_s { int marker; } MenuSystem_t;
typedef struct Render_s { int marker; } Render_t;
typedef struct DoomCanvas_s { int marker; } DoomCanvas_t;
typedef struct EspNativeRootOwners_s {
    MenuSystem_t* menuSystem;
    Render_t* render;
    DoomCanvas_t* doomCanvas;
} EspNativeRootOwners_t;
static DoomRPG_t nativeRootIdentity, foreignIdentity;
static DoomRPG_t* doomRpg;
static EspNativeRootOwners_t nativeRootOwners;
static MenuSystem_t menu;
static Render_t render;
static DoomCanvas_t canvas;
static int menuCalls, renderCalls, canvasCalls;
static char order[4];
static int nOrder;
static DoomRPG_t* observedMenuOwner;
static DoomRPG_t* observedCanvasOwner;
static EspNativeRootOwners_t* EspNativeRootOwners_live(void) { return &nativeRootOwners; }
static void EspNativeMenuStorage_free(MenuSystem_t* m, DoomRPG_t* root, int freePtr) {
    assert(m == &menu && freePtr == true);
    observedMenuOwner = root;
    ++menuCalls; order[nOrder++] = 'M';
}
static void EspNativeEngineRoot_releaseRender(Render_t* r, int freePtr) {
    assert(r == &render && freePtr == true);
    ++renderCalls; order[nOrder++] = 'R';
}
static void EspNativeEngineRoot_releaseCanvas(DoomCanvas_t* c, DoomRPG_t* root) {
    assert(c == &canvas);
    observedCanvasOwner = root;
    ++canvasCalls; order[nOrder++] = 'C';
}
__PRODUCTION_FUNCTION__
int main(void) {
    for (int i=0; i<4096; ++i) {
        int mask=i & 7, mode=(i/8)%5;
        nativeRootIdentity.nativeProcessIdentity=0x5a;
        foreignIdentity.nativeProcessIdentity=0xa5;
        doomRpg = mode==3 ? NULL : &nativeRootIdentity;
        nativeRootOwners.menuSystem = (mask & 1) ? &menu : NULL;
        nativeRootOwners.render = (mask & 2) ? &render : NULL;
        nativeRootOwners.doomCanvas = (mask & 4) ? &canvas : NULL;
        EspNativeRootOwners_t before=nativeRootOwners;
        menuCalls=renderCalls=canvasCalls=nOrder=0;
        memset(order, 0, sizeof(order));
        observedMenuOwner=observedCanvasOwner=NULL;
        DoomRPG_t* arg = mode==1 ? &foreignIdentity : (mode==2 ? NULL : &nativeRootIdentity);
        int allow = (mode==0 || mode==4);
        DoomRPG_FreeAppData(arg);
        if (allow) {
            assert(doomRpg == NULL);
            assert(nativeRootIdentity.nativeProcessIdentity == 0);
            assert(nativeRootOwners.menuSystem == NULL && nativeRootOwners.render == NULL &&
                   nativeRootOwners.doomCanvas == NULL);
            assert(menuCalls == !!(mask & 1) && renderCalls == !!(mask & 2) &&
                   canvasCalls == !!(mask & 4));
            char expected[4]={0}; int k=0;
            if(mask & 1) expected[k++]='M';
            if(mask & 2) expected[k++]='R';
            if(mask & 4) expected[k++]='C';
            assert(nOrder == k && memcmp(order,expected,(size_t)k)==0);
            assert(observedMenuOwner == ((mask & 1) ? &nativeRootIdentity : NULL));
            assert(observedCanvasOwner == ((mask & 4) ? &nativeRootIdentity : NULL));
        } else {
            assert(nativeRootOwners.menuSystem == before.menuSystem &&
                   nativeRootOwners.render == before.render &&
                   nativeRootOwners.doomCanvas == before.doomCanvas);
            assert(doomRpg == (mode==3 ? NULL : &nativeRootIdentity));
            assert(nativeRootIdentity.nativeProcessIdentity == 0x5a);
            assert(menuCalls==0 && renderCalls==0 && canvasCalls==0 && nOrder==0);
        }
        int calls=menuCalls+renderCalls+canvasCalls;
        /* No second release for a completed teardown or a wrong-root call. */
        if (allow) DoomRPG_FreeAppData(&nativeRootIdentity);
        assert(calls == menuCalls+renderCalls+canvasCalls);
    }
    puts("PASS Root-J6 4096 compiled real teardown C cases, identity/foreign/null/");
    puts("     uninitialized guards, menu-render-canvas order, no double free");
    return 0;
}
""".replace("__PRODUCTION_FUNCTION__", function)
with tempfile.TemporaryDirectory(prefix="root-j6-fatal-") as tmp:
    fixture_c = Path(tmp) / "fixture.c"
    exe = Path(tmp) / "fixture"
    fixture_c.write_text(fixture, encoding="utf-8")
    build = subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                            str(fixture_c), "-o", str(exe)],
                           capture_output=True, text=True)
    assert build.returncode == 0, build.stderr
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
    assert "PASS Root-J6 4096 compiled real teardown C cases" in run.stdout
print("PASS Root-J6 exact baseline Git blob and 4096 real C process-teardown cases")
