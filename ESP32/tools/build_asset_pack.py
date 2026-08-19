#!/usr/bin/env python3
"""Build the ESP32-native heavy graphics asset pack from DoomRPG.zip.

The ESP32 must not inflate bitshapes.bin, wtexels.bin or stexels.bin as whole
files at runtime. This tool performs that work offline and stores the three
uncompressed resources behind a tiny fixed index so the CYD can seek/read only
the bytes it needs.
"""

from __future__ import annotations

import argparse
import struct
import sys
import zipfile
import zlib
from pathlib import Path

MAGIC = b"DRPGESP1"
VERSION = 1
ASSETS = ("bitshapes.bin", "wtexels.bin", "stexels.bin")
HEADER = struct.Struct("<8sII")
ENTRY = struct.Struct("<16sIIII")


def resolve_member(zf: zipfile.ZipFile, wanted: str) -> str:
    matches = {name.lower(): name for name in zf.namelist()}
    try:
        return matches[wanted.lower()]
    except KeyError as exc:
        raise RuntimeError(f"{wanted} is missing from {zf.filename}") from exc


def build_pack(source_zip: Path, output_pack: Path) -> None:
    payloads: list[tuple[str, bytes, int]] = []

    with zipfile.ZipFile(source_zip, "r") as zf:
        for asset_name in ASSETS:
            member = resolve_member(zf, asset_name)
            data = zf.read(member)
            crc = zlib.crc32(data) & 0xFFFFFFFF
            payloads.append((asset_name, data, crc))

    first_payload_offset = HEADER.size + (len(payloads) * ENTRY.size)
    offset = first_payload_offset
    entries: list[tuple[str, int, int, int]] = []

    for name, data, crc in payloads:
        entries.append((name, offset, len(data), crc))
        offset += len(data)

    output_pack.parent.mkdir(parents=True, exist_ok=True)
    with output_pack.open("wb") as out:
        out.write(HEADER.pack(MAGIC, VERSION, len(entries)))

        for name, entry_offset, size, crc in entries:
            encoded = name.encode("ascii")
            if len(encoded) >= 16:
                raise RuntimeError(f"asset name too long for pack index: {name}")
            out.write(
                ENTRY.pack(
                    encoded.ljust(16, b"\0"),
                    entry_offset,
                    size,
                    crc,
                    0,
                )
            )

        for _name, data, _crc in payloads:
            out.write(data)

    print(f"[PACK] source: {source_zip}")
    print(f"[PACK] output: {output_pack}")
    print(f"[PACK] header+index: {first_payload_offset} bytes")
    for name, entry_offset, size, crc in entries:
        print(
            f"[PACK] {name:13s} offset={entry_offset:6d} "
            f"size={size:6d} crc32={crc:08x}"
        )
    print(f"[PACK] total: {output_pack.stat().st_size} bytes")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build DoomRPG-ESP32.pak from the existing DoomRPG.zip"
    )
    parser.add_argument("source_zip", type=Path, help="path to DoomRPG.zip")
    parser.add_argument(
        "output_pack",
        type=Path,
        nargs="?",
        help="output path (default: DoomRPG-ESP32.pak beside the ZIP)",
    )
    args = parser.parse_args()

    source_zip = args.source_zip.expanduser().resolve()
    if not source_zip.is_file():
        parser.error(f"source ZIP not found: {source_zip}")

    output_pack = args.output_pack
    if output_pack is None:
        output_pack = source_zip.with_name("DoomRPG-ESP32.pak")
    else:
        output_pack = output_pack.expanduser().resolve()

    try:
        build_pack(source_zip, output_pack)
    except (OSError, RuntimeError, zipfile.BadZipFile) as exc:
        print(f"[PACK] ERROR: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
