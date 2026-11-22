#!/usr/bin/env python3
"""WALL-SD-P1: audit exact C4 and restored original E8 SD artwork.

The CYD hardware log 261122-175956 had HEAP-POST-REJECT and visibly
nontransparent (255,4,255) pixels. This host oracle guards both regressions.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ITEMS = (
    ("c", (192, 128), 12406, "b9da41fa33449c8ffeebec5ce0662325c754d280", 4, 16, 118, 12288, 350.0),
    ("e", (128, 128), 17462, "16fe8cd8ddff2b18e040d69134bfb355fe2bb32b", 8, 256, 1078, 16384, 300.0),
)


def git_blob(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()


def rgb565(rgb: tuple[int, ...]) -> tuple[int, int, int]:
    r, g, b = rgb[:3]
    r5, g6, b5 = r >> 3, g >> 2, b >> 3
    return (r5 << 3 | r5 >> 2, g6 << 2 | g6 >> 4, b5 << 3 | b5 >> 2)


def is_pink_fringe(rgb: tuple[int, int, int]) -> bool:
    r, g, b = rgb
    return (r >= 156 and b >= 140 and g <= 170 and abs(r - b) < 90
            and r - g >= 40 and b - g >= 40)


def verify(name: str, size: tuple[int, int], byte_count: int,
           sha: str, bpp: int, palette_size: int, offset: int,
           image_bytes: int, error_limit: float) -> None:
    png = Image.open(ROOT / "artwork" / "wallpaper" / f"{name}.png")
    bmp_path = ROOT / "sdroot" / "doomrpg" / "img" / f"{name}.bmp"
    data = bmp_path.read_bytes()
    assert len(data) == byte_count
    assert git_blob(data) == sha, (name, "delivered BMP blob changed")
    assert data[:2] == b"BM"
    assert struct.unpack_from("<I", data, 2)[0] == byte_count
    assert struct.unpack_from("<I", data, 10)[0] == offset
    assert struct.unpack_from("<IiiHHII", data, 14)[:6] == (
        40, size[0], size[1], 1, bpp, 0
    ), name
    assert struct.unpack_from("<I", data, 34)[0] == image_bytes
    assert struct.unpack_from("<I", data, 46)[0] == palette_size
    assert data[offset:] and len(data[offset:]) == image_bytes

    bmp = Image.open(bmp_path)
    png.load()
    bmp.load()
    assert bmp.size == png.size == size
    assert bmp.mode == "P" and png.mode in ("RGB", "RGBA")
    original = list(png.convert("RGB").getdata())
    quantized = list(bmp.convert("RGB").getdata())
    indices = list(bmp.getdata())
    key_count, fringe_count, error = 0, 0, 0
    for src, dst, index in zip(original, quantized, indices):
        if name == "e":
            if rgb565(src) == (255, 0, 255):
                assert index == 0, ("original magenta visible", src, dst)
            if index == 0:
                key_count += 1
                assert rgb565(dst) == (255, 0, 255), ("invalid transparent palette")
                continue
            if is_pink_fringe(rgb565(dst)):
                fringe_count += 1  # original near-magenta preserved for A/B
        else:
            assert index < 16
        a = rgb565(src)
        b = rgb565(dst)
        error += 3 * (a[0] - b[0]) ** 2 + 6 * (a[1] - b[1]) ** 2 + 2 * (a[2] - b[2]) ** 2
    assert key_count == (7527 if name == "e" else 0), (name, key_count)
    if name == "e":
        assert fringe_count >= 1000, fringe_count
    used = len(set(indices))
    assert used >= (14 if name == "c" else 140), (name, used)
    weighted_error = error / (size[0] * size[1])
    assert weighted_error < error_limit, (name, weighted_error, error_limit)
    print(f"WALL-SD ASSET PASS: {name}.bmp {size[0]}x{size[1]} "
          f"indexed={bpp} colors={used} bytes={len(data)} "
          f"transparent={key_count} fringe_preserved={fringe_count} rgb565_weighted_error={weighted_error:.2f}")


def verify_audio_memory_recovery() -> None:
    pack = (ROOT / "ESP32/src/esp_asset_pack.cpp").read_text()
    guard = (ROOT / "ESP32/src/esp_audio_entrance_heap_guard.h").read_text()
    assert "kResidentRangeCapacityBytes = 15U * 1024U" in pack
    assert "kResidentRangeEntryCapacity = 256U" in pack
    assert "ESP_AUDIO_MUSIC_POST_MIN_HEAP8 = 47104U" in guard
    assert "ESP_AUDIO_MUSIC_POST_MIN_LARGEST8 = 16384U" in guard
    # Empirical failure readings, NOT a claim about the next firmware's heap.
    assert 45764 + (24576 + 1024 - 12288 - 64) > 47104
    # Reducing cache capacity/metadata saves 4096 + 32*12 in its owner.
    assert 44820 + (19 - 15) * 1024 + (288 - 256) * 12 > 47104
    print("WALL-SD MEMORY PLAN PASS: audio floors unchanged; "
          "intro C4 and gameplay smaller L1 projected above post-ring floor")


if __name__ == "__main__":
    for item in ITEMS:
        verify(*item)
    verify_audio_memory_recovery()
