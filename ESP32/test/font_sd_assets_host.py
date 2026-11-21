#!/usr/bin/env python3
"""Check SD-ready Doom glyph sheets and isolate the lowercase a/o repairs."""
from pathlib import Path
import hashlib
import struct

root = Path(__file__).resolve().parents[2]
assets = root / "sdroot" / "doomrpg" / "img"

EXPECTED = {
    "a.bmp": {
        "alias": "a_doom.bmp",
        "size": (144, 72),
        "cell": (9, 12),
        "magenta": 0,
        "sha256": "e088bb3f65f9314e394443dc659c643fd4cca0cd10fd5782d9b698ca16f8deae",
        "other_94": "0c032372595501a986fdc107a4560284aa67511327e89313d57ba764f426adfa",
        "ink": 52,
    },
    "larger_font.bmp": {
        "alias": "larger_font_doom.bmp",
        "size": (208, 102),
        "cell": (13, 17),
        "magenta": 2,
        "sha256": "faa893a06e63a9ca0e223cf2bcc5216950db600a437b01ab68a156eb1b8a4908",
        "other_94": "ed651f3eb94098ee5f604de3841f8536365252b135aefbfc8564365d6b93d819",
        "ink": 96,
    },
}


def inspect(name, spec):
    data = (assets / name).read_bytes()
    assert data == (assets / spec["alias"]).read_bytes(), name
    assert data[:2] == b"BM", name
    file_size, pixel_offset, dib_size = (
        struct.unpack_from("<I", data, pos)[0] for pos in (2, 10, 14)
    )
    w, h, planes, bpp, compression = struct.unpack_from("<iiHHI", data, 18)
    palette_size = struct.unpack_from("<I", data, 46)[0]
    assert (file_size == len(data) and dib_size >= 40
            and (w, h) == spec["size"] and planes == 1
            and bpp == 4 and compression == 0 and palette_size == 16), name
    pitch = ((w * bpp + 31) // 32) * 4
    assert pixel_offset >= 14 + dib_size + palette_size * 4
    assert pixel_offset + pitch * h <= len(data)
    palette = [data[54 + 4*i:58 + 4*i] for i in range(16)]
    assert palette[spec["magenta"]] == bytes((255, 0, 255, 0)), name
    assert hashlib.sha256(data).hexdigest() == spec["sha256"], name

    def pixel(x, y):
        p = data[pixel_offset + (h - 1 - y) * pitch + (x >> 1)]
        return (p >> 4) if (x & 1) == 0 else (p & 15)

    cw, ch = spec["cell"]
    unchanged = bytearray()
    for code in range(96):
        if code in (ord("a") - 33, ord("o") - 33):
            continue
        x0, y0 = code % 16 * cw, code // 16 * ch
        for y in range(ch):
            for x in range(cw):
                unchanged.append(pixel(x0 + x, y0 + y))
    assert hashlib.sha256(unchanged).hexdigest() == spec["other_94"], name

    for letter in "ao":
        code = ord(letter) - 33
        x0, y0 = code % 16 * cw, code // 16 * ch
        ink = [(x, y) for y in range(ch) for x in range(cw)
               if pixel(x0 + x, y0 + y) != spec["magenta"]]
        assert len(ink) == spec["ink"], (name, letter, len(ink))
        assert sum(y >= ch // 2 for _, y in ink) > 20, (name, letter)
        assert min(y for _, y in ink) >= 2 and max(y for _, y in ink) < ch - 2
    print(f"FONT-SD ASSET PASS {name}: {w}x{h} 4-bpp, "
          f"{len(data)} B, 94 other glyphs unchanged, a/o complete")


for file_name, settings in EXPECTED.items():
    inspect(file_name, settings)

# RGB24 was a reference image, never runtime-capable, now removed from SD distribution.
assert not (assets / "larger_font_doom_24bit.bmp").exists()
print("FONT-SD ASSET PASS: only indexed fonts distributed; BMP24 removed")
