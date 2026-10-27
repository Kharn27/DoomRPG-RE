#!/usr/bin/env python3
"""Validate the real CYD Render startup body after private-ABI retirement.

Compile extracted production C with source-body FNV lock and a bounded mock
for the *existing* shared framebuffer and palette loader. This does not
simulate physical TFT output or replace hardware validation.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
path = ROOT / "ESP32/src/render_startup_bridge.c"
source = path.read_text(encoding="utf-8")
header = (ROOT / "src/Render.h").read_text(encoding="utf-8")
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text(encoding="utf-8")
name = "EspNativeRenderStartup_initialize"
start = "static int " + name + "(Render_t* render) {"
end = "\nint EspRenderStartupBridge_start("
assert source.count(start) == 1
assert source.count(name + "(") == 2  # private body + single bridge call
assert "result = " + name + "(render);" in source
assert "int Render_startup(Render_t* render) {" not in source
assert "nativeRenderStartup=private" in builder
assert re.search(r"#ifndef DOOMRPG_ESP32\s+int Render_startup\(", header)
body = source[source.index(start):source.index(end, source.index(start))].rstrip()
normalized = (body.replace("static int " + name + "(", "int Render_startup(")
              .replace("EspNativeRenderStartup_loadPalettes(render);",
                       "Render_loadPalettes(render);"))
h = 2166136261
for byte in normalized.encode("utf-8"):
    h = ((h ^ byte) * 16777619) & 0xffffffff
assert h == 0x0ca067c4, f"Render_startup implementation drift: {h:08x}"

# Reject any ESP32 C/C++ call/definition with the retired name, ignoring
# comments and diagnostic strings (which intentionally retain old labels).
for file in sorted((ROOT / "ESP32/src").rglob("*")):
    if file.suffix not in (".c", ".cpp"):
        continue
    sanitized = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ",
                       file.read_text(encoding="latin-1"), flags=re.S)
    assert not re.search(r"\b(?:Render_startup|Render_loadPalettes)\s*\(", sanitized), file

shim = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char byte;
typedef struct SDL_Rect { int x,y,w,h; } SDL_Rect;
typedef struct DoomRPG_s { void* doomCanvas; } DoomRPG_t;
typedef struct Render_s {
    DoomRPG_t* doomRpg;
    uint32_t sinTable[256];
    SDL_Rect clipRect;
    int pitch;
    byte* framebuffer;
    short* mediaPalettes;
    int mediaPalettesLength;
} Render_t;
static struct { int rendererW,rendererH; } sdlVideo={160,120};
static byte framebuffer[160*120*2];
static int fbReady, failOpen, failPalettes;
static size_t fbBytes;
static int fileReads,fileFrees,paletteLoads;
#define DOOMRPG_CANVAS_X 0
#define DOOMRPG_CANVAS_Y 0
#define DOOMRPG_CANVAS_WIDTH 160
#define DOOMRPG_CANVAS_HEIGHT 120
#define SDL_memmove memmove
#define SDL_free tracedFree
#define SDL_SwapLE32(x) (x)
#define printf(...) ((void)0)
static void tracedFree(void* p) {
    if(p) ++fileFrees;
    free(p);
}
static byte* DoomRPG_fileOpenRead(DoomRPG_t* root, const char* name) {
    assert(root != NULL);
    assert(strcmp(name,"/sintable.bin")==0);
    ++fileReads;
    if (failOpen) return NULL;
    byte* result=(byte*)malloc(1024);
    assert(result);
    for (int i=0;i<256;++i) {
        result[4*i]=i & 255;
        result[4*i+1]=0;
        result[4*i+2]=0;
        result[4*i+3]=0;
    }
    return result;
}
static void* Esp32PlatformVideo_framebuffer(void) {
    return fbReady ? framebuffer : NULL;
}
static size_t Esp32PlatformVideo_framebufferSizeBytes(void) { return fbBytes; }
static void EspNativeRenderStartup_loadPalettes(Render_t* render) {
    static short palette[3280];
    ++paletteLoads;
    render->mediaPalettes = failPalettes ? NULL : palette;
    render->mediaPalettesLength = failPalettes ? 0 : 3280;
}
"""
main = r"""
static void resetProbes(void) {
    failOpen=failPalettes=0;
    fbReady=1;
    fbBytes=sizeof(framebuffer);
    fileReads=fileFrees=paletteLoads=0;
    memset(framebuffer,0,sizeof(framebuffer));
}
int main(void) {
    DoomRPG_t root={.doomCanvas=(void*)1};
    Render_t render={0};
    render.doomRpg=&root;
    for (int n=0;n<4096;++n) {
        resetProbes();
        render.framebuffer=NULL;
        assert(EspNativeRenderStartup_initialize(&render)==1);
        assert(render.pitch==320);
        assert(render.framebuffer==framebuffer);
        assert(render.clipRect.x==0 && render.clipRect.y==0);
        assert(render.clipRect.w==160 && render.clipRect.h==120);
        assert(framebuffer[0]==255 && framebuffer[sizeof(framebuffer)-1]==255);
        assert(render.sinTable[0]==0 && render.sinTable[255]==255);
        assert(render.mediaPalettesLength==3280 && render.mediaPalettes);
        assert(fileReads==1 && fileFrees==1 && paletteLoads==1);
    }
    resetProbes();
    assert(EspNativeRenderStartup_initialize(NULL)==0);
    assert(fileReads==0 && paletteLoads==0);

    root.doomCanvas=NULL;
    assert(EspNativeRenderStartup_initialize(&render)==0);
    assert(fileReads==0 && paletteLoads==0);
    root.doomCanvas=(void*)1;

    resetProbes();failOpen=1;
    assert(EspNativeRenderStartup_initialize(&render)==0);
    assert(fileReads==1 && fileFrees==0 && paletteLoads==0);

    resetProbes();fbReady=0;
    assert(EspNativeRenderStartup_initialize(&render)==0);
    assert(fileReads==1 && fileFrees==1 && paletteLoads==0);

    resetProbes();fbBytes=sizeof(framebuffer)-1;
    assert(EspNativeRenderStartup_initialize(&render)==0);
    assert(fileReads==1 && fileFrees==1 && paletteLoads==0);

    resetProbes();failPalettes=1;
    assert(EspNativeRenderStartup_initialize(&render)==0);
    assert(fileReads==1 && fileFrees==1 && paletteLoads==1);

    puts("PASS Render startup 4096 shared-FB/palette cases + 5 fail-closed branches");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="private-render-startup-") as d:
    src = Path(d) / "render_startup_test.c"
    src.write_text(shim + "\n" + body + "\n" + main, encoding="utf-8")
    exe = str(Path(d) / "run")
    subprocess.run([shutil.which("cc") or "cc", "-O2", "-std=c11",
                    "-Wall", "-Wextra", "-Werror", str(src), "-o", exe],
                   check=True)
    subprocess.run([exe], check=True)
print("PASS Render_startup retired; shared framebuffer only, native init FNV=0ca067c4")
