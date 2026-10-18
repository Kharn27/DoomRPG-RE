#!/usr/bin/env python3
"""Check actual classic-CYD clock provider against the pinned desktop bodies.

Only SDL_GetTicks is mocked. The two implementations run side by side with
independent basetime state, including boot tick=0, uptime order and wraparound.
This does not time the physical CYD or establish SDL's underlying clock source.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
SIGNATURES = (
    ("unsigned int DoomRPG_GetTimeMS(void)", 0xC3048DB2),
    ("unsigned int DoomRPG_GetUpTimeMS(void)", 0x0FAD5924),
)
BASE = "static Uint32 basetime = 0;"


def extract(source, signature):
    anchor = signature + "\n{"
    assert source.count(anchor) == 1, "clock signature drift: " + signature
    start = source.index(anchor)
    depth = 0
    for i in range(source.index("{", start), len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise AssertionError("unterminated clock " + signature)


legacy = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
native = (ROOT / "ESP32/src/esp_doomrpg_process_clock.c").read_text(encoding="latin-1")
assert legacy.count(BASE) == native.count(BASE) == 1
bodies = []
for sig, crc in SIGNATURES:
    expected = extract(legacy, sig)
    assert zlib.crc32(expected.encode("latin-1")) == crc, "reference drift " + sig
    assert extract(native, sig) == expected, "effective clock behavior changed " + sig
    bodies.append(expected.replace("DoomRPG_Get", "Legacy_Get"))

HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL.h>
#include "DoomRPG.h"
_Static_assert(sizeof(Uint32) == 4, "SDL ticks must be exactly 32-bit");
static Uint32 fake_ticks = 0;
Uint32 SDL_GetTicks(void) { return fake_ticks; }
extern unsigned int Legacy_GetTimeMS(void);
extern unsigned int Legacy_GetUpTimeMS(void);

static unsigned int calls = 0;
static void check_time(Uint32 tick) {
    fake_ticks = tick;
    assert(DoomRPG_GetTimeMS() == Legacy_GetTimeMS());
    calls++;
}
static void check_uptime(Uint32 tick) {
    fake_ticks = tick;
    assert(DoomRPG_GetUpTimeMS() == Legacy_GetUpTimeMS());
    calls++;
}
int main(void) {
    /* The historical zero sentinel permits a new epoch at tick 1. */
    check_time(0);
    check_uptime(0);
    check_time(1);
    check_uptime(2);
    check_uptime(UINT32_MAX);
    check_time(0);
    check_uptime(1);
    /* Mixed consumers, repeated clock values, a 32-bit wrap and random deltas. */
    Uint32 tick = UINT32_MAX - 64u;
    uint32_t seed = 0xC0FFEE12u;
    for (int i = 0; i < 10000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        tick += (seed >> 24) & 0xFFu;
        if (i & 1) check_time(tick); else check_uptime(tick);
        if ((i % 7) == 0) check_uptime(tick);
        if ((i % 11) == 0) check_time(tick);
    }
    puts("PASS DoomRPG process clock parity: 32-bit tick, zero sentinel, wrap, mixed-order 10000 steps");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-clock-") as tmp:
    out = Path(tmp)
    (out / "SDL.h").write_text(
        "#pragma once\n#include <stdint.h>\ntypedef uint32_t Uint32;\n"
        "Uint32 SDL_GetTicks(void);\n"
    )
    (out / "DoomRPG.h").write_text(
        "#pragma once\nunsigned int DoomRPG_GetTimeMS(void);\n"
        "unsigned int DoomRPG_GetUpTimeMS(void);\n"
    )
    (out / "original_clock.c").write_text(
        '#include <SDL.h>\n' + BASE + "\n\n" + "\n\n".join(bodies) + "\n"
    )
    (out / "harness.c").write_text(HARNESS)
    exe = out / "clock_parity"
    subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(out),
         str(out / "harness.c"), str(out / "original_clock.c"),
         str(ROOT / "ESP32/src/esp_doomrpg_process_clock.c"),
         "-o", str(exe)],
        check=True,
    )
    subprocess.run([str(exe)], check=True)
