#!/usr/bin/env python3
"""R-A2a PAK-v2 wall-span provenance: real packed bytes vs actual legacy
physical adjacency. The original DoomRPG-ESP32.pak is user-supplied and NEVER
checked into Git; CI uses a deliberately synthetic mini-PAK, clearly labeled.
Runs actual Render_SpanMode0 and sampleWallSpan extracted from source in host C.
No production changes, SD measurement, or complete-frame oracle is claimed.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import zlib

from native_wall_boundary_inline_host import ROOT, SOURCE, HARNESS, extract

PAK_MAGIC = b"DRPGESP2"
FIXTURE_MAGIC = b"DRPGR2A2"
HEADER = struct.Struct("<8sIIII")
ENTRY = struct.Struct("<IIIII")
MAP_HEADER = struct.Struct("<IIII")
PAIR = struct.Struct("<ii")
PALETTE_SIZE = 16
PACKED_BYTES = 2048
MAX_PAK_BYTES = 32 * 1024 * 1024
# These two *provenance witnesses* come from the user's real classic CYD Serial
# capture platformio-device-monitor-261115-103040.log. They are not invented
# measurements nor a substitute for an asset-backed parity result.
WITNESSES = (
    (15, 40, 20480, 68, 32768, 0x11),
    (49, 112, 65536, 116, 69632, 0xA7),
)


class FixtureError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise FixtureError(message)


def fnv_name(name: str) -> int:
    h = 2166136261
    for octet in name.encode("ascii"):
        h = ((h ^ octet) * 16777619) & 0xFFFFFFFF
    return h


def fnv_pixels(packed: bytes, colors: tuple[int, ...]) -> int:
    h = 2166136261
    require(len(packed) == PACKED_BYTES and len(colors) == 16,
            "invalid full-texture hash arguments")
    for b in packed:
        for shift in (0, 4):
            pixel = colors[(b >> shift) & 15]
            for octet in (pixel & 255, pixel >> 8):
                h = ((h ^ octet) * 16777619) & 0xFFFFFFFF
    return h


def rgb_from_bgr565(raw: int) -> int:
    return ((raw & 0x1F) << 11) | (raw & 0x07E0) | ((raw & 0xF800) >> 11)


class Pak:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.length = path.stat().st_size
        require(HEADER.size <= self.length <= MAX_PAK_BYTES, "PAK size invalid")
        with path.open("rb") as f:
            raw = f.read(HEADER.size)
            magic, version, count, index_start, data_start = HEADER.unpack(raw)
            require(magic == PAK_MAGIC and version == 2, "not a native PAK v2")
            require(0 < count <= 4096 and index_start == HEADER.size and
                    data_start == HEADER.size + count * ENTRY.size and
                    data_start <= self.length, "PAK header/index bounds invalid")
            self.entries: dict[int, tuple[int, int, int, int]] = {}
            prior = 0
            used: list[tuple[int, int]] = []
            for _ in range(count):
                record = f.read(ENTRY.size)
                require(len(record) == ENTRY.size, "short PAK index record")
                name_hash, off, size, crc, flags = ENTRY.unpack(record)
                require(name_hash > prior and flags in (0, 1),
                        "PAK hashes unsorted/duplicated or flags invalid")
                require(data_start <= off <= self.length and
                        size <= self.length - off, "PAK entry outside data")
                if flags == 1:
                    require(size == 0, "PAK directory with data")
                else:
                    used.append((off, off + size))
                self.entries[name_hash] = (off, size, crc, flags)
                prior = name_hash
            used.sort()
            require(all(used[i][0] >= used[i - 1][1]
                        for i in range(1, len(used))),
                    "PAK payload entries overlap")

    def entry(self, name: str) -> tuple[int, int, int, int]:
        require(name == name.lower() and "/" not in name, "bad PAK lookup name")
        record = self.entries.get(fnv_name(name))
        require(record is not None and record[3] == 0,
                "missing PAK file: " + name)
        return record

    def read(self, name: str, offset: int, length: int) -> bytes:
        start, size, _crc, _flags = self.entry(name)
        require(0 <= offset <= size and 0 <= length <= size - offset,
                f"PAK bounded read failed: {name}@{offset}+{length}")
        with self.path.open("rb") as f:
            f.seek(start + offset)
            content = f.read(length)
        require(len(content) == length, "truncated PAK resource read")
        return content

    def verify_crc(self, name: str) -> None:
        start, size, expected, _flags = self.entry(name)
        actual = 0
        with self.path.open("rb") as f:
            f.seek(start)
            for _ in range((size + 4095) // 4096):
                chunk = f.read(min(4096, size))
                require(bool(chunk), f"short CRC stream: {name}")
                actual = zlib.crc32(chunk, actual)
                size -= len(chunk)
        require((actual & 0xFFFFFFFF) == expected,
                f"PAK resource CRC differs: {name}")


def pack_fixture(path: Path) -> tuple[bytes, list[str]]:
    pak = Pak(path)
    for name in ("mappings.bin", "wtexels.bin", "palettes.bin"):
        pak.verify_crc(name)
    mapping_size = pak.entry("mappings.bin")[1]
    palette_size = pak.entry("palettes.bin")[1]
    wall_size = pak.entry("wtexels.bin")[1]
    require(mapping_size >= MAP_HEADER.size and palette_size >= 4 + 32 and
            wall_size >= 4 + PACKED_BYTES, "required PAK assets undersized")
    pairs, bit_pairs, texture_ids, sprite_ids = MAP_HEADER.unpack(
        pak.read("mappings.bin", 0, 16))
    require(0 < pairs <= 4096 and 0 < bit_pairs <= 4096 and
            0 < texture_ids <= 4096 and 0 <= sprite_ids <= 4096,
            "mappings header exceeds native limits")
    id_base = 16 + 8 * (pairs + bit_pairs)
    require(mapping_size == id_base + 2 * (texture_ids + sprite_ids),
            "mappings.bin size inconsistent with four counts")
    (palette_bytes,) = struct.unpack("<I", pak.read("palettes.bin", 0, 4))
    (wall_bytes,) = struct.unpack("<I", pak.read("wtexels.bin", 0, 4))
    require(palette_bytes % 2 == 0 and 4 + palette_bytes == palette_size,
            "palettes.bin header inconsistent")
    require(4 + wall_bytes == wall_size, "wtexels.bin header inconsistent")
    palette_count = palette_bytes // 2
    out = bytearray(FIXTURE_MAGIC + struct.pack("<I", len(WITNESSES)))
    descriptions: list[str] = []

    for logical, actual, source, successor, successor_source, observed in WITNESSES:
        require(logical < texture_ids and actual < pairs and successor < pairs,
                "real serial witness not in mappings domain")
        (base_actual,) = struct.unpack(
            "<H", pak.read("mappings.bin", id_base + 2 * logical, 2))
        require(base_actual <= actual < base_actual + 4,
                "actual texture not inside mapped four phases")
        src_bytes = pak.read("mappings.bin", 16 + actual * 8, 8)
        next_bytes = pak.read("mappings.bin", 16 + successor * 8, 8)
        src_offset, pal_offset = PAIR.unpack(src_bytes)
        next_offset, _ = PAIR.unpack(next_bytes)
        require(src_offset == source and next_offset == successor_source,
                "real Serial source/successor mapping differs from PAK")
        require(src_offset >= 0 and (src_offset & 1) == 0 and
                next_offset > src_offset and (next_offset & 1) == 0 and
                pal_offset >= 0 and pal_offset + 16 <= palette_count,
                "wall source/palette mapping invalid")
        read_pos = 4 + src_offset // 2
        next_pos = 4 + next_offset // 2
        require(read_pos + PACKED_BYTES + 1 <= wall_size and
                next_pos + PACKED_BYTES <= wall_size,
                "wall cache/physical/guard byte exceeds wtexels resource")
        wall = pak.read("wtexels.bin", read_pos, PACKED_BYTES)
        physical = pak.read("wtexels.bin", read_pos + PACKED_BYTES, 1)[0]
        mapped = pak.read("wtexels.bin", next_pos, 1)[0]
        require(mapped == observed, "real Serial guard byte differs from PAK")
        raw_palette = struct.unpack(
            "<16H", pak.read("palettes.bin", 4 + 2 * pal_offset, 32))
        rgb_palette = tuple(rgb_from_bgr565(word) for word in raw_palette)
        pixel_fnv = fnv_pixels(wall, rgb_palette)
        out.extend(struct.pack("<4H2I", logical, actual, successor, 0,
                               src_offset, next_offset))
        out.extend(struct.pack("<16H", *rgb_palette))
        out.extend(wall)
        out.extend(bytes((physical, mapped)))
        out.extend(struct.pack("<I", pixel_fnv))
        descriptions.append(
            f"logical={logical} actual={actual} source={source} "
            f"successorActual={successor} successorSource={next_offset} "
            f"physical={physical:02x} mapped={mapped:02x} "
            f"physicalAdjacent={source + 4096 == next_offset} "
            f"pixelFNV={pixel_fnv:08x}"
        )
    return bytes(out), descriptions


def build_synthetic_pack(path: Path, diverge: bool = False,
                         wrong_guard: bool = False,
                         wrong_source: bool = False) -> None:
    """CI fixture only; never label these bytes as original Doom RPG art."""
    pairs, bit_pairs, ids, sprite_ids = 120, 1, 64, 1
    mapping = bytearray(16 + 8 * (pairs + bit_pairs) + 2 * (ids + sprite_ids))
    struct.pack_into("<4I", mapping, 0, pairs, bit_pairs, ids, sprite_ids)
    id_base = 16 + 8 * (pairs + bit_pairs)
    struct.pack_into("<H", mapping, id_base + 15 * 2, 40)
    struct.pack_into("<H", mapping, id_base + 49 * 2, 112)
    for actual, source, palette in (
        (40, 24576 if wrong_source else 20480, 0),
        (68, 32768, 0),
        (112, 65536, 16),
        (116, 69632, 16),
    ):
        struct.pack_into("<ii", mapping, 16 + actual * 8, source, palette)
    palette_words = tuple((i * 1733 + 19) & 65535 for i in range(32))
    palettes = struct.pack("<I32H", 64, *palette_words)
    texels = bytearray(4 + 37000)
    struct.pack_into("<I", texels, 0, len(texels) - 4)
    for source, salt in ((20480, 7), (65536, 17)):
        off = 4 + source // 2
        for i in range(PACKED_BYTES):
            texels[off + i] = (i * 73 + salt) & 255
    texels[4 + 32768 // 2] = 0x12 if wrong_guard else 0x11
    texels[4 + 69632 // 2] = 0xA7
    texels[4 + 20480 // 2 + PACKED_BYTES] = 0x99 if diverge else 0x11
    resources = {
        "mappings.bin": bytes(mapping),
        "palettes.bin": palettes,
        "wtexels.bin": bytes(texels),
    }
    data_start = HEADER.size + len(resources) * ENTRY.size
    payload = bytearray()
    index = []
    for name in sorted(resources):
        content = resources[name]
        index.append((fnv_name(name), data_start + len(payload),
                      len(content), zlib.crc32(content) & 0xFFFFFFFF, 0))
        payload.extend(content)
    index.sort()
    path.write_bytes(
        HEADER.pack(PAK_MAGIC, 2, len(index), HEADER.size, data_start) +
        b"".join(ENTRY.pack(*row) for row in index) + bytes(payload)
    )


def compile_host(binary: Path, generated_c: Path) -> None:
    native = SOURCE.read_text()
    legacy = (ROOT / "src/Render.c").read_text()
    code = "\n\n".join([
        extract(native, "static uint16_t readLe16("),
        extract(native, "static uint32_t readLe32("),
        extract(native, "static int prepareLegacyWallGuard("),
        extract(native, "static int sampleWallSpan("),
        extract(legacy, "void Render_SpanMode0("),
    ])
    base = HARNESS.read_text()
    require(base.count("/* PRODUCTION_FUNCTIONS */") == 1,
            "host harness no longer has exact C extraction slot")
    require("static int runPackFixture(" in base and
            "legacyAtlas[65536]" in base,
            "R-A2a real pack fixture handler missing")
    generated_c.write_text(base.replace("/* PRODUCTION_FUNCTIONS */", code))
    cc = shlex.split(os.environ.get("CC", "cc"))
    subprocess.run(cc + ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                         "-fwrapv", str(generated_c), "-o", str(binary)],
                   check=True, cwd=ROOT)


def run_host(binary: Path, fixture: bytes, tmp: Path,
             expected_returncode: int) -> str:
    f = tmp / "wall-ra2a.fixture"
    f.write_bytes(fixture)
    result = subprocess.run([str(binary), "--fixture", str(f)],
                            capture_output=True, text=True, cwd=ROOT)
    require(result.returncode == expected_returncode,
            f"host C returned {result.returncode}, expected "
            f"{expected_returncode}: {result.stdout}\n{result.stderr}")
    return result.stdout + result.stderr


def self_test(binary: Path, tmp: Path) -> None:
    pack_path = tmp / "synthetic.pak"
    build_synthetic_pack(pack_path)
    fixture, info = pack_fixture(pack_path)
    require(len(fixture) == 12 + 2 * (16 + 32 + 2048 + 2 + 4),
            "unexpected bounded host fixture size")
    require(all("physicalAdjacent=" in line for line in info),
            "witness summary invalid")
    good = run_host(binary, fixture, tmp, 0)
    require("PASS RA2 PACK" in good and good.count("boundaryParity=EXACT") == 2,
            "real C runner did not prove two matched synthetic boundaries")
    build_synthetic_pack(pack_path, diverge=True)
    fixture, info = pack_fixture(pack_path)
    require("physical=99 mapped=11" in info[0],
            "divergent physical byte was not kept independent")
    divergent = run_host(binary, fixture, tmp, 2)
    require("boundaryParity=DIFF" in divergent and
            "legacy physical adjacency != mapped guard" in divergent,
            "mismatched legacy byte was not rejected by actual C functions")
    build_synthetic_pack(pack_path, wrong_guard=True)
    try:
        pack_fixture(pack_path)
        raise AssertionError("wrong logged successor byte accepted")
    except FixtureError as e:
        require("guard byte differs" in str(e), "wrong failure for guard")
    build_synthetic_pack(pack_path, wrong_source=True)
    try:
        pack_fixture(pack_path)
        raise AssertionError("wrong source offset accepted")
    except FixtureError as e:
        require("mapping differs" in str(e), "wrong failure for mapping")
    build_synthetic_pack(pack_path)
    corrupted = bytearray(pack_path.read_bytes())
    corrupted[-1] ^= 0x40
    pack_path.write_bytes(corrupted)
    try:
        pack_fixture(pack_path)
        raise AssertionError("resource CRC corruption accepted")
    except FixtureError as e:
        require("CRC differs" in str(e), "wrong failure for CRC")
    corrupted[0] = 0xFF
    pack_path.write_bytes(corrupted)
    try:
        pack_fixture(pack_path)
        raise AssertionError("PAK magic corruption accepted")
    except FixtureError as e:
        require("not a native PAK v2" in str(e), "wrong failure for header")
    print("PASS RA2a synthetic pack-v2 index+CRC+mapping+palette+"
          "real-RenderSpan parity=8192 nibbles boundary=4 "
          "physical-mismatch-fail-closed corrupt-pack-fail-closed; "
          "original-PAK=not-present-in-CI")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pak", type=Path,
                        help="user-owned DoomRPG-ESP32.pak (optional; CI has no original PAK)")
    args = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix="doomrpg-ra2-wall-") as d:
            tmp = Path(d)
            binary = tmp / "wall-ra2"
            compile_host(binary, tmp / "wall_ra2_generated.c")
            self_test(binary, tmp)
            if args.pak is not None:
                real_path = args.pak.expanduser().resolve()
                require(real_path.is_file(), f"original PAK not found: {real_path}")
                fixture, info = pack_fixture(real_path)
                for line in info:
                    print("[RA2 ORIGINAL PAK]", line)
                result = run_host(binary, fixture, tmp, 0)
                print(result, end="")
                print("PASS RA2a original-PAK TWO observed boundaries; "
                      "not full-frame/SD-timing parity")
    except (FixtureError, OSError, subprocess.CalledProcessError,
            struct.error, AssertionError) as e:
        print("[RA2] FAIL", e)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
