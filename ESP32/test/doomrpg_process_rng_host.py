#!/usr/bin/env python3
"""Differential real-source DoomRPG RNG table/refill and native replay smoke test.

Compiles exact original bodies and the permanent native RNG owner into separate
translation units using SDL tick mocks. Tests public outputs, hidden sequence
via future refills, plus the ACTUAL native replay guard's byte/word reservation,
lease replay, and attack-probe rollback. Not proof of physical timing/entropy.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
METHODS = (
    ("void Random_SetSeed(unsigned int seed)", 0x73c03c40),
    ("unsigned int Random_Get(void)", 0x0fd52c14),
    ("void  Random_getRand(byte* buff, int i)", 0xf75045df),
    ("void DoomRPG_setRand(Random_t* rand)", 0x3c57334d),
)
GLOBALS = ("static unsigned int _seed = 0;", "static int resetRand = 0;")

def extract(s, signature):
    anchor = signature + "\n{"
    assert s.count(anchor) == 1, "missing/duplicate method " + signature
    start = s.index(anchor)
    depth = 0
    for i in range(s.index("{", start), len(s)):
        if s[i] == "{":
            depth += 1
        elif s[i] == "}":
            depth -= 1
            if depth == 0:
                return s[start:i + 1]
    raise AssertionError("unterminated " + signature)

src = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
native = (ROOT / "ESP32/src/esp_doomrpg_process_rng.c").read_text(encoding="latin-1")
for glob in GLOBALS:
    assert src.count(glob) == native.count(glob) == 1
    assert glob in build
bodies = []
for signature, checksum in METHODS:
    original = extract(src, signature)
    assert zlib.crc32(original.encode("latin-1")) == checksum, "legacy drift"
    assert extract(native, signature) == original, "native RNG drift"
    bodies.append(original)

# Do not alter expression or local/global state types, only namespace symbols
# in a SECOND (reference) TU so it can execute beside the actual native owner.
reference = "\n".join(GLOBALS) + "\n\n" + "\n\n".join(bodies)
for name in ("Random_SetSeed", "Random_Get", "Random_getRand", "DoomRPG_setRand"):
    reference = re.sub(r"\b" + name + r"\b", "Legacy_" + name, reference)

HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "DoomRPG.h"
#include "esp_native_rng_replay_guard.h"

static Uint32 fake_tick;
Uint32 SDL_GetTicks(void) { return fake_tick; }
unsigned int DoomRPG_GetTimeMS(void) { return fake_tick; }
unsigned int DoomRPG_GetUpTimeMS(void) { return fake_tick; }

void Legacy_Random_SetSeed(unsigned int);
unsigned int Legacy_Random_Get(void);
void Legacy_Random_getRand(byte*, int);
void Legacy_DoomRPG_setRand(Random_t*);
byte __wrap_DoomRPG_randNextByte(Random_t*);
int __wrap_DoomRPG_randNextInt(Random_t*);

static uint32_t prng = 0x41ba3c37U;
static uint32_t next(void) {
    prng = 1664525U * prng + 1013904223U;
    return prng;
}
static void match(const Random_t* actual, const Random_t* expected) {
    assert(memcmp(actual, expected, sizeof(*actual)) == 0);
}
static void match_hidden(void) {
    /* The direct next seed draw exposes an entire hidden _seed state step. */
    assert(Random_Get() == Legacy_Random_Get());
}
static void direct_refill(Random_t* actual, Random_t* expected) {
    DoomRPG_setRand(actual);
    Legacy_DoomRPG_setRand(expected);
    match(actual, expected);
}
static void match_refill_after_probe(Random_t* actual, Random_t* expected) {
    direct_refill(actual, expected);
    match_hidden();
}

int main(int argc, char** argv) {
    assert(sizeof(unsigned int) == 4 && sizeof(int) == 4);
    assert(sizeof(Random_t) == 132);
    fake_tick = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 0U;
    Random_t actual = {{0},0}, expected = {{0},0};

    /* This sequence compares 12k native-vs-desktop operations including
       0/1/3/4/127/128-byte requests and explicit SetSeed midstream. */
    for (int i = 0; i < 12000; ++i) {
        const uint32_t v = next();
        fake_tick = (i < 4) ? fake_tick : v;
        switch (i % 6) {
        case 0: direct_refill(&actual, &expected); break;
        case 1: {
            uint8_t a[132], b[132];
            memset(a, 0xa5, sizeof(a));
            memset(b, 0xa5, sizeof(b));
            int count = (int[]){0,1,3,4,127,128}[(v >> 16) % 6];
            Random_getRand(a, count);
            Legacy_Random_getRand(b, count);
            assert(memcmp(a,b,sizeof(a)) == 0);
            break;
        }
        case 2: match_hidden(); break;
        case 3: {
            unsigned int seed = v ^ 0x8cafe501U;
            Random_SetSeed(seed);
            Legacy_Random_SetSeed(seed);
            break;
        }
        case 4: {
            uint8_t a[136], b[136];
            memset(a, 0x5a, sizeof(a));
            memset(b, 0x5a, sizeof(b));
            Random_getRand(a, 128);
            Legacy_Random_getRand(b, 128);
            assert(memcmp(a,b,sizeof(a)) == 0);
            break;
        }
        case 5: direct_refill(&actual, &expected); break;
        }
    }

    /* Exact boundary reserve then rollback; the next LIVE byte must reuse
       the reserved post-refill table, without advancing the hidden source. */
    EspNativeRngReplayGuard_reset();
    direct_refill(&actual, &expected);
    actual.nextRand = expected.nextRand = 127;
    Random_t before = actual, snapshot = actual;
    uint8_t prepared = 0;
    assert(EspNativeRngReplayGuard_beginProbeBoundary(&actual, &snapshot, &prepared));
    assert(prepared == 1 && snapshot.nextRand == 127);
    Legacy_DoomRPG_setRand(&expected);
    match(&actual, &expected);
    assert(EspNativeRngReplayGuard_endProbeBoundary(&actual, &snapshot, prepared));
    match(&actual, &before);
    uint8_t value = __wrap_DoomRPG_randNextByte(&actual);
    assert(value == expected.randTable[0]);
    expected.nextRand = 1;
    match(&actual, &expected);
    match_refill_after_probe(&actual, &expected);

    /* Expiring rollback leases are not confused with permanent attack-probe
       reservations. The attack rolls back Random_t then resolves later. */
    EspNativeRngReplayGuard_reset();
    actual.nextRand = expected.nextRand = 127;
    before = actual;
    assert(EspNativeRngReplayGuard_beginAttackProbe(&actual));
    value = __wrap_DoomRPG_randNextByte(&actual);
    Legacy_DoomRPG_setRand(&expected);
    assert(value == expected.randTable[0]);
    actual = before;
    assert(EspNativeRngReplayGuard_endAttackProbe(&actual, &before));
    fake_tick += 4000U; /* beyond ordinary 1-second lease */
    value = __wrap_DoomRPG_randNextByte(&actual);
    assert(value == expected.randTable[0]);
    expected.nextRand = 1;
    match(&actual, &expected);
    match_refill_after_probe(&actual, &expected);

    /* 4-byte word draw crosses boundary at byte offset 126. The guard must
       advance the exact same refill ONCE, and preserve little-endian offset. */
    EspNativeRngReplayGuard_reset();
    actual.nextRand = expected.nextRand = 126;
    int word = __wrap_DoomRPG_randNextInt(&actual);
    Legacy_DoomRPG_setRand(&expected);
    uint32_t expectedWord = (uint32_t)expected.randTable[0] |
        ((uint32_t)expected.randTable[1] << 8) |
        ((uint32_t)expected.randTable[2] << 16) |
        ((uint32_t)expected.randTable[3] << 24);
    assert((uint32_t)word == expectedWord);
    expected.nextRand = 4;
    match(&actual, &expected);
    match_refill_after_probe(&actual, &expected);

    /* An immediate speculative rollback replays the same table from the
       time-limited lease. This checks the hidden generator did NOT move twice. */
    EspNativeRngReplayGuard_reset();
    actual.nextRand = expected.nextRand = 127;
    before = actual;
    value = __wrap_DoomRPG_randNextByte(&actual);
    Legacy_DoomRPG_setRand(&expected);
    assert(value == expected.randTable[0]);
    actual = before;
    assert(__wrap_DoomRPG_randNextByte(&actual) == expected.randTable[0]);
    expected.nextRand = 1;
    match(&actual, &expected);
    match_refill_after_probe(&actual, &expected);

    EspNativeRngReplayGuard_reset(); /* must NOT reset _seed/resetRand */
    match_refill_after_probe(&actual, &expected);
    printf("PASS RNG source/state/refill/replay 12000 steps tick=%u\n",
           (unsigned)fake_tick);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-rng-") as dir:
    p = Path(dir)
    (p / "SDL.h").write_text("#pragma once\n#include <stdint.h>\ntypedef uint32_t Uint32;\n")
    (p / "DoomRPG.h").write_text(
        "#pragma once\n#include <stdint.h>\ntypedef unsigned char byte;\n"
        "#define RANDTABLESIZE 128\n"
        "typedef struct Random_s { byte randTable[RANDTABLESIZE]; int nextRand; } Random_t;\n"
        "unsigned int DoomRPG_GetTimeMS(void);\n"
        "unsigned int DoomRPG_GetUpTimeMS(void);\n"
        "void Random_SetSeed(unsigned int);\nunsigned int Random_Get(void);\n"
        "void Random_getRand(byte*,int);\nvoid DoomRPG_setRand(Random_t*);\n"
    )
    (p / "reference.c").write_text('#include <SDL.h>\n#include "DoomRPG.h"\n' + reference + "\n")
    (p / "harness.c").write_text(HARNESS)
    exe = p / "rng-test"
    subprocess.run([
        "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I", str(p), "-I", str(ROOT / "ESP32/include"),
        str(p / "harness.c"), str(p / "reference.c"),
        str(ROOT / "ESP32/src/esp_doomrpg_process_rng.c"),
        str(ROOT / "ESP32/src/esp_native_rng_replay_guard.c"),
        "-o", str(exe)
    ], check=True)
    for tick in ("0", "1", "1234567", "4294967295"):
        subprocess.run([str(exe), tick], check=True)
