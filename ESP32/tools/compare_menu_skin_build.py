#!/usr/bin/env python3
"""Compare two *separately compiled*, same-env CYD firmware ELFs and images.
Run after pio builds (usually in one CI runner); never interpret as CYD RAM. """
import re
import shutil
import subprocess
import sys
from pathlib import Path

if len(sys.argv) != 5:
    raise SystemExit("usage: compare_menu_skin_build.py BASE.elf CAND.elf BASE.bin CAND.bin")

base_elf, candidate_elf, base_bin, candidate_bin = [Path(x) for x in sys.argv[1:]]
for path in (base_elf, candidate_elf, base_bin, candidate_bin):
    if not path.is_file():
        raise SystemExit(f"[MENUSKIN-COST] missing: {path}")

objdump = shutil.which("xtensa-esp32-elf-objdump")
if not objdump:
    packages = Path.home() / ".platformio/packages"
    candidates = list(packages.glob("toolchain-xtensa-esp32*/bin/xtensa-esp32-elf-objdump"))
    if not candidates:
        raise SystemExit("[MENUSKIN-COST] missing ESP32 objdump")
    objdump = str(candidates[0])

def sections(elf):
    output = subprocess.check_output([objdump, "-h", str(elf)], text=True)
    found = {}
    for line in output.splitlines():
        m = re.match(r"^\s*\d+\s+(\.\S+)\s+([0-9a-fA-F]+)\s+", line)
        if m:
            found[m.group(1)] = int(m.group(2), 16)
    if not found:
        raise RuntimeError(f"no section table: {elf}")
    return found

before = sections(base_elf)
after = sections(candidate_elf)
print(f"[MENUSKIN-COST] base-elf={base_elf} candidate-elf={candidate_elf}")
for label, b, a in (("firmware.bin", base_bin.stat().st_size, candidate_bin.stat().st_size),
                    ("firmware.elf", base_elf.stat().st_size, candidate_elf.stat().st_size)):
    print(f"[MENUSKIN-COST] {label} before={b} after={a} delta={a-b:+d} bytes")
for name in sorted(before.keys() | after.keys()):
    b = before.get(name, 0)
    a = after.get(name, 0)
    if b != a:
        print(f"[MENUSKIN-COST] section={name} before={b} after={a} delta={a-b:+d}")
for family, suffixes in (
    (".text", (".text",)),
    (".rodata", (".rodata",)),
    (".data", (".data",)),
    (".bss", (".bss",)),
):
    selected = {name for name in before.keys() | after.keys()
                if any(name.endswith(suffix) for suffix in suffixes)}
    b = sum(before.get(name, 0) for name in selected)
    a = sum(after.get(name, 0) for name in selected)
    print(f"[MENUSKIN-COST] {family}-family before={b} after={a} delta={a-b:+d} bytes")
print("[MENUSKIN-COST] section accounting=xtensa-objdump actual compiled ELF; "
      "runtime heap8/timing must come from real CYD Serial; this is not hardware PASS")
