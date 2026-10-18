#!/usr/bin/env python3
"""Exact-reference and compiled ABI regression for six stateless DoomRPG helpers.

The desktop reference is used only by this host fixture. No original desktop
translation unit is registered with the CYD build. The test does not prove
flash/heap identity, RNG/clock/save behavior or physical CYD execution.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
CC = shlex.split(os.environ.get("CC", "cc"))
SIGNATURES = (
    ("fixed_t\tDoomRPG_FixedMul(fixed_t a, fixed_t b)", 0x383e7af6),
    ("fixed_t\tDoomRPG_FixedDiv(fixed_t a, fixed_t b)", 0x3cc430eb),
    ("short DoomRPG_shortAtNext(byte* data, int* posData)", 0x1af28fb4),
    ("short DoomRPG_shortAt(byte* data, int posData)", 0xe3c972b6),
    ("int DoomRPG_intAtNext(byte* data, int* posData)", 0x1fa2c02d),
    ("int DoomRPG_intAt(byte* data, int posData)", 0xd1998a2d),
)


def extract(source, signature):
    anchor = signature + "\n{"
    assert source.count(anchor) == 1, signature
    start = source.index(anchor)
    depth = 0
    for pos in range(source.index("{", start), len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError("unclosed primitive: " + signature)


def run(args):
    subprocess.run(args, cwd=ROOT, check=True)


def main():
    desktop = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
    native = (ROOT / "ESP32/src/esp_doomrpg_primitives.c").read_text(encoding="latin-1")
    bodies = []
    names = []
    for sig, crc in SIGNATURES:
        original = extract(desktop, sig)
        assert zlib.crc32(original.encode("latin-1")) == crc, "reference drift: " + sig
        assert original == extract(native, sig), "native drift: " + sig
        bodies.append(original)
        names.append(re.search(r"(DoomRPG_\w+)\(", sig).group(1))
    names_csv = ", ".join(names)
    # Rename symbols before including the inherited prototypes. The six
    # originals then compile in isolation, without desktop Game/Canvas/Render.
    reference = "".join("#define %s Reference_%s\n" % (n, n) for n in names)
    reference += '#include <stdint.h>\n#include <SDL.h>\n#include "DoomRPG.h"\n'
    reference += "\n\n".join(bodies) + "\n"
    runner = r'''
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <SDL.h>
#include "DoomRPG.h"
fixed_t Reference_DoomRPG_FixedMul(fixed_t, fixed_t);
fixed_t Reference_DoomRPG_FixedDiv(fixed_t, fixed_t);
short Reference_DoomRPG_shortAtNext(byte*, int*);
short Reference_DoomRPG_shortAt(byte*, int);
int Reference_DoomRPG_intAtNext(byte*, int*);
int Reference_DoomRPG_intAt(byte*, int);
static uint32_t seed = 0x31415926u;
static uint32_t next(void) {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}
int main(void) {
    uint8_t data[16];
    for (int i = 0; i < 60000; ++i) {
        int32_t a = (int32_t)(next() & 0x7fffffffu);
        int32_t b = (int32_t)(next() & 0x7fffffffu);
        if (next() & 1u) a = -a;
        if (next() & 1u) b = -b;
        if (b == 0) b = 1;
        if (DoomRPG_FixedMul(a, b) != Reference_DoomRPG_FixedMul(a, b) ||
            DoomRPG_FixedDiv(a, b) != Reference_DoomRPG_FixedDiv(a, b)) {
            fprintf(stderr, "fixed mismatch iteration=%d a=%ld b=%ld\n",
                    i, (long)a, (long)b);
            return 1;
        }
        for (int j = 0; j < 16; ++j) data[j] = (uint8_t)next();
        int off = (int)(next() % 9u);
        int shortPos = off, refShortPos = off;
        int intPos = off, refIntPos = off;
        if (DoomRPG_shortAt(data, off) != Reference_DoomRPG_shortAt(data, off) ||
            DoomRPG_intAt(data, off) != Reference_DoomRPG_intAt(data, off) ||
            DoomRPG_shortAtNext(data, &shortPos) !=
                Reference_DoomRPG_shortAtNext(data, &refShortPos) ||
            DoomRPG_intAtNext(data, &intPos) !=
                Reference_DoomRPG_intAtNext(data, &refIntPos) ||
            shortPos != off + 2 || refShortPos != off + 2 ||
            intPos != off + 4 || refIntPos != off + 4) {
            fprintf(stderr, "binary mismatch iteration=%d off=%d\n", i, off);
            return 2;
        }
        uint32_t expected = (uint32_t)data[off] |
            ((uint32_t)data[off + 1] << 8) |
            ((uint32_t)data[off + 2] << 16) |
            ((uint32_t)data[off + 3] << 24);
        if ((uint32_t)DoomRPG_intAt(data, off) != expected) {
            fprintf(stderr, "binary endian mismatch iteration=%d\n", i);
            return 3;
        }
    }
    /* Saturation and sign boundary witnesses, no INT_MIN ABS overflow. */
    const int32_t a[] = {0, 1, -1, 65536, -65536, INT32_MAX, -INT32_MAX};
    const int32_t b[] = {1, -1, 65536, -65536, INT32_MAX, -INT32_MAX};
    for (size_t i = 0; i < sizeof(a)/sizeof(*a); ++i)
        for (size_t j = 0; j < sizeof(b)/sizeof(*b); ++j)
            if (DoomRPG_FixedDiv(a[i], b[j]) != Reference_DoomRPG_FixedDiv(a[i], b[j]))
                return 4;
    puts("PURE-PRIMITIVES PASS 60000 seeded math/binary vectors and boundaries");
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix="doomrpg-pure-host-") as tmp:
        ref = Path(tmp) / "reference.c"
        probe = Path(tmp) / "probe.c"
        exe = str(Path(tmp) / "probe")
        ref.write_text(reference, encoding="latin-1")
        probe.write_text(runner)
        # The inherited original has negative left shifts and high-bit
        # intAt shifts; preserve its toolchain semantics, do not 'fix' them
        # unnoticed in a no-behavior-change migration.
        flags = ["-O2", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fwrapv",
                 "-IESP32/include", "-Isrc"]
        flags += shlex.split(os.environ.get("PURE_SANITIZE_FLAGS", ""))
        run(CC + flags + ["ESP32/src/esp_doomrpg_primitives.c",
                          str(ref), str(probe), "-o", exe])
        run([exe])


if __name__ == "__main__":
    main()
