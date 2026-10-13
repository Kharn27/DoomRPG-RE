#!/usr/bin/env python3
"""Compare loaded ELF32 sections, not debug data or discarded map entries.

Host-only audit tool. No image metadata is masked: every non-empty SHF_ALLOC
section must retain its address, layout and bytes, including the app descriptor.
NOBITS sections are compared by layout. This is not a runtime execution trace.
"""

import argparse
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess


def allocated_sections(data):
    if len(data) < 52 or data[:6] != b"\x7fELF\x01\x01":
        raise ValueError("expected little-endian ELF32")
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    machine, entry, shoff, shsize, shcount, names_index = (
        header[2], header[4], header[6], header[11], header[12], header[13]
    )
    if shsize != 40 or not shcount or not 0 < names_index < shcount:
        raise ValueError("unsupported or missing section table")
    if shoff + shsize * shcount > len(data):
        raise ValueError("truncated section table")
    sections = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shsize)
                for i in range(shcount)]
    names = sections[names_index]
    if names[4] + names[5] > len(data):
        raise ValueError("truncated section names")
    strings = data[names[4]:names[4] + names[5]]
    result = {}
    for name_offset, kind, flags, address, offset, size, _, _, align, _ in sections:
        if not flags & 2 or not size:
            continue
        if name_offset >= len(strings) or b"\0" not in strings[name_offset:]:
            raise ValueError("invalid allocated section name")
        name = strings[name_offset:].split(b"\0", 1)[0].decode("ascii")
        if name in result:
            raise ValueError("duplicate allocated section name: " + name)
        if kind != 8 and offset + size > len(data):
            raise ValueError("truncated allocated section: " + name)
        payload = None if kind == 8 else data[offset:offset + size]
        result[name] = (kind, flags, address, size, align, payload)
    if not result:
        raise ValueError("no allocated sections")
    return machine, entry, result


def compare(baseline, candidate):
    before_machine, before_entry, before = allocated_sections(baseline)
    after_machine, after_entry, after = allocated_sections(candidate)
    if (before_machine, before_entry) != (after_machine, after_entry):
        raise ValueError("machine or entry point changed")
    changed = [name for name in sorted(before.keys() | after.keys())
               if before.get(name) != after.get(name)]
    if changed:
        raise ValueError("allocated sections changed: " + ", ".join(changed))
    return before


def assert_absent(candidate, nm, prefixes):
    output = subprocess.check_output([nm, "--defined-only", str(candidate)], text=True)
    names = [line.split()[-1] for line in output.splitlines() if line.split()]
    unexpected = [name for name in names if any(name.startswith(p) for p in prefixes)]
    if unexpected:
        raise ValueError("retired symbols linked: " + ", ".join(unexpected))
    print("Retired symbol prefixes absent: " + ", ".join(prefixes))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--forbid-prefix", action="append", default=[])
    parser.add_argument("--nm", default=shutil.which("xtensa-esp32-elf-nm") or str(
        Path.home() / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm"))
    args = parser.parse_args()
    try:
        data = args.candidate.read_bytes()
        sections = allocated_sections(data)[2]
        if args.baseline:
            sections = compare(args.baseline.read_bytes(), data)
            print("ALLOC IDENTICAL: addresses/layout/bytes, %u sections" % len(sections))
        for name, (_, _, address, size, _, payload) in sorted(sections.items()):
            digest = "NOBITS" if payload is None else hashlib.sha256(payload).hexdigest()
            print("%s address=%08x bytes=%u sha256=%s" % (name, address, size, digest))
        if args.forbid_prefix:
            assert_absent(args.candidate, args.nm, args.forbid_prefix)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, "FAIL: %s\n" % error)


if __name__ == "__main__":
    main()
