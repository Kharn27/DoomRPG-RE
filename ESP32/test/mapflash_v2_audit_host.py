#!/usr/bin/env python3
"""MAPFLASH-V2/P0 host inventory and geometry oracle (does not alter firmware).

Run without a PAK for synthetic negative tests, or pass --pak to audit the
authoritative local DoomRPG-ESP32.pak. No ZIP/runtime dependency.
"""
from __future__ import annotations

import argparse
import re
import struct
import zlib
from pathlib import Path

HEADER = struct.Struct("<8sIIII")
ENTRY = struct.Struct("<IIIII")
SECTOR = 4096
PARTITION = 0x2A0000


def catalog() -> list[str]:
    source = Path(__file__).resolve().parents[1] / "src" / "esp_map_catalog.c"
    names = re.findall(r'"/[^"]+\\.bsp"', source.read_text(encoding="utf-8"))
    if len(names) != 13 or len(set(names)) != 13:
        raise ValueError("native map catalog changed: audit contract needs updating")
    return [name.strip('"') for name in names]


def name_hash(name: str) -> int:
    h = 2166136261
    for b in name.lstrip("/").lower().encode("ascii"):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def align_sector(value: int) -> int:
    return (value + SECTOR - 1) & ~(SECTOR - 1)


def inspect(blob: bytes, partition_bytes: int = PARTITION, verify_crc: bool = False) -> dict:
    if len(blob) < HEADER.size:
        raise ValueError("truncated PAK header")
    magic, version, count, index_offset, data_offset = HEADER.unpack_from(blob)
    if magic != b"DRPGESP2" or version != 2 or count < 1 or count > 4096:
        raise ValueError("invalid PAK header")
    index_bytes = count * ENTRY.size
    if (index_offset < HEADER.size or index_offset + index_bytes > data_offset
            or data_offset > len(blob)):
        raise ValueError("PAK index bounds")
    entries = {}
    last_hash = -1
    for i in range(count):
        h, off, size, crc, flags = ENTRY.unpack_from(blob, index_offset + i * ENTRY.size)
        if h == 0 or h <= last_hash or off < data_offset or off > len(blob) or size > len(blob) - off:
            raise ValueError("PAK entry bounds/order")
        last_hash = h
        entries[h] = (off, size, crc, flags)
        if verify_crc and not flags and zlib.crc32(blob[off:off + size]) != crc:
            raise ValueError(f"entry CRC mismatch {h:08x}")

    bsp = []
    for map_id, path in enumerate(catalog(), 1):
        entry = entries.get(name_hash(path))
        if entry is None:
            bsp.append(dict(id=map_id, name=path, source=0, bytes=0, present=False))
            continue
        off, size, _, flags = entry
        if size == 0 or flags:
            raise ValueError("invalid BSP entry")
        bsp.append(dict(id=map_id, name=path, source=off, bytes=size, present=True))
    spans = sorted((b["source"], b["source"] + b["bytes"]) for b in bsp if b["present"])
    for a, b in zip(spans, spans[1:]):
        if a[1] > b[0]:
            raise ValueError("overlapping BSPs")

    all_bsp = sum(b["bytes"] for b in bsp)
    data_bytes = len(blob) - data_offset
    if all_bsp > data_bytes:
        raise ValueError("BSP bytes exceed PAK payload")
    common = data_bytes - all_bsp
    maximum = max(b["bytes"] for b in bsp)
    v2_index_start = 2 * SECTOR
    common_offset = align_sector(v2_index_start + index_bytes)
    bsp_offset = align_sector(common_offset + common)
    reserved = align_sector(maximum)
    single_end = bsp_offset + reserved
    all_end = bsp_offset + all_bsp
    return dict(
        pak_bytes=len(blob), entries=count, data_bytes=data_bytes,
        index_bytes=index_bytes, common_bytes=common, all_bsp_bytes=all_bsp,
        max_bsp_bytes=maximum, common_flash_offset=common_offset,
        bsp_flash_offset=bsp_offset, bsp_slot_reserve=reserved,
        partition_bytes=partition_bytes,
        single_fits=single_end <= partition_bytes,
        single_headroom=max(0, partition_bytes - single_end),
        all_fits_lower_bound=all_end <= partition_bytes,
        all_headroom_lower_bound=max(0, partition_bytes - all_end),
        bsp=bsp,
    )


def synthetic_pack() -> bytes:
    names = [name for name in catalog()] + ["/wall.bin", "/sfx.wav"]
    payloads = {name: bytes([i % 255 + 1]) * (100 + i * 17)
                for i, name in enumerate(names)}
    ordered = sorted(names, key=lambda name: name.lower())
    data_offset = HEADER.size + len(names) * ENTRY.size
    position = data_offset
    records = []
    for name in ordered:
        payload = payloads[name]
        records.append((name_hash(name), position, len(payload), zlib.crc32(payload), 0))
        position += len(payload)
    header = HEADER.pack(b"DRPGESP2", 2, len(names), HEADER.size, data_offset)
    index = b"".join(ENTRY.pack(*record) for record in sorted(records))
    return header + index + b"".join(payloads[name] for name in ordered)


def self_test() -> None:
    blob = synthetic_pack()
    expected = inspect(blob, verify_crc=True)
    assert expected["entries"] == 15
    assert sum(b["present"] for b in expected["bsp"]) == 13
    assert expected["common_bytes"] == 100 + 13 * 17 + 100 + 14 * 17
    assert expected["single_fits"] and expected["all_fits_lower_bound"]
    assert not inspect(blob, partition_bytes=2 * SECTOR)["single_fits"]

    def rejected(broken: bytes) -> None:
        try:
            inspect(broken, verify_crc=True)
        except ValueError:
            return
        raise AssertionError("mutant accepted")

    rejected(blob[:10])
    rejected(b"BADMAGIC" + blob[8:])
    rejected(blob[:8] + struct.pack("<I", 3) + blob[12:])
    rejected(blob[:16] + struct.pack("<I", 0xFFFFFFF0) + blob[20:])
    rejected(blob[:-1])
    records = [list(ENTRY.unpack_from(blob, HEADER.size + i * ENTRY.size)) for i in range(15)]
    # Hash order is strict, even for data that does not represent a BSP.
    duplicate = bytearray(blob)
    ENTRY.pack_into(duplicate, HEADER.size + ENTRY.size, *records[0])
    rejected(bytes(duplicate))
    # Two BSPs must not overlap, even if the index itself is ordered by hash.
    map_hashes = {name_hash(name) for name in catalog()}
    indices = [i for i, row in enumerate(records) if row[0] in map_hashes]
    overlap = bytearray(blob)
    records[indices[1]][1] = records[indices[0]][1]
    ENTRY.pack_into(overlap, HEADER.size + indices[1] * ENTRY.size, *records[indices[1]])
    rejected(bytes(overlap))
    # CRC does not silently accept a mutated texture.
    corrupt = bytearray(blob)
    corrupt[-1] ^= 0x55
    rejected(bytes(corrupt))
    print("[MAPFLASHV2-HOST] PASS synthetic catalog=13 capacity/CRC/order/overlap/truncation")


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--pak", type=Path, help="local authoritative DoomRPG-ESP32.pak")
    p.add_argument("--check-crc", action="store_true")
    args = p.parse_args()
    self_test()
    if args.pak:
        result = inspect(args.pak.read_bytes(), verify_crc=args.check_crc)
        import json
        print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
