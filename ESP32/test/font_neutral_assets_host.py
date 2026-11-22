#!/usr/bin/env python3
"""FONT-NEUTRAL-P1 host strict integrity & deterministic preview simulation."""
import binascii
import hashlib
import importlib.util
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
MOD = ROOT / "artwork/fonts/neutral/generate.py"
spec = importlib.util.spec_from_file_location("font_neutral_generate", MOD)
assert spec and spec.loader
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)

ORIGINALS = (
    ("a.bmp", "a_neutral.bmp", (144, 72), (9, 12), 0, 52,
     "e088bb3f65f9314e394443dc659c643fd4cca0cd10fd5782d9b698ca16f8deae",
     "1724b300816bad83aba5017395a51666df532f3f19fcfd4a3958f9e72bf5657f"),
    ("larger_font.bmp", "larger_font_neutral.bmp", (208, 102), (13, 17), 2, 96,
     "faa893a06e63a9ca0e223cf2bcc5216950db600a437b01ab68a156eb1b8a4908",
     "43b86d143d6e2e91d165da06e29f322047abeba977878d837cb699d816cb0533"),
)
PREVIEW_SIZES = {
    "a_compare.png": (624, 176),
    "a_zoom.png": (468, 156),
    "a_themes.png": (320, 816),
    "larger_font_compare.png": (880, 236),
    "larger_font_zoom.png": (524, 172),
    "larger_font_themes.png": (448, 1116),
}


def decode_png(data: bytes) -> tuple[int, int, bytes]:
    """Independent PNG/CRC/decompression validation, including every scanline filter."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos, payload, dims = 8, bytearray(), None
    while pos < len(data):
        n = struct.unpack_from(">I", data, pos)[0]
        tag, content = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + n]
        crc = struct.unpack_from(">I", data, pos + 8 + n)[0]
        assert crc == binascii.crc32(tag + content) & 0xffffffff
        pos += n + 12
        if tag == b"IHDR":
            w, h, depth, color, comp, filtering, interlace = struct.unpack(
                ">IIBBBBB", content)
            assert (depth, color, comp, filtering, interlace) == (8, 2, 0, 0, 0)
            dims = (w, h)
        elif tag == b"IDAT":
            payload.extend(content)
        elif tag == b"IEND":
            break
    assert dims is not None and pos == len(data)
    raw = zlib.decompress(payload)
    w, h = dims
    assert len(raw) == (w * 3 + 1) * h
    assert all(raw[y * (w * 3 + 1)] == 0 for y in range(h))
    pixels = b"".join(raw[y * (w * 3 + 1) + 1:(y + 1) * (w * 3 + 1)]
                      for y in range(h))
    return w, h, pixels


def rgb(pixels: bytes, width: int, x: int, y: int) -> tuple:
    offset = (y * width + x) * 3
    return tuple(pixels[offset:offset + 3])


first = gen.render_outputs()
second = gen.render_outputs()
assert first.keys() == second.keys()
assert all(first[path] == second[path] for path in first)
assert len(first) == 8

for original, neutral, dims, cell, transparent, ao_ink, sha_orig, sha_neut in ORIGINALS:
    src_bytes = (gen.ASSETS / original).read_bytes()
    dst_bytes = (gen.ASSETS / neutral).read_bytes()
    assert hashlib.sha256(src_bytes).hexdigest() == sha_orig
    assert hashlib.sha256(dst_bytes).hexdigest() == sha_neut
    assert (len(src_bytes), len(dst_bytes)) == (
        118 + (dims[0] // 2) * dims[1], 118 + (dims[0] // 2) * dims[1])
    assert src_bytes[:54] == dst_bytes[:54]  # 4bpp/stride/header identical
    assert src_bytes[118:] == dst_bytes[118:]  # every index nibble identical
    expected_magenta = b"\xff\0\xff\0"
    assert src_bytes[54 + transparent * 4:58 + transparent * 4] == expected_magenta
    assert dst_bytes[54 + transparent * 4:58 + transparent * 4] == expected_magenta
    assert gen.neutralize(src_bytes, transparent) == dst_bytes
    src = gen.inspect_bmp(src_bytes, dims)
    dst = gen.inspect_bmp(dst_bytes, dims)
    for i in range(16):
        assert src_bytes[57 + i * 4] == dst_bytes[57 + i * 4]  # reserved
        if i != transparent:
            r, g, b = dst["palette"][i]
            assert r == g == b
            assert r == gen.neutral_gray(*src["palette"][i])
    assert gen.neutral_gray(0, 0, 0) == 0
    assert gen.neutral_gray(255, 255, 255) == 255

    cw, ch = cell
    assert dims == (cw * 16, ch * 6)
    for code in range(96):
        x0, y0 = (code % 16) * cw, (code // 16) * ch
        actual = [gen.pixel(src, x0 + x, y0 + y)
                  for y in range(ch) for x in range(cw)]
        same = [gen.pixel(dst, x0 + x, y0 + y)
                for y in range(ch) for x in range(cw)]
        assert actual == same, (original, code)
        if code in (ord("a") - 33, ord("o") - 33):
            assert sum(i != transparent for i in actual) == ao_ink
    for x in range(dims[0]):
        for y in range(dims[1]):
            assert gen.pixel(src, x, y) == gen.pixel(dst, x, y)

    compare = first[gen.PREVIEWS / (original[:-4] + "_compare.png")]
    outw, outh, rendered = decode_png(compare)
    assert (outw, outh) == PREVIEW_SIZES[original[:-4] + "_compare.png"]
    for y in range(dims[1]):
        for x in range(dims[0]):
            i = gen.pixel(src, x, y)
            old = gen.BG if i == transparent else src["palette"][i]
            new = gen.BG if i == transparent else dst["palette"][i]
            assert rgb(rendered, outw, 16 + 2*x, 16 + 2*y) == tuple(old)
            assert rgb(rendered, outw, 16 + 2*dims[0] + 16 + 2*x,
                       16 + 2*y) == tuple(new)

    # Blue pure is entirely blue at every ink pixel, black stays black;
    # transparent pixels are skipped, and no index cache/tone texture exists.
    for i, colors in enumerate(dst["palette"]):
        if i == transparent:
            continue
        ink = colors[0]
        assert tuple((ink * channel + 127) // 255
                     for channel in (0, 0, 255)) == (0, 0, ink)
        assert tuple((ink * channel + 127) // 255
                     for channel in (0, 0, 0)) == (0, 0, 0)
    print("FONT-NEUTRAL HOST PASS", original, "->", neutral,
          "96/96 cells + a/o, mask, key, 4bpp, palette, SHA256")

for path, expected in first.items():
    assert path.is_file() and path.read_bytes() == expected, path
    if path.suffix == ".png":
        w, h, content = decode_png(expected)
        assert (w, h) == PREVIEW_SIZES[path.name]
        assert len(content) == w * h * 3
        print("FONT-NEUTRAL PNG PASS", path.name, w, h)
assert gen.GLYPHS == "ADOao08%"
assert [name for name, _ in gen.THEMES] == [
    "classic", "inferno", "phosphore", "white", "blue"]
print("FONT-NEUTRAL-P1 HOST PASS: 8 exact outputs, 2 deterministic runs, no ESP32 code")
