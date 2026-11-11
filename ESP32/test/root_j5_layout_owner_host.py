#!/usr/bin/env python3
"""Root-J5: exact Git inverse parity + extracted real layout C under two owners."""
from pathlib import Path
import hashlib
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BASE = {
    "ESP32/src/engine_stubs.c": "44b9f32c303b9beb5b1d48f0b3bc202ab34801c7",
    "ESP32/include/engine_metrics.h": "98bf1ccd7577a38a1b0f6202fe297e04d7b26ccf",
    "ESP32/src/main.cpp": "1f510e2219aeefd1c38ac7be34627a3c7edd04a4",
}
files = {path: (ROOT / path).read_text(encoding="utf-8") for path in BASE}
source, header, main = [files[path] for path in BASE]
sig = "int DoomRPG_startEngineLayout(DoomRPG_t* root, DoomRpgLayoutReport* report) {"
assert source.count(sig) == 1
assert "struct DoomRPG_s;\nint DoomRPG_startEngineLayout(struct DoomRPG_s* root, DoomRpgLayoutReport* report);" in header
assert main.count("DoomRPG_startEngineLayout(doomRpg, &layoutReport) != 0;") == 1
undo = {
    "ESP32/src/engine_stubs.c": [
        (sig, "int DoomRPG_startEngineLayout(DoomRpgLayoutReport* report) {"),
        ("if (!coreInitReport.ready || root == NULL || EspNativeRootOwners_live()->doomCanvas == NULL ||",
         "if (!coreInitReport.ready || doomRpg == NULL || EspNativeRootOwners_live()->doomCanvas == NULL ||"),
        ("EspNativeEngineLayout_startupCanvas(canvas, root);",
         "EspNativeEngineLayout_startupCanvas(canvas, doomRpg);"),
    ],
    "ESP32/include/engine_metrics.h": [
        ("struct DoomRPG_s;\nint DoomRPG_startEngineLayout(struct DoomRPG_s* root, DoomRpgLayoutReport* report);",
         "int DoomRPG_startEngineLayout(DoomRpgLayoutReport* report);"),
    ],
    "ESP32/src/main.cpp": [
        ("DoomRPG_startEngineLayout(doomRpg, &layoutReport) != 0;",
         "DoomRPG_startEngineLayout(&layoutReport) != 0;"),
    ],
}
for path, replacements in undo.items():
    restored = files[path]
    for newer, older in replacements:
        assert restored.count(newer) == 1, (path, newer)
        restored = restored.replace(newer, older, 1)
    binary = restored.encode("utf-8")
    actual = hashlib.sha1(b"blob " + str(len(binary)).encode() + b"\0" + binary).hexdigest()
    assert actual == BASE[path], (path, actual, BASE[path])

start = source.index(sig)
brace = source.index("{", start)
end = None
depth = 0
for i in range(brace, len(source)):
    if source[i] == "{":
        depth += 1
    elif source[i] == "}":
        depth -= 1
        if depth == 0:
            end = i+1
            break
assert end is not None
prod = source[start:end]
assert "doomRpg" not in prod
assert "layoutAttempted = true;" in prod
assert "EspNativeEngineLayout_startupCanvas(canvas, root);" in prod
assert "render->columnScale == NULL" in prod
assert "layoutReport.ready = 1;" in prod

fixture = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Mirror legacy DoomRPG.h boolean, not ESP-IDF stdbool macros. */
enum { false, true };
#define SDL_memset memset
#define DOOMRPG_CANVAS_X 0
#define DOOMRPG_CANVAS_Y 0
#define DOOMRPG_CANVAS_WIDTH 160
#define DOOMRPG_CANVAS_HEIGHT 120
#define DOOMRPG_VIEWPORT_X 0
#define DOOMRPG_VIEWPORT_Y 20
#define DOOMRPG_VIEWPORT_WIDTH 160
#define DOOMRPG_VIEWPORT_HEIGHT 80
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
typedef struct DoomRPG_s { uint8_t token; } DoomRPG_t;
typedef struct DoomCanvas_s { int marker; } DoomCanvas_t;
typedef struct Render_s { int screenWidth, screenHeight; int* columnScale; } Render_t;
typedef struct EspNativeRootOwners_s { DoomCanvas_t* doomCanvas; Render_t* render; } EspNativeRootOwners_t;
typedef struct DoomRpgLayoutReport_s {
    uint32_t heap8Before, heap8After, largest8Before, largest8After;
    uint32_t bytesUsed, renderArrayPayloadBytes;
    int16_t clipX, clipY; uint16_t clipWidth, clipHeight;
    int16_t displayX, displayY; uint16_t displayWidth, displayHeight;
    int16_t screenX, screenY; uint16_t screenWidth, screenHeight;
    uint16_t renderWidth, renderHeight, statusTopBarHeight, statusBarHeight;
    uint8_t ready;
} DoomRpgLayoutReport;
static struct { int ready; } coreInitReport;
static int layoutAttempted;
static DoomRpgLayoutReport layoutReport;
static DoomCanvas_t canvas;
static Render_t render;
static int col[160];
static EspNativeRootOwners_t owners;
static DoomRPG_t a={0x12}, b={0x21};
static int initCalls, allowColumn, badGeometry;
static DoomRPG_t* passedRoot;
static DoomCanvas_t* passedCanvas;
static EspNativeRootOwners_t* EspNativeRootOwners_live(void) { return &owners; }
static uint32_t coreFreeHeap(void) { return 145232U - (initCalls ? 5948U : 0U); }
static uint32_t coreLargestBlock(void) { return 86004U; }
static void EspNativeEngineLayout_startupCanvas(DoomCanvas_t* c, DoomRPG_t* root) {
    ++initCalls;
    passedCanvas = c;
    passedRoot = root;
    assert(c != NULL && root != NULL && owners.render != NULL);
    render.screenWidth = badGeometry ? 159 : 160;
    render.screenHeight = 80;
    render.columnScale = allowColumn ? col : NULL;
}
#define printf(...) ((void)0)
__PRODUCTION_FUNCTION__
#undef printf
int main(void) {
    for (int i=0; i<4096; ++i) {
        int mode = i % 10;
        DoomRPG_t* token = (i & 1) ? &a : &b;
        layoutAttempted = 0;
        memset(&layoutReport, 0x5a, sizeof(layoutReport));
        memset(&render, 0, sizeof(render));
        memset(&canvas, 0, sizeof(canvas));
        owners.doomCanvas = &canvas;
        owners.render = &render;
        coreInitReport.ready = 1;
        initCalls = 0; passedRoot = NULL; passedCanvas = NULL;
        allowColumn = 1; badGeometry = 0;
        switch (mode) {
            case 2: coreInitReport.ready=0; break;
            case 3: token=NULL; break;
            case 4: owners.doomCanvas=NULL; break;
            case 5: owners.render=NULL; break;
            case 6: allowColumn=0; break;
            case 7: badGeometry=1; break;
        }
        int success = (mode==0 || mode==1 || mode==8 || mode==9);
        int reachedCanvas = (mode != 2 && mode != 3 && mode != 4 && mode != 5);
        DoomRpgLayoutReport output;
        memset(&output, 0xf1, sizeof(output));
        assert(DoomRPG_startEngineLayout(token, &output) == success);
        assert(layoutAttempted == 1 && output.ready == success);
        assert(initCalls == reachedCanvas);
        assert(passedRoot == (reachedCanvas ? token : NULL));
        assert(passedCanvas == (reachedCanvas ? &canvas : NULL));
        if (success) {
            assert(output.clipWidth==160 && output.clipHeight==120);
            assert(output.displayWidth==160 && output.displayHeight==120);
            assert(output.screenWidth==160 && output.screenHeight==80);
            assert(output.renderWidth==160 && output.renderHeight==80);
            assert(output.renderArrayPayloadBytes==160U*sizeof(int));
            assert(output.bytesUsed==5948);
        }
        int before = initCalls;
        DoomRPG_t* other = token == &a ? &b : &a;
        assert(DoomRPG_startEngineLayout(other, NULL) == success);
        memset(&output, 0xee, sizeof(output));
        assert(DoomRPG_startEngineLayout(NULL, &output) == success);
        assert(output.ready == success && initCalls == before);
    }
    puts("PASS Root-J5 4096 real layout-startup C cycles, two roots,");
    puts("     no-root/core/canvas/render, bad column/geometry and cached once");
    return 0;
}
""".replace("__PRODUCTION_FUNCTION__", prod)
with tempfile.TemporaryDirectory(prefix="root-j5-layout-") as tmp:
    path=Path(tmp)/"fixture.c"
    exe=Path(tmp)/"fixture"
    path.write_text(fixture, encoding="utf-8")
    build=subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                          str(path),"-o",str(exe)],capture_output=True,text=True)
    assert build.returncode == 0, build.stderr
    run=subprocess.run([str(exe)],capture_output=True,text=True)
    assert run.returncode == 0, run.stdout + run.stderr
    assert "PASS Root-J5 4096 real layout-startup C cycles" in run.stdout
print("PASS Root-J5 exact Git parity 3 blobs and 4096 production C cases")
