#!/usr/bin/env python3
"""Compile exact native/legacy config reader C bodies with a mock SDL RWops.

Tests byte signedness and little-endian 32-bit reads including split read sizes.
No fabricated 'fixed' EOF semantics: original uses uninitialized local on short
or NULL reads. For safety the test only invokes fully readable buffers.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
FUNCTIONS = (
    ("int File_readByte(SDL_RWops* rw)", 0x91bf04f3),
    ("int File_readInt(SDL_RWops* rw)", 0x417442d7),
)

def extract(text, signature):
    a = signature + "\n{"
    assert text.count(a) == 1, "reader signature missing/multiplied"
    start = text.index(a)
    depth = 0
    for i in range(text.index("{", start), len(text)):
        depth += (text[i] == "{") - (text[i] == "}")
        if depth == 0:
            return text[start:i + 1]
    raise AssertionError("missing closing brace")

legacy = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
owner = (ROOT / "ESP32/src/esp_doomrpg_config_read.c").read_text(encoding="latin-1")
script = (ROOT / "ESP32/scripts/build_engine.py").read_text()
reference = []
for signature, crc in FUNCTIONS:
    code = extract(legacy, signature)
    assert zlib.crc32(code.encode("latin-1")) == crc
    assert extract(owner, signature) == code
    assert script.count('("' + signature + '",') == 1
    reference.append(code)
reference = "\n\n".join(reference)
for name in ("File_readByte", "File_readInt"):
    reference = re.sub(r"\b" + name + r"\b", "Legacy_" + name, reference)

test = r"""
#include <SDL.h>
#include <DoomRPG.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

int File_readByte(SDL_RWops*);
int File_readInt(SDL_RWops*);
int Legacy_File_readByte(SDL_RWops*);
int Legacy_File_readInt(SDL_RWops*);
size_t SDL_RWread(SDL_RWops* rw, void* target, size_t size, size_t items) {
    assert(rw != NULL);
    assert(rw->pos + size * items <= rw->size);
    memcpy(target, rw->data + rw->pos, size * items);
    rw->pos += size * items;
    rw->calls++;
    return items;
}
static uint32_t seq = 0x93051203U;
static uint32_t next(void) {
    seq = seq * 1664525U + 1013904223U;
    return seq;
}
int main(void) {
    assert(sizeof(char) == 1 && sizeof(int) == 4);
    uint8_t buffer[8192];
    for (int n=0; n < 8192; ++n) buffer[n] = (uint8_t)next();
    SDL_RWops one={buffer,sizeof(buffer),0,0}, two=one;
    for (int i=0; i < 4096; ++i) {
        int a,b;
        if (i % 3 == 0) {
            a = File_readInt(&one);
            b = Legacy_File_readInt(&two);
        } else {
            a = File_readByte(&one);
            b = Legacy_File_readByte(&two);
        }
        assert(a == b && one.pos == two.pos && one.calls == two.calls);
    }
    assert(one.pos == 8192 - ((4096/3)*0)); /* validated below instead */
    printf("PASS DoomRPG config RWops reader parity mixedCalls=%zu bytes=%zu\n",
           one.calls, one.pos);
}
"""
# Each third operation uses four bytes: 1366 ints (indices 0..4095 divisible
# by 3) plus 2730 byte reads = 8194 bytes; pick 4k available and pad.
test = test.replace("uint8_t buffer[8192];", "uint8_t buffer[12288];")
test = test.replace("n < 8192", "n < 12288")
test = test.replace('    assert(one.pos == 8192 - ((4096/3)*0)); /* validated below instead */',
                    '    assert(one.pos == 8194 && one.calls == 4096);')
with tempfile.TemporaryDirectory(prefix="config-reader-") as d:
    p=Path(d)
    (p/"SDL.h").write_text(
        "#pragma once\n#include <stddef.h>\n#include <stdint.h>\n"
        "typedef struct SDL_RWops { const uint8_t* data; size_t size,pos,calls; } SDL_RWops;\n"
        "size_t SDL_RWread(SDL_RWops*,void*,size_t,size_t);\n"
        "static inline uint32_t SDL_SwapLE32(uint32_t v) { return v; }\n"
    )
    (p/"DoomRPG.h").write_text("#pragma once\ntypedef unsigned char byte;\n")
    (p/"legacy.c").write_text('#include <SDL.h>\n#include <DoomRPG.h>\n'+reference)
    (p/"test.c").write_text(test)
    exe=p/"test"
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-I",str(p),
        str(p/"test.c"),str(p/"legacy.c"),
        str(ROOT/"ESP32/src/esp_doomrpg_config_read.c"),
        "-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
