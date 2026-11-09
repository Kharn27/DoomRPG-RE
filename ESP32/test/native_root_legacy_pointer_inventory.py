#!/usr/bin/env python3
"""Native-only inventory of dormant desktop root owner field references."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
NAMES = ("player", "game", "entityDef", "combat", "hud", "menu",
         "particleSystem", "sound", "errorID", "upTimeMs")
counts = {name: [] for name in NAMES}
files = 0
for base in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for path in sorted(base.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        files += 1
        original = path.read_text(encoding="latin-1")
        scrubbed = re.sub(
            r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
            lambda m: "\n" * m.group(0).count("\n"), original,
            flags=re.S,
        )
        for n, line in enumerate(scrubbed.splitlines(), 1):
            for name in NAMES:
                if re.search(r"->\s*" + name + r"\b", line):
                    counts[name].append((path.relative_to(ROOT).as_posix(), n, line.strip()[:120]))

print(f"ROOT_POINTER_INVENTORY files={files}")
for name, hits in counts.items():
    print(f"ROOT_POINTER_INVENTORY {name} accesses={len(hits)} "
          f"nativeFiles={len(set(p for p, _, _ in hits))}")
    for p, n, line in hits[:60]:
        print(f"ROOT_POINTER_ACCESS {name} {p}:{n} {line}")
    if len(hits) > 60:
        print(f"ROOT_POINTER_ACCESS {name} remaining={len(hits)-60}")
assert files >= 300, "native inventory incomplete"
