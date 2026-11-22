#!/usr/bin/env python3
"""WALL-SD-P1: audit the real artwork and delivered uncompressed BMP8 files.

No firmware or PAK patch is involved; the image importer uses the existing
bounded indexed palette/row access and the immutable PAK as fallback.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ITEMS = (
    ("c", (192, 128), 25654, "7c2749ce1bb75c0d5df0882351c82bf11ee8fa83", 30.0),
    ("e", (128, 128), 17462, "16fe8cd8ddff2b18e040d69134bfb355fe2bb32b", 300.0),
)


def git_blob(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\\0" + data).hexdigest()


def rgb565(rgb: tuple[int, ...]) -> tuple[int, int, int]:
    r, g, b = rgb[:3]
    r5, g6, b5 = r >> 3, g >> 2, b >> 3
    return (r5 << 3 | r5 >> 2, g6 << 2 | g6 >> 4, b5 << 3 | b5 >> 2)


def verify(name: str, size: tuple[int, int], byte_count: int,
           sha: str, error_limit: float) -> None:
    png_path = ROOT / "artwork" / "wallpaper" / f"{name}.png"
    bmp_path = ROOT / "sdroot" / "doomrpg" / "img" / f"{name}.bmp"
    data = bmp_path.read_bytes()
    assert len(data) == byte_count, (name, len(data), byte_count)
    assert git_blob(data) == sha, (name, "delivery BMP hash mismatch")
    assert data[:2] == b"BM", name
    assert struct.unpack_from("<I", data, 2)[0] == byte_count
    assert struct.unpack_from("<I", data, 10)[0] == 1078
    assert struct.unpack_from("<IiiHHII", data, 14)[:6] == (
        40, size[0], size[1], 1, 8, 0
    ), name
    assert struct.unpack_from("<I", data, 34)[0] == size[0] * size[1]
    assert struct.unpack_from("<I", data, 46)[0] == 256
    assert data[54:58] == bytes((255, 0, 255, 0)), name

    src = Image.open(png_path)
    src.load()
    bmp = Image.open(bmp_path)
    bmp.load()
    assert src.size == bmp.size == size
    assert src.mode in ("RGB", "RGBA")
    assert bmp.mode == "P", (name, bmp.mode)
    bmp_rgb = bmp.convert("RGB")
    rgb = src.convert("RGB")
    pixels_src = list(rgb.getdata())
    pixels_bmp = list(bmp_rgb.getdata())
    indices = list(bmp.getdata())
    mismatch = 0
    error = 0
    magenta_count = 0
    for a, b, index in zip(pixels_src, pixels_bmp, indices):
        opaque_key = rgb565(a) == (255, 0, 255)
        if opaque_key:
            magenta_count += 1
            if index != 0:
                mismatch += 1
        elif index == 0:
            mismatch += 1
        if not opaque_key:
            ar, ag, ab = rgb565(a)
            br, bg, bb = rgb565(b)
            error += 3 * (ar - br) ** 2 + 6 * (ag - bg) ** 2 + 2 * (ab - bb) ** 2
    assert mismatch == 0, (name, "colorkey mismatch", mismatch)
    assert magenta_count == (7527 if name == "e" else 0), (name, magenta_count)
    weighted_error = error / (size[0] * size[1])
    assert weighted_error < error_limit, (name, weighted_error, error_limit)
    assert (max(indices) > 15) and len(set(indices)) > 200, name
    print(f"WALL-SD ASSET PASS: {name}.bmp {size[0]}x{size[1]} 8bpp "
          f"bytes={byte_count} colors={len(set(indices))} "
          f"magenta={magenta_count} rgb565_weighted_error={weighted_error:.3f}")


if __name__ == "__main__":
    for item in ITEMS:
        verify(*item)
