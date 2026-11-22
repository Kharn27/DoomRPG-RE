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

HISTORICAL_SHA = {
    "a.bmp": "e088bb3f65f9314e394443dc659c643fd4cca0cd10fd5782d9b698ca16f8deae",
    "larger_font.bmp": "faa893a06e63a9ca0e223cf2bcc5216950db600a437b01ab68a156eb1b8a4908",
}
NEUTRAL_SHA = {
    "a_neutral.bmp": "1724b300816bad83aba5017395a51666df532f3f19fcfd4a3958f9e72bf5657f",
    "larger_font_neutral.bmp": "cb974827c206ab80342af9af91680c2c871a69cc855d36ed0d99f39e5e1a879c",
}
LARGE_COLOR_SHA = "a3724b44ff987010abfd3614c6ddc3ed0cedd5b7a26f51a94edc9b0446b60a6f"
PREVIEW_SIZES = {
    "a_compare.png": (928, 200),
    "a_zoom.png": (468, 156),
    "a_themes.png": (320, 816),
    "larger_font_compare.png": (1312, 260),
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
assert len(first) == 9

for name, expected in HISTORICAL_SHA.items():
    actual = (gen.ASSETS / name).read_bytes()
    assert hashlib.sha256(actual).hexdigest() == expected, name
    print("FONT-NEUTRAL HISTORICAL PASS", name, expected)

small_data = (gen.ASSETS / "a.bmp").read_bytes()
small_sheet = gen.inspect_bmp(small_data, (144, 72))
large_color = gen.LARGE_COLOR_PROOF.read_bytes()
assert hashlib.sha256(large_color).hexdigest() == LARGE_COLOR_SHA
assert large_color == gen.scale_small_to_large(small_data)
color_large_sheet = gen.inspect_bmp(large_color, (208, 102))
assert color_large_sheet["palette"][2] == (255, 0, 255)
assert color_large_sheet["palette"][0] == small_sheet["palette"][2]
assert color_large_sheet["palette"][2] == small_sheet["palette"][0]
# Independently check all 96*13*17 pixels: exact cell mapping, 11x15
# inner area, safe 1px border, 0/2 transparency permutation.
for y in range(102):
    for x in range(208):
        cx, cy = x % 13, y % 17
        expected = 2
        if 1 <= cx <= 11 and 1 <= cy <= 15:
            sx = (2 * (cx - 1) + 1) * 9 // 22
            sy = (2 * (cy - 1) + 1) * 12 // 30
            old = gen.pixel(small_sheet, (x // 13) * 9 + sx,
                            (y // 17) * 12 + sy)
            expected = 2 if old == 0 else (0 if old == 2 else old)
        assert gen.pixel(color_large_sheet, x, y) == expected, (x, y)

for original, neutral, dims, cell, transparent in (
    ("a.bmp", "a_neutral.bmp", (144, 72), (9, 12), 0),
    ("larger_font.bmp", "larger_font_neutral.bmp", (208, 102), (13, 17), 2),
):
    historical_bytes = (gen.ASSETS / original).read_bytes()
    src_bytes = historical_bytes if original == "a.bmp" else large_color
    dst_bytes = (gen.ASSETS / neutral).read_bytes()
    assert hashlib.sha256(dst_bytes).hexdigest() == NEUTRAL_SHA[neutral]
    assert len(dst_bytes) == 118 + (dims[0] // 2) * dims[1]
    assert src_bytes[:54] == dst_bytes[:54]
    assert src_bytes[118:] == dst_bytes[118:]  # only palette changes!
    assert gen.neutralize(src_bytes, transparent) == dst_bytes
    src = gen.inspect_bmp(src_bytes, dims)
    dst = gen.inspect_bmp(dst_bytes, dims)
    for index in range(16):
        assert src_bytes[57 + index * 4] == dst_bytes[57 + index * 4]
        if index != transparent:
            r, g, b = dst["palette"][index]
            assert r == g == b == gen.neutral_gray(*src["palette"][index])
    assert src["palette"][transparent] == dst["palette"][transparent] == (255, 0, 255)
    cw, ch = cell
    assert dims == (cw * 16, ch * 6)
    for code in range(96):
        x0, y0 = code % 16 * cw, code // 16 * ch
        for y in range(ch):
            for x in range(cw):
                xg, yg = x0 + x, y0 + y
                assert gen.pixel(src, xg, yg) == gen.pixel(dst, xg, yg)
    if original == "larger_font.bmp":
        counts = []
        for glyph in "ao":
            idx = ord(glyph) - 33
            x0, y0 = (idx % 16) * cw, (idx // 16) * ch
            ink = sum(gen.pixel(dst, x0 + x, y0 + y) != transparent
                      for y in range(ch) for x in range(cw))
            counts.append(ink)
        assert counts == [80, 81], counts
        # New style intentionally differs from the historical large atlas.
        assert src_bytes != historical_bytes

    comparison = first[gen.PREVIEWS / (original[:-4] + "_compare.png")]
    width, height, rendered = decode_png(comparison)
    assert (width, height) == PREVIEW_SIZES[original[:-4] + "_compare.png"]
    for y in range(dims[1]):
        for x in range(dims[0]):
            i, j = gen.pixel(src, x, y), gen.pixel(dst, x, y)
            color = gen.BG if i == transparent else src["palette"][i]
            grayscale = gen.BG if j == transparent else dst["palette"][j]
            tint = gen.theme_color(gen.THEMES[2][1], y % ch, ch)
            tinted = (gen.BG if j == transparent else tuple(
                (grayscale[0] * channel + 127) // 255 for channel in tint))
            py = gen.PADDING + gen.LABEL_HEIGHT + 2 * y
            for panel, rgb_value in enumerate((color, grayscale, tinted)):
                px = gen.PADDING + panel * (dims[0] * 2 + gen.GAP) + 2 * x
                assert rgb(rendered, width, px, py) == tuple(rgb_value)
    print("FONT-NEUTRAL HOST PASS", original, "->", neutral,
          "96 cells 4bpp, transparent index, exact palette, source proof")

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
print("FONT-NEUTRAL-P1 HOST PASS: 9 exact outputs, 2 deterministic runs, large glyphs derived from small source, no ESP32 code")
