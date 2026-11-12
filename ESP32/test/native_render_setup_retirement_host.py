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
anchor = "static void " + name + "(Render_t* render, EspNativeRect* windowRect)\n{"
assert owner.count(anchor) == 1
assert owner.count(name + "(") == 2
assert "void Render_setup(" not in bridge
assert re.search(r"#ifndef DOOMRPG_ESP32\s+void Render_setup\(", header)
first = owner.index(anchor)
last = owner.index("\n/* Bounded CYD layout and font initialization", first)
body = owner[first:last]
assert body.count("EspNativeRect") == 1, "SDL-F19 native layout signature"
# Historical source FNV and host fixture use the exact pre-F19 spelling.
# SDL_Rect is already a typedef of EspNativeRect; no C layout or behavior change.
legacy_body = body.replace("EspNativeRect", "SDL_Rect")
assert "render->halfScreenWidth" not in body
assert "render->fracHalfScreenHeight" not in body
# Historical #if 0 printf labels contain the literal "render->screenX"
# text, but they are not accesses. Scan tokens after stripping literals.
live_code = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                   " ", body, flags=re.S)
assert not re.search(r"\brender->(?:screenX|screenY)\b", live_code)
assert re.search(r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)\s+int screenX;\s+int screenY;\s+#endif", header)
assert "windowRect->x, windowRect->y," in body
assert "render->ceilingColor" not in body and "render->floorColor" not in body
assert "sizeof(short)" not in body
assert "render->columnScale = malloc(render->screenWidth * sizeof(int));" in body
assert "sizeof(Render_t) == 108U" in bridge
assert re.search(r"#ifndef DOOMRPG_ESP32\s+short\* ceilingColor;\s+short\* floorColor;\s+#endif", header)
for subtree in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for p in sorted(subtree.rglob("*")):
        if p.suffix not in (".c", ".cpp", ".h"):
            continue
        code = re.sub(r"#if 0\b[\s\S]*?#endif", " ", p.read_text(encoding="latin-1"))
        code = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", code, flags=re.S)
        assert not re.search(r"\b(?:render|r|rootRender|renderBase)\s*->\s*(?:ceilingColor|floorColor)\b", code), p
assert "render->fracHalfScreenWidth" not in live_code
normalized = (legacy_body.replace("static void " + name + "(", "void Render_setup(")
              .replace("free(render->columnScale);", "SDL_free(render->columnScale);")
              .replace("malloc(render->screenWidth * sizeof(int));",
                       "SDL_malloc(render->screenWidth * sizeof(int));")
              .replace("/* Desktop errorID is not part of the native renderer. */",
                       "//DoomRPG_setErrorID(render->doomRpg, 2);")).rstrip()
h = 2166136261
for b in normalized.encode("utf-8"):
    h = ((h ^ b) * 16777619) & 0xffffffff
assert h == 0x67b0fc80, f"Render_setup source drift {h:08x}"
for p in sorted((ROOT / "ESP32/src").rglob("*")):
    if p.suffix not in (".c", ".cpp"):
        continue
    clean = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ",
                   p.read_text(encoding="latin-1"), flags=re.S)
    assert not re.search(r"\bRender_setup\s*\(", clean), p

probe = ('#include <SDL.h>\n#include "DoomRPG.h"\n#include "Render.h"\n'
         'int read_color(Render_t* r) { return (r->ceilingColor != 0) + (r->floorColor != 0); }\n')
with tempfile.TemporaryDirectory(prefix="native-plane-color-abi-") as tmp:
    p = Path(tmp) / "probe.c"
    p.write_text(probe, encoding="utf-8")
    cc = [shutil.which("cc") or "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
          "-fsyntax-only", "-IESP32/include", "-Isrc"]
    for profile, defs, valid in (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("oracle", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], False),
        ("desktop", [], True),
    ):
        result = subprocess.run(cc + defs + [str(p)], cwd=ROOT, capture_output=True, text=True)
        assert (result.returncode == 0) == valid, (profile, result.stderr)
        if not valid:
            assert "ceilingColor" in result.stderr or "floorColor" in result.stderr

shim = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FRACBITS 16
typedef bool boolean;
typedef struct SDL_Rect { int x,y,w,h; } SDL_Rect;
typedef struct Render_s {
    int screenWidth,screenHeight;
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
#define malloc tracked_malloc
#define free tracked_free
static char lastRenderLog[160];
static void capture_log(const char* fmt, ...) {
    va_list args;
    va_start(args,fmt);
    (void)vsnprintf(lastRenderLog,sizeof(lastRenderLog),fmt,args);
    va_end(args);
}
#define printf capture_log
"""
main = r"""
int main(void) {
    for (int i=0; i<4096; ++i) {
        Render_t r={0};
        SDL_Rect window={5,20,160,80+(i&1)};
        mallocs=frees=errors=fail_at=0;
        EspNativeEngineLayout_setupRender(&r,&window);
        assert(mallocs==1 && frees==0 && errors==0);
        assert(r.screenWidth==160 && r.screenHeight==80);
        assert(strcmp(lastRenderLog,
            "[RENDERCORE] SETUP owner=esp-native-bridge view=160x80@5,20 arrays=640B\n")==0);
        assert(r.screenHeight/2==40 && r.screenHeight==(window.h & ~1));
        assert(r.columnScale);
        mallocs=frees=0;
        EspNativeEngineLayout_setupRender(&r,&window);
        assert(mallocs==1 && frees==1 && errors==0);
        tracked_free(r.columnScale);
    }
    for(int bad=1;bad<=1;bad++) {
        Render_t r={0};
        SDL_Rect window={0,20,160,80};
        mallocs=frees=errors=0;fail_at=bad;
        EspNativeEngineLayout_setupRender(&r,&window);
        assert(mallocs==1 && errors==1);
        tracked_free(r.columnScale);
    }
    puts("PASS Render setup 4096 normal/even-odd/reentry and 1 OOM case");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="render-setup-retirement-") as d:
    source = Path(d) / "test.c"
    source.write_text(shim + "\n" + legacy_body + "\n" + main, encoding="utf-8")
    exe = str(Path(d) / "run")
    subprocess.run([shutil.which("cc") or "cc", "-std=c11", "-O2", "-Wall",
                    "-Wextra", "-Werror", str(source), "-o", exe], check=True)
    subprocess.run([exe], check=True)
print("PASS Render_setup=retired owner=layout FNV=67b0fc80 native-screen-origin=SDL_Rect owner; 4096 cycles, 1 OOM, arrays=640B")
