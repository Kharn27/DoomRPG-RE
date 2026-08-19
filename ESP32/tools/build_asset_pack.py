#!/usr/bin/env python3
"""Build the complete ESP32-native Doom RPG asset pack from DoomRPG.zip.

Pack v2 stores every ZIP entry uncompressed behind a fixed-size on-disk index.
The ESP32 therefore performs bounded seek/read operations and never needs to
materialize a ZIP entry or the complete index in RAM.
"""

from __future__ import annotations

import argparse
import shutil
import struct
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path

MAGIC = b"DRPGESP2"
VERSION = 2
FLAG_DIRECTORY = 0x00000001
HEADER = struct.Struct("<8sIIII")
ENTRY = struct.Struct("<IIIII")
COPY_CHUNK_BYTES = 64 * 1024


@dataclass
class AssetRecord:
    zip_info: zipfile.ZipInfo
    normalized_name: str
    name_hash: int
    offset: int = 0

    @property
    def size(self) -> int:
        return self.zip_info.file_size

    @property
    def crc32(self) -> int:
        return self.zip_info.CRC & 0xFFFFFFFF

    @property
    def flags(self) -> int:
        return FLAG_DIRECTORY if self.zip_info.is_dir() else 0


def normalize_name(name: str) -> str:
    normalized = name.replace("\\", "/")
    normalized = normalized.lstrip("/")
    if not normalized:
        raise RuntimeError("ZIP contains an empty asset name")

    try:
        normalized.encode("ascii")
    except UnicodeEncodeError as exc:
        raise RuntimeError(
            f"non-ASCII asset name is unsupported by the ESP32 pack: {name!r}"
        ) from exc

    return normalized.lower()


def fnv1a32_name(normalized_name: str) -> int:
    value = 0x811C9DC5
    for byte in normalized_name.encode("ascii"):
        value ^= byte
        value = (value * 0x01000193) & 0xFFFFFFFF
    return value


def collect_records(zf: zipfile.ZipFile) -> list[AssetRecord]:
    records: list[AssetRecord] = []
    hashes: dict[int, str] = {}
    normalized_names: set[str] = set()

    for info in zf.infolist():
        normalized = normalize_name(info.filename)
        if normalized in normalized_names:
            raise RuntimeError(
                f"duplicate case-insensitive asset name in ZIP: {info.filename!r}"
            )
        normalized_names.add(normalized)

        name_hash = fnv1a32_name(normalized)
        if name_hash == 0:
            raise RuntimeError(
                f"asset name produced reserved zero hash: {info.filename!r}"
            )
        previous = hashes.get(name_hash)
        if previous is not None:
            raise RuntimeError(
                "FNV-1a name hash collision: "
                f"{previous!r} and {normalized!r} -> {name_hash:08x}"
            )
        hashes[name_hash] = normalized
        records.append(AssetRecord(info, normalized, name_hash))

    if not records:
        raise RuntimeError("source ZIP contains no entries")

    # Stable payload ordering makes identical input ZIP contents produce an
    # identical pack even if the original central-directory ordering changes.
    records.sort(key=lambda item: item.normalized_name)
    return records


def assign_payload_offsets(records: list[AssetRecord]) -> int:
    data_offset = HEADER.size + (len(records) * ENTRY.size)
    offset = data_offset

    for record in records:
        record.offset = offset
        if not record.zip_info.is_dir():
            offset += record.size

    return data_offset


def write_pack(
    zf: zipfile.ZipFile,
    records: list[AssetRecord],
    output_pack: Path,
    data_offset: int,
) -> None:
    index_records = sorted(records, key=lambda item: item.name_hash)

    output_pack.parent.mkdir(parents=True, exist_ok=True)
    with output_pack.open("wb") as out:
        out.write(
            HEADER.pack(
                MAGIC,
                VERSION,
                len(index_records),
                HEADER.size,
                data_offset,
            )
        )

        for record in index_records:
            out.write(
                ENTRY.pack(
                    record.name_hash,
                    record.offset,
                    record.size,
                    record.crc32,
                    record.flags,
                )
            )

        if out.tell() != data_offset:
            raise RuntimeError(
                f"internal index size mismatch: {out.tell()} != {data_offset}"
            )

        for record in records:
            if record.zip_info.is_dir():
                continue
            if out.tell() != record.offset:
                raise RuntimeError(
                    f"payload offset mismatch for {record.normalized_name}: "
                    f"{out.tell()} != {record.offset}"
                )
            with zf.open(record.zip_info, "r") as source:
                shutil.copyfileobj(source, out, COPY_CHUNK_BYTES)


def verify_pack(output_pack: Path, records: list[AssetRecord], data_offset: int) -> None:
    expected_by_hash = {record.name_hash: record for record in records}
    expected_size = data_offset + sum(
        record.size for record in records if not record.zip_info.is_dir()
    )

    with output_pack.open("rb") as pack:
        raw_header = pack.read(HEADER.size)
        if len(raw_header) != HEADER.size:
            raise RuntimeError("generated pack header is truncated")

        magic, version, count, index_offset, stored_data_offset = HEADER.unpack(
            raw_header
        )
        if (
            magic != MAGIC
            or version != VERSION
            or count != len(records)
            or index_offset != HEADER.size
            or stored_data_offset != data_offset
        ):
            raise RuntimeError("generated pack header failed self-verification")

        previous_hash = -1
        for _ in range(count):
            raw_entry = pack.read(ENTRY.size)
            if len(raw_entry) != ENTRY.size:
                raise RuntimeError("generated pack index is truncated")
            name_hash, offset, size, crc32, flags = ENTRY.unpack(raw_entry)
            if name_hash <= previous_hash:
                raise RuntimeError("generated pack index is not strictly hash-sorted")
            previous_hash = name_hash

            expected = expected_by_hash.get(name_hash)
            if expected is None:
                raise RuntimeError(f"unexpected name hash in pack: {name_hash:08x}")
            if (
                offset != expected.offset
                or size != expected.size
                or crc32 != expected.crc32
                or flags != expected.flags
            ):
                raise RuntimeError(
                    f"index mismatch for {expected.normalized_name}"
                )

    if output_pack.stat().st_size != expected_size:
        raise RuntimeError(
            f"generated pack size mismatch: {output_pack.stat().st_size} "
            f"!= {expected_size}"
        )


def build_pack(source_zip: Path, output_pack: Path, list_entries: bool) -> None:
    if source_zip == output_pack:
        raise RuntimeError("source ZIP and output pack must be different files")

    with zipfile.ZipFile(source_zip, "r") as zf:
        records = collect_records(zf)
        data_offset = assign_payload_offsets(records)
        write_pack(zf, records, output_pack, data_offset)

    verify_pack(output_pack, records, data_offset)

    payload_bytes = sum(
        record.size for record in records if not record.zip_info.is_dir()
    )
    directory_count = sum(1 for record in records if record.zip_info.is_dir())

    print(f"[PACK] source: {source_zip}")
    print(f"[PACK] output: {output_pack}")
    print(f"[PACK] format: v{VERSION} / hash-sorted on-disk index")
    print(
        f"[PACK] entries: {len(records)} "
        f"(files={len(records) - directory_count}, dirs={directory_count})"
    )
    print(
        f"[PACK] header+index: {data_offset} bytes "
        f"({HEADER.size} + {len(records)} x {ENTRY.size})"
    )
    print(f"[PACK] payload: {payload_bytes} bytes (uncompressed, directly seekable)")
    print(f"[PACK] total: {output_pack.stat().st_size} bytes")
    print("[PACK] self-check: OK")

    critical = {"bitshapes.bin", "wtexels.bin", "stexels.bin", "mappings.bin", "menu.bsp"}
    for record in records:
        if list_entries or record.normalized_name in critical:
            print(
                f"[PACK] {record.normalized_name:24s} "
                f"hash={record.name_hash:08x} offset={record.offset:8d} "
                f"size={record.size:7d} crc32={record.crc32:08x} "
                f"flags={record.flags}"
            )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build the complete DoomRPG-ESP32.pak from DoomRPG.zip"
    )
    parser.add_argument("source_zip", type=Path, help="path to DoomRPG.zip")
    parser.add_argument(
        "output_pack",
        type=Path,
        nargs="?",
        help="output path (default: DoomRPG-ESP32.pak beside the ZIP)",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        dest="list_entries",
        help="print every generated pack entry instead of only key resources",
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
        build_pack(source_zip, output_pack, args.list_entries)
    except (OSError, RuntimeError, zipfile.BadZipFile) as exc:
        print(f"[PACK] ERROR: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
