#!/usr/bin/env python3
"""Package Adobe-edited blank UAC button RGBA PNGs as CYD-compatible BMP V4.

Art editing happens in Adobe. This tool only encodes the finished pixels in
BITMAPV4HEADER/BGRA32/BI_BITFIELDS, preserving transparency and exact dimensions.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
from PIL import Image

ROOT = Path(__file__).resolve().parents[4]
ART = ROOT / "artwork/menu-uac/generic"
SD = ROOT / "sdroot/doomrpg/img"
SIZES = ((120, 55, 20), (240, 110, 40))

def make_bmp_v4(source: Path, width: int, height: int) -> bytes:
    with Image.open(source) as img:
        if img.format != "PNG" or img.mode != "RGBA" or img.size != (width, height):
            raise ValueError(f"{source}: expected PNG RGBA {width}x{height}, got {img.format} {img.mode} {img.size}")
        extrema = img.getchannel("A").getextrema()
        if extrema != (0, 255):
            raise ValueError(f"{source}: transparency lost, alpha extrema={extrema}")
        rgba = img.tobytes()
    pixels = bytearray(width * height * 4)
    stride = width * 4
    for y in range(height):
        src = (height - 1 - y) * stride
        dst = y * stride
        for x in range(width):
            s, d = src + 4 * x, dst + 4 * x
            pixels[d:d + 4] = bytes((rgba[s + 2], rgba[s + 1], rgba[s], rgba[s + 3]))
    size = 14 + 108 + len(pixels)
    file_header = b"BM" + struct.pack("<IHHI", size, 0, 0, 122)
    dib = struct.pack("<IiiHHIIiiII", 108, width, height, 1, 32, 3, len(pixels), 2835, 2835, 0, 0)
    dib += struct.pack("<IIIII", 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, 0x73524742)
    dib += bytes(36 + 12)
    assert len(file_header) == 14 and len(dib) == 108
    return file_header + dib + pixels

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def main() -> None:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    args = parser.parse_args()
    entries = []
    for tier, width, height in SIZES:
        source = ART / f"sources/menu_generic_{tier}.png"
        target = SD / f"menu_generic_{tier}.bmp"
        data = make_bmp_v4(source, width, height)
        assert len(data) == 122 + width * height * 4
        if args.write:
            target.write_bytes(data)
        elif not target.exists() or target.read_bytes() != data:
            raise AssertionError(f"{target}: BMP differs from Adobe RGBA master")
        entries += [(sha256(source.read_bytes()), f"sources/menu_generic_{tier}.png"),
                    (sha256(data), f"../../sdroot/doomrpg/img/menu_generic_{tier}.bmp")]
        print(f"[UAC-GENERIC] {tier} {width}x{height} V4 BGRA32 {len(data)} bytes sha256={sha256(data)}")
    manifest = "".join(f"{hash_}  {name}\n" for hash_, name in entries).encode()
    manifest_file = ART / "SHA256SUMS.txt"
    if args.write:
        manifest_file.write_bytes(manifest)
    elif not manifest_file.exists() or manifest_file.read_bytes() != manifest:
        raise AssertionError("SHA256SUMS.txt mismatch")
    print("[UAC-GENERIC] " + ("WRITE" if args.write else "CHECK") + " PASS")

if __name__ == "__main__":
    main()
