#!/usr/bin/env python3
"""Audit native root RNG coupling before the Root-E ownership extraction.

This is a read-only inventory. In particular, replay-guard reservations
compare Random_t* identity, so changing its owner is NOT a blind ABI shrink.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
PATTERNS = (
    ("native-owner", re.compile(r"\bEspNativeProcessRng_live\s*\(")),
    ("legacy-root", re.compile(r"\b(?:doomRpg|doomrpg|runtime)\s*->\s*random\b")),
    ("byte-api", re.compile(r"\bDoomRPG_randNextByte\s*\(")),
    ("int-api", re.compile(r"\bDoomRPG_randNextInt\s*\(")),
    ("seed", re.compile(r"\bDoomRPG_setRand\s*\(")),
    ("replay-guard", re.compile(r"\bEspNativeRngReplayGuard_(?:beginAttackProbe|beginProbeBoundary|endProbeBoundary|endAttackProbe|commitProbeBoundary)\s*\(")),
)
counts = {k: 0 for k, _ in PATTERNS}
files = 0
for folder in ("ESP32/src", "ESP32/include"):
    for path in sorted((ROOT / folder).rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        files += 1
        raw = path.read_text(encoding="latin-1")
        stripped = re.sub(
            r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
            lambda m: "\n" * m.group(0).count("\n"), raw,
            flags=re.S,
        )
        for number, line in enumerate(stripped.splitlines(), 1):
            for name, regex in PATTERNS:
                if regex.search(line):
                    counts[name] += 1
                    print(f"ROOT_RNG_ACCESS kind={name} {path.relative_to(ROOT)}:{number} {line.strip()[:175]}")
print(f"ROOT_RNG_INVENTORY files={files} " +
      " ".join(f"{k}={v}" for k, v in counts.items()))
assert files >= 300, "native inventory incomplete"
assert counts["native-owner"] >= 150, "native RNG consumer inventory incomplete"
assert counts["legacy-root"] == 0, "legacy inline RNG root access forbidden"
