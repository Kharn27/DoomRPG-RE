#!/usr/bin/env python3
"""ARCH-RC17: both actual native public RNG draw bodies plus adversarial C mutants.

Run the existing 12k-step exact legacy generator oracle and its real native
replay-guard C implementation. Under mutation, ONLY a bounded C draw body is
altered: the actual generator, hidden seed, ABI and test assertions stay real.
No synthetic Python emulation and no assertion about hardware entropy/timing.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "ESP32/src/esp_native_rng_replay_guard.c"
HOST = ROOT / "ESP32/test/doomrpg_process_rng_host.py"
INI = ROOT / "ESP32/platformio.ini"
BYTE = "byte DoomRPG_randNextByte(Random_t* rand) {"
WORD = "int DoomRPG_randNextInt(Random_t* rand) {"


def require(ok, why):
    if not ok:
        raise AssertionError(why)


def extract(source, signature):
    require(source.count(signature) == 1, "real C RNG draw absent/drift: " + signature)
    start = source.index(signature)
    depth = 0
    for i in range(source.index("{", start), len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i+1]
    raise AssertionError("unbalanced actual C function")


MUTANTS = (
    ("byte-stale-offset", BYTE,
     "rand->nextRand = next + (int)sizeof(byte);",
     "rand->nextRand = next + 2;"),
    ("byte-corrupt-stream", BYTE,
     "return rand->randTable[next];",
     "return (byte)(rand->randTable[next] ^ 1U);"),
    ("byte-skip-boundary", BYTE,
     "if (atByteBoundary(rand)) {",
     "if (!atByteBoundary(rand)) {"),
    ("byte-forget-reservation", BYTE,
     "if (rngReplayGuard.probeReserved != 0U) {",
     "if (rngReplayGuard.probeReserved == 0U) {"),
    ("byte-ignore-lease", BYTE,
     "else if (leaseActive(now) &&",
     "else if (!leaseActive(now) &&"),
    ("byte-wrong-reserved-state", BYTE,
     "*rand = rngReplayGuard.postRefill;",
     "*rand = rngReplayGuard.preRefill;"),
    ("word-stale-offset", WORD,
     "rand->nextRand = next + (int)sizeof(uint32_t);",
     "rand->nextRand = next + 1;"),
    ("word-corrupt-stream", WORD,
     "return (int32_t)word;",
     "return (int32_t)(word ^ 1U);"),
    ("word-skip-boundary", WORD,
     "if (atWordBoundary(rand)) {",
     "if (!atWordBoundary(rand)) {"),
    ("word-shift-not-little-endian", WORD,
     "((uint32_t)rand->randTable[next + 1] << 8)",
     "((uint32_t)rand->randTable[next + 1] << 9)"),
    ("word-forget-lease", WORD,
     "else if (leaseActive(now) &&",
     "else if (!leaseActive(now) &&"),
    ("word-wrong-first-byte", WORD,
     "word = (uint32_t)rand->randTable[next] |",
     "word = (uint32_t)rand->randTable[next + 1] |"),
)


def source_scan(source):
    cfg = INI.read_text()
    normal = cfg.split("[env:esp32-cyd]", 1)[1].split(
        "[env:esp-cyd-inverted]", 1)[0]
    require(len(re.findall(r"^\s+-Wl,--wrap=", normal, re.M)) == 2,
            "expected exact 2 normal GNU wrappers after RC22")
    require(BYTE in source and WORD in source,
            "permanent native RNG definitions absent")
    for symbol in ("DoomRPG_randNextByte", "DoomRPG_randNextInt"):
        require("-Wl,--wrap=" + symbol not in cfg,
                "retired RNG linker interposition returned")
    require("#include \"DoomRPG.h\"" in source,
            "RNG must preserve original Random_t ABI/header")
    count = 0
    for path in (ROOT / "ESP32/src").iterdir():
        if path.suffix not in (".c", ".cpp"):
            continue
        count += 1
        data = path.read_text(encoding="latin-1")
        code = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", data)
        require(not re.search(
            r"\b__(?:wrap|real)_DoomRPG_randNext(?:Byte|Int)\b", code),
            "retired alias survives in " + path.name)
    require(count >= 178, "firmware source census incomplete")
    for sig in (BYTE, WORD):
        extract(source, sig)
    print("PASS ARCH-RC17 source: public byte/word owner, 2 wraps, desktop C absent")


def with_mutation(source, signature, before, after, name):
    body = extract(source, signature)
    count = body.count(before)
    require(count > 0, "real C mutant anchor missing: " + name)
    edited = body.replace(before, after, 1)
    require(edited != body, "mutant did not modify actual draw body: " + name)
    return source.replace(body, edited, 1)


def exercise(actual_source, tag):
    # Copy the existing differential harness verbatim except for (1) its
    # resolved project root and (2) one guard-source path. It still compiles
    # the permanent native generator, immutable legacy generator references,
    # and exactly the actual public byte+word C functions, each in real TUs.
    with tempfile.TemporaryDirectory(prefix="rc17-rng-") as d:
        p = Path(d)
        native = p / "guard.c"
        native.write_text(actual_source)
        script = HOST.read_text()
        root_anchor = "ROOT = Path(__file__).resolve().parents[2]"
        guard_anchor = 'str(ROOT / "ESP32/src/esp_native_rng_replay_guard.c")'
        require(script.count(root_anchor) == 1 and
                script.count(guard_anchor) == 1,
                "original host C oracle compiler anchor drift")
        script = script.replace(root_anchor,
                                "ROOT = Path(" + repr(str(ROOT)) + ")")
        script = script.replace(guard_anchor,
                                "str(Path(" + repr(str(native)) + "))")
        test = p / "test.py"
        test.write_text(script)
        out = subprocess.run([sys.executable, str(test)], cwd=ROOT,
                             capture_output=True, text=True, timeout=45)
        # A broken compiler invocation is NOT a valid semantic mutant kill.
        require("error:" not in out.stderr and
                "Command '['cc'" not in out.stderr,
                "mutant did not compile: " + tag + " " + out.stderr[-1300:])
        return (out.returncode == 0 and
                out.stdout.count("PASS RNG source/state/refill/replay") == 4)


def main():
    source = SOURCE.read_text()
    source_scan(source)
    require(exercise(source, "real-public"), "actual C RNG public owner violated oracle")
    for name, signature, before, after in MUTANTS:
        mutated = with_mutation(source, signature, before, after, name)
        require(not exercise(mutated, name),
                "adversarial semantic mutation survived: " + name)
    print("PASS ARCH-RC17: actual public byte/word C, 12 adversarial mutants rejected")


if __name__ == "__main__":
    main()
