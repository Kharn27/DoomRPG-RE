#!/usr/bin/env python3
"""Check the REAL native-owned palette loader against desktop source body parity.

Compiles extracted C with bounded palette.bin bytes and tracked heap allocations.
This verifies private API retirement, palette RGB565 conversion, replacement
frees, and paletteMemory accounting; physical CYD still validates PAK/TFT.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
native = (ROOT / "ESP32/src/render_startup_bridge.c").read_text(encoding="utf-8")
desktop = (ROOT / "src/Render.c").read_text(encoding="latin-1")
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

old_sig = "void Render_loadPalettes(Render_t* render)"
new_sig = "static void EspNativeRenderStartup_loadPalettes(Render_t* render)"
palette = extract(native, new_sig)
desktop_palette = extract(desktop, old_sig)
normalized = palette.replace(new_sig, old_sig, 1)
assert normalized == desktop_palette, "Real palette loader differs from desktop"
h = 2166136261
for b in normalized.encode("utf-8"):
    h = ((h ^ b) * 16777619) & 0xffffffff
assert h == 0x3573ad2c, f"Palette body FNV drift {h:08x}"
assert native.count("EspNativeRenderStartup_loadPalettes(") == 2
assert "void Render_loadPalettes(Render_t* render)" not in native
assert re.search(r"#ifndef DOOMRPG_ESP32\s+void Render_loadPalettes\(", header)
assert "nativePaletteLoader=private" in builder
assert "DoomRPG_Error(\"Render_loadPalettes: Insufficient memory for allocation\")" in palette
rgb565 = extract(native, "unsigned int Render_make565RGB(Render_t* render, int blue, int green, int red)")

for file in sorted((ROOT / "ESP32/src").rglob("*")):
    if file.suffix not in (".c", ".cpp"):
        continue
    sanitized = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                       " ", file.read_text(encoding="latin-1"), flags=re.S)
    assert not re.search(r"\bRender_loadPalettes\s*\(", sanitized), file

shim = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char byte;
typedef struct DoomRPG_s { int marker; } DoomRPG_t;
typedef struct Render_s {
    DoomRPG_t *doomRpg;
    int paletteMemory;
    short* mediaPalettes;
    int mediaPalettesLength;
} Render_t;
typedef struct Block_s { size_t bytes; } Block_t;
static size_t ownedBytes;
static int allocs, frees, reads, errors, headerReads, colorReads;
static void* tracedMalloc(size_t bytes) {
    Block_t* p=(Block_t*)malloc(sizeof(Block_t)+bytes);
    assert(p);
    p->bytes=bytes;
    ownedBytes+=bytes;
    ++allocs;
    return (void*)(p+1);
}
static void tracedFree(void* p) {
    if (!p) return;
    Block_t* b=((Block_t*)p)-1;
    assert(ownedBytes>=b->bytes);
    ownedBytes-=b->bytes;
    ++frees;
    free(b);
}
#define SDL_malloc tracedMalloc
#define SDL_free tracedFree
static int DoomRPG_freeMemory(void) {
    return 100000-(int)ownedBytes;
}
static byte* DoomRPG_fileOpenRead(DoomRPG_t* root, const char* path) {
    assert(root && root->marker==123);
    assert(strcmp(path,"/palettes.bin")==0);
    byte* data=(byte*)SDL_malloc(14);
    static const byte fixture[14]={
        10,0,0,0, 0,0, 31,0, 0xe0,0x07, 0,0xf8, 0xff,0xff
    };
    memcpy(data,fixture,sizeof(fixture));
    ++reads;
    return data;
}
static int DoomRPG_intAtNext(const byte* data, int* pos) {
    ++headerReads;
    int result=(int)data[*pos] | ((int)data[*pos+1]<<8) |
       ((int)data[*pos+2]<<16) | ((int)data[*pos+3]<<24);
    *pos+=4;
    return result;
}
static short DoomRPG_shortAtNext(const byte* data, int* pos) {
    ++colorReads;
    short result=(short)((unsigned)data[*pos] |
                  ((unsigned)data[*pos+1]<<8));
    *pos+=2;
    return result;
}
static void DoomRPG_Error(const char* message) {
    ++errors;
    fprintf(stderr,"%s\n",message);
    abort();
}
"""
# The real Render_make565RGB body intentionally does not use Render_t*.
# Compiler suppresses only that unused parameter in the host harness.
main = r"""
static void verify(const Render_t* render) {
    const unsigned short expected[]={0,0xf800,0x07e0,0x001f,0xffff};
    assert(render->mediaPalettesLength==5);
    for(int i=0;i<5;i++)
        assert((unsigned short)render->mediaPalettes[i]==expected[i]);
}
int main(void) {
    DoomRPG_t root={123};
    Render_t render={0};
    render.doomRpg=&root;
    for(int i=0;i<4096;++i) {
        const int priorFree=DoomRPG_freeMemory();
        EspNativeRenderStartup_loadPalettes(&render);
        verify(&render);
        assert(reads==i+1 && headerReads==i+1 && colorReads==(i+1)*5);
        assert(allocs==(i+1)*2 && frees==(i+1)*2-1);
        assert(ownedBytes==5*sizeof(short));
        assert(render.paletteMemory==DoomRPG_freeMemory()-priorFree);
        assert(errors==0);
    }
    SDL_free(render.mediaPalettes);
    assert(ownedBytes==0);
    assert(allocs==frees);
    puts("PASS private Render palette loader 4096 corpus/replace cycles, RGB565 and heap delta");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="native-palettes-") as temp:
    file = Path(temp) / "test.c"
    file.write_text(shim + "\n" + rgb565 + "\n" + palette + "\n" +
                    main, encoding="utf-8")
    exe = Path(temp) / "run"
    subprocess.run([shutil.which("cc") or "cc", "-std=c11", "-O2",
                    "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                    str(file), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS Render_loadPalettes=retired nativePaletteLoader=private FNV=3573ad2c")
