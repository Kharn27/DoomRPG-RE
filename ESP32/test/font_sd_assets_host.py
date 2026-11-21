#!/usr/bin/env python3
"""Assert shipping SD font overrides are usable without a BMP24 decoder."""
from pathlib import Path
import hashlib
import struct

root = Path(__file__).resolve().parents[2]
assets = root / "sdroot" / "doomrpg" / "img"

def inspect(name, source, width, height):
    data = (assets / name).read_bytes()
    reference = (assets / source).read_bytes()
    assert data == reference, f"{name}: not an exact copy of {source}"
    assert len(data) >= 54 and data[:2] == b"BM", f"{name}: missing BMP header"
    file_size, pixel_offset, dib_size = (struct.unpack_from("<I", data, i)[0]
                                         for i in (2, 10, 14))
    w, h, planes, bpp, compression, palette_size = (
        struct.unpack_from("<iiHHI", data, 18) +
        (struct.unpack_from("<I", data, 46)[0],)
    )
    assert (file_size == len(data) and dib_size >= 40 and
            (w, h) == (width, height) and planes == 1 and
            bpp == 4 and compression == 0 and palette_size == 16), (
            f"{name}: invalid 4-bpp atlas metadata"
    )
    row = ((width * bpp + 31) // 32) * 4
    assert pixel_offset >= 14 + dib_size + palette_size * 4
    assert pixel_offset + row * height <= len(data)
    # The magenta colorkey can occupy any palette slot (standard=0, large=2).
    palette = [data[54 + 4 * i: 58 + 4 * i] for i in range(palette_size)]
    assert bytes((255, 0, 255, 0)) in palette, f"{name}: no magenta colorkey"
    print(f"FONT-SD ASSET PASS {name}: {w}x{h} {bpp}-bpp "
          f"size={len(data)} sha256={hashlib.sha256(data).hexdigest()}")

inspect("a.bmp", "a_doom.bmp", 144, 72)
inspect("larger_font.bmp", "larger_font_doom.bmp", 208, 102)
# The original 24-bit variant is kept only under its explicitly named alias.
reference_24 = (assets / "larger_font_doom_24bit.bmp").read_bytes()
assert struct.unpack_from("<H", reference_24, 28)[0] == 24
print("FONT-SD ASSET PASS: runtime names are indexed; 24-bit reference isolated")
