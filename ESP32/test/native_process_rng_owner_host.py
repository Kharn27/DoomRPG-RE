#!/usr/bin/env python3
"""Root-E: real CYD process-lifetime RNG owner and 132-byte table extraction.

The exact legacy PRNG/refill bodies are tested separately. This checks the
native storage/ABI/identity/snapshot boundary without changing RNG cadence.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/DoomRPG.h").read_text(encoding="latin-1")
CORE = (ROOT / "ESP32/src/engine_stubs.c").read_text()
PROVIDER = (ROOT / "ESP32/src/esp_doomrpg_process_rng.c").read_text()
OWNER_HEADER = (ROOT / "ESP32/include/esp_native_process_rng_owner.h").read_text()
REPLAY = (ROOT / "ESP32/src/esp_native_rng_replay_guard.c").read_text()

assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)"
    r"\s+struct Random_s\s+random;\s+#endif", HEADER)
assert "_Static_assert(sizeof(DoomRPG_t) == 28U," in CORE
assert "_Static_assert(sizeof(Random_t) == 132U," in PROVIDER
assert "static Random_t nativeProcessRandom;" in PROVIDER
assert "return &nativeProcessRandom;" in PROVIDER
assert "Random_t* EspNativeProcessRng_live(void);" in OWNER_HEADER
assert CORE.count("DoomRPG_setRand(EspNativeProcessRng_live());") == 1
assert "DoomRPG_setRand(&rngReplayGuard.postRefill)" in REPLAY
assert "memcmp(liveRandom, &rngReplayGuard.preRefill" in REPLAY
assert "rngReplayGuard.probeReservedRand == liveRandom" in REPLAY
assert "resetRand = 0" in PROVIDER and "_seed = 0" in PROVIDER
assert "DoomRPG_setRand(Random_t* rand)" in PROVIDER

count = 0
for base in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for path in sorted(base.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        count += 1
        src = path.read_text(encoding="latin-1")
        src = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                     " ", src, flags=re.S)
        # The 'runtime' form occurs only where runtime is a DoomRPG_t*.
        assert not re.search(
            r"\b(?:doomRpg|doomrpg|runtime)\s*->\s*random\b",
            src), (path.relative_to(ROOT), "desktop RNG root leaked")
assert count >= 300, count

cc = shutil.which("cc") or "cc"
with tempfile.TemporaryDirectory(prefix="root-e-rng-owner-") as tmp:
    path = Path(tmp) / "probe.c"
    command = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-fsyntax-only", "-IESP32/include", "-Isrc"]
    path.write_text(
        '#include <SDL.h>\n#include "DoomRPG.h"\n'
        'int original_root_rng(DoomRPG_t* r) { return r->random.nextRand; }\n')
    for name, flags, available in (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1",
                     "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("oracle", ["-DDOOMRPG_ESP32=1",
                    "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], True),
        ("desktop", [], True),
    ):
        result = subprocess.run(command + flags + [str(path)], cwd=ROOT,
                                capture_output=True, text=True)
        assert (result.returncode == 0) == available, (name, result.stderr)
        if not available:
            assert "random" in result.stderr, name
    path.write_text(r"""
#include <SDL.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "DoomRPG.h"
#include "esp_native_process_rng_owner.h"
unsigned int DoomRPG_GetTimeMS(void) { return 1234U; }
int main(void) {
    Random_t* p = EspNativeProcessRng_live();
    assert(p != NULL && p == EspNativeProcessRng_live());
    assert(sizeof(*p) == 132U);
    for (unsigned int i = 0; i < 128U; ++i) assert(p->randTable[i] == 0);
    assert(p->nextRand == 0);
    DoomRPG_setRand(p);
    unsigned int nonzero = 0;
    for (unsigned int i = 0; i < 128U; ++i) nonzero += (p->randTable[i] != 0U);
    assert(nonzero > 0U && p->nextRand == 0);
    Random_t before = *p;
    p->nextRand = 127;
    p->randTable[127] ^= 0x5aU;
    assert(p == EspNativeProcessRng_live());
    *p = before;
    assert(memcmp(p, &before, sizeof(before)) == 0);
    assert(p == EspNativeProcessRng_live());
    return 0;
}
""")
    exe = Path(tmp) / "probe"
    subprocess.run([cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-DDOOMRPG_ESP32=1", "-IESP32/include", "-Isrc",
                    "ESP32/src/esp_doomrpg_process_rng.c", str(path),
                    "-o", str(exe)], cwd=ROOT, check=True)
    subprocess.run([str(exe)], check=True)
print(f"PASS Root-E process RNG 132B stable native BSS; root=28B; "
      f"{count} native C/C++ headers scanned; desktop/oracle retained")
