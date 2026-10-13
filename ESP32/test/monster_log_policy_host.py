#!/usr/bin/env python3
"""Check real monster TUs at INFO/TRACE; retain failure and watcher witnesses.

This tests compilation/diagnostic admission, not AI/combat semantics or CYD
timings. --measure-log estimates removed Serial bytes in an existing capture;
UART line time is not a measured CPU/gameplay speedup.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CC = shlex.split(os.environ.get("CC", "cc"))
FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable",
         "-IESP32/test/host_include", "-IESP32/include", "-IESP32/src", "-Isrc"]
# The existing Turn attackSequence local is TRACE-only; don't turn that old
# unused-variable warning into a new unrelated code edit. Other warnings fail.
SITES = {
    "active_sequence": {"[MONSTERACTIVESEQ] RESUME ": 1,
                        "[MONSTERACTIVESEQ] PAUSE ": 1,
                        "[MONSTERACTIVESEQ] PAUSE-FINAL ": 1},
    "movement": {"[MONSTERMOVE] TRACE-BLOCK ": 3},
    "movement_activation": {},
    "movement_probe": {"[MONSTERPOSTMOVE] SKIP ": 1},
    "movement_publish": {"[MONSTERMOVELIVE] COMMIT ": 1},
    "turn": {"[IMPAI] TARGET ": 4, "[MONSTERTURN] MEMBER-MOVE ": 1,
             "[MONSTERPOSTMOVE] COMPLETE ": 3},
}
GUARDS = {
    "active_sequence": r'if\s*\(!transactionStatus\)\s*printf\("\[MONSTERACTIVESEQ\] MEMBER ',
    "movement": r'if\s*\(!positionExact\s*\|\|\s*!randomExact\)\s*printf\("\[MONSTERMOVE\] PROBE ',
    "movement_activation": r'if\s*\(!prepared\)\s*printf\("\[MONSTERMOVEACT\] %s ',
}


def run(args):
    result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    return result.stdout


def check():
    outputs = {}
    for suffix, markers in SITES.items():
        source = f"ESP32/src/esp_native_gameplay_monster_{suffix}.c"
        for level in (2, 4):
            flags = CC + FLAGS + [f"-DDOOMRPG_LOG_LEVEL={level}"]
            run(flags + ["-fsyntax-only", source])
            outputs[suffix, level] = text = run(flags + ["-E", source])
            for marker, count in markers.items():
                assert text.count(marker) == (count if level == 4 else 0), (suffix, level, marker)
        if suffix in GUARDS:
            assert re.search(GUARDS[suffix], outputs[suffix, 2]), suffix
            assert not re.search(GUARDS[suffix], outputs[suffix, 4]), suffix
        # Every failure literal admitted at TRACE must still exist at INFO.
        # One pre-existing zero-candidate DEFER is deliberately already TRACE;
        # this set comparison doesn't pretend to validate its branch predicate.
        failures = set(re.findall(
            r'"(\[[^"\n]+\] (?:[^"\n]*?DEFER|FATAL[^"\n]*|REPLAY-MISMATCH|ROLLBACK)[^"\n]*)"',
            outputs[suffix, 4]))
        for literal in failures:
            assert literal in outputs[suffix, 2], (suffix, literal)

    # Keep production attack-probe evidence needed by playtest_watch RNG checks.
    for marker in ("[MONSTERTURN] MEMBER-ATTACK-PROBE ",
                   "[MONSTERPOSTMOVE] ATTACK-PROBE ",
                   "[MONSTER3ATTACK] ATTACK-PROBE ",
                   "[MONSTERTURN] ORDERED-DISPATCH "):
        assert marker in outputs["turn", 2], marker
    assert "[MONSTERACTIVESEQ] BEGIN " in outputs["active_sequence", 2]
    assert "[MONSTERACTIVESEQ] COMPLETE " in outputs["active_sequence", 2]

    with tempfile.TemporaryDirectory(prefix="doomrpg-monster-log-test-") as tmp:
        for level in (2, 4):
            binary = str(Path(tmp) / f"activation-{level}")
            run(CC + FLAGS + [f"-DDOOMRPG_LOG_LEVEL={level}",
                "ESP32/test/monster_activation_log_host.c",
                "ESP32/src/esp_native_gameplay_monster_movement_activation.c", "-o", binary])
            text = run([binary])
            assert text.count("[MONSTERMOVEACT] DEFER ") == 2
            assert text.count("[MONSTERMOVEACT] ALLOW ") == (1 if level == 4 else 0)
            assert "PASS admission/capture preserved" in text
    print("monster_log_policy_host: PASS 16 TRACE-only sites, 3 failure guards, watcher evidence, INFO/TRACE capture")


def measure(path):
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)
    prefixes = tuple(marker for markers in SITES.values() for marker in markers)
    def quiet(line):
        if line.startswith(prefixes):
            return True
        if line.startswith("[MONSTERMOVEACT] ALLOW "):
            return True
        if line.startswith("[MONSTERMOVE] PROBE "):
            return "randomLiveUntouched=yes" in line and "positionRollback=yes" in line
        if line.startswith("[MONSTERACTIVESEQ] MEMBER "):
            return "movement=deferred" not in line
        return False
    total = sum(len(line.encode("utf-8")) for line in lines)
    removed = [line for line in lines if quiet(line)]
    removed_bytes = sum(len(line.encode("utf-8")) for line in removed)
    print(f"capture: {len(lines)} lines / {total} bytes; INFO removes {len(removed)} lines / {removed_bytes} bytes")
    print(f"UART 115200 8N1 equivalent: {removed_bytes * 10 / 115200 * 1000:.1f} ms, NOT measured gameplay gain")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--measure-log", type=Path)
    args = parser.parse_args()
    check()
    if args.measure_log:
        measure(args.measure_log)
