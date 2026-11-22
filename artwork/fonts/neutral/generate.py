#!/usr/bin/env python3
"""FONT-NEUTRAL-P1: deterministic 4bpp grayscale BMP assets and PNG proofs.

Only original index-paletted Doom glyphs are used. No Pillow or ESP32 dependency.
Usage: python artwork/fonts/neutral/generate.py --check  (read-only)
       python artwork/fonts/neutral/generate.py --write  (regenerate files)
"""
from __future__ import annotations

import argparse
import binascii
import hashlib
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[3]
ASSETS = ROOT / "sdroot" / "doomrpg" / "img"
PREVIEWS = ROOT / "artwork" / "fonts" / "neutral" / "previews"
BG = (19, 25, 33)
PADDING = 16
GAP = 16
COMPARISON_SCALE = 2
THEME_SCALE = 2
GLYPHS = "ADOao08%"
LABEL_HEIGHT = 24
# The new large 4bpp neutral font comes from the smaller a.bmp, NOT from
# the historically distinct larger_font.bmp. Keep that old asset untouched.
# Scale 9x12 -> an 11x15 ink grid centred within each required 13x17 cell.
# This deliberately preserves the delicate small-font strokes and cell spacing.
LARGE_COLOR_PROOF = ROOT / "artwork" / "fonts" / "neutral" / "larger_font_from_a_color.bmp"
LARGE_INNER = (11, 15)
LARGE_MARGIN = (1, 1)
LABEL_FONT = {
    "A": ("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
    "D": ("11110", "10001", "10001", "10001", "10001", "10001", "11110"),
    "E": ("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
    "G": ("01111", "10000", "10000", "10111", "10001", "10001", "01110"),
    "H": ("10001", "10001", "10001", "11111", "10001", "10001", "10001"),
    "I": ("11111", "00100", "00100", "00100", "00100", "00100", "11111"),
    "L": ("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
    "N": ("10001", "11001", "10101", "10011", "10001", "10001", "10001"),
    "O": ("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
    "P": ("11110", "10001", "10001", "11110", "10000", "10000", "10000"),
    "R": ("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
    "S": ("01111", "10000", "10000", "01110", "00001", "00001", "11110"),
    "T": ("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
    "U": ("10001", "10001", "10001", "10001", "10001", "10001", "01110"),
}
SPECS = (
    ("a.bmp", "a_neutral.bmp", (144, 72), (9, 12), 0,
     "e088bb3f65f9314e394443dc659c643fd4cca0cd10fd5782d9b698ca16f8deae"),
    ("larger_font.bmp", "larger_font_neutral.bmp", (208, 102), (13, 17), 2,
     "faa893a06e63a9ca0e223cf2bcc5216950db600a437b01ab68a156eb1b8a4908"),
)
# Top -> middle -> bottom, relative to each GLYPH, not to the whole atlas.
THEMES = (
    ("classic", ((105, 49, 20), (220, 142, 45), (255, 236, 171))),
    ("inferno", ((90, 12, 29), (246, 41, 38), (255, 173, 202))),
    ("phosphore", ((0, 76, 96), (0, 236, 136), (198, 255, 223))),
    ("white", ((255, 255, 255),) * 3),
    ("blue", ((0, 0, 255),) * 3),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def inspect_bmp(data: bytes, size: tuple[int, int]) -> dict:
    assert data[:2] == b"BM" and len(data) >= 118
    assert struct.unpack_from("<I", data, 2)[0] == len(data)
    assert struct.unpack_from("<I", data, 10)[0] == 118
    assert struct.unpack_from("<I", data, 14)[0] == 40
    width, height, planes, bpp, compression = struct.unpack_from("<iiHHI", data, 18)
    assert (width, height) == size and height > 0
    assert (planes, bpp, compression) == (1, 4, 0)
    assert struct.unpack_from("<I", data, 46)[0] == 16
    pitch = ((width * 4 + 31) // 32) * 4
    assert 118 + pitch * height == len(data)
    palette = [tuple(data[54 + i * 4 + j] for j in (2, 1, 0))
               for i in range(16)]
    return {"data": data, "w": width, "h": height, "pitch": pitch, "palette": palette}


def pixel(sheet: dict, x: int, y: int) -> int:
    assert 0 <= x < sheet["w"] and 0 <= y < sheet["h"]
    b = sheet["data"][118 + (sheet["h"] - 1 - y) * sheet["pitch"] + x // 2]
    return (b >> 4) if x % 2 == 0 else (b & 15)


def neutral_gray(r: int, g: int, b: int) -> int:
    # Integer Rec.709 (+0.5 rounding) with 20% max-channel highlight lift.
    luminance = (2126 * r + 7152 * g + 722 * b + 5000) // 10000
    return (4 * luminance + max(r, g, b) + 2) // 5


def neutralize(data: bytes, transparent_index: int) -> bytes:
    result = bytearray(data)
    for i in range(16):
        if i == transparent_index:
            continue
        p = 54 + i * 4
        gray = neutral_gray(data[p + 2], data[p + 1], data[p])
        result[p:p + 3] = bytes((gray, gray, gray))
    return bytes(result)


def scale_small_to_large(small_data: bytes) -> bytes:
    """Rebuild 96 glyphs from actual a.bmp as a thinner 13x17 4-bpp atlas.

    A full-canvas nearest-neighbour 9x12 -> 13x17 would thicken every
    stroke. Instead resize each 9x12 glyph to 11x15 with 1px margins.
    Small palette 0 = transparent magenta; big format needs index 2.
    Swap indices 0/2 and their palette entries; keep all other indices.
    The historical larger_font.bmp is never read as glyph input.
    """
    small = inspect_bmp(small_data, (144, 72))
    assert small["palette"][0] == (255, 0, 255)
    width, height, pitch = 208, 102, 104
    output = bytearray(118 + pitch * height)
    output[:118] = small_data[:118]
    struct.pack_into("<I", output, 2, len(output))
    struct.pack_into("<i", output, 18, width)
    struct.pack_into("<i", output, 22, height)
    struct.pack_into("<I", output, 34, pitch * height)
    output[54:58] = small_data[62:66]  # index 0 <- small black 2
    output[62:66] = small_data[54:58]  # index 2 <- small magenta 0
    for y in range(height):
        glyph_y, local_y = divmod(y, 17)
        for x in range(width):
            glyph_x, local_x = divmod(x, 13)
            value = 2  # magenta: 1-pixel inset, no glyph clipping
            if (1 <= local_x < 12) and (1 <= local_y < 16):
                sx = ((2 * (local_x - 1) + 1) * 9) // 22
                sy = ((2 * (local_y - 1) + 1) * 12) // 30
                value = pixel(small, glyph_x * 9 + sx, glyph_y * 12 + sy)
                if value == 0:
                    value = 2
                elif value == 2:
                    value = 0
            offset = 118 + (height - 1 - y) * pitch + x // 2
            if x & 1:
                output[offset] = (output[offset] & 0xf0) | value
            else:
                output[offset] = (output[offset] & 0x0f) | (value << 4)
    result = bytes(output)
    assert inspect_bmp(result, (width, height))["palette"][2] == (255, 0, 255)
    return result


def canvas(w: int, h: int) -> bytearray:
    return bytearray(bytes(BG) * (w * h))


def interpolate(c0: int, c1: int, n: int, d: int) -> int:
    return (c0 * (d - n) + c1 * n + d // 2) // d


def theme_color(stops: tuple, y: int, cell_h: int) -> tuple[int, int, int]:
    assert cell_h > 1 and 0 <= y < cell_h
    n, d = y * 2, cell_h - 1
    if n <= d:
        return tuple(interpolate(stops[0][i], stops[1][i], n, d)
                     for i in range(3))
    return tuple(interpolate(stops[1][i], stops[2][i], n - d, d)
                 for i in range(3))


def draw(dst: bytearray, dw: int, sheet: dict, palette: list,
         transparent_index: int, ox: int, oy: int, scale: int,
         crop: tuple | None = None, stops: tuple | None = None,
         cell_h: int = 12) -> None:
    sx, sy, sw, sh = crop or (0, 0, sheet["w"], sheet["h"])
    for y in range(sh):
        target_color = theme_color(stops, (sy + y) % cell_h, cell_h) if stops else None
        for x in range(sw):
            i = pixel(sheet, sx + x, sy + y)
            if i == transparent_index:
                continue  # magenta becomes preview background only
            rgb = palette[i]
            if target_color:
                gray = rgb[0]
                rgb = tuple((gray * c + 127) // 255 for c in target_color)
            row_bytes = bytes(rgb) * scale
            for dy in range(scale):
                begin = ((oy + y * scale + dy) * dw + ox + x * scale) * 3
                dst[begin:begin + len(row_bytes)] = row_bytes


def draw_label(dst: bytearray, dw: int, ox: int, oy: int,
               text: str, color: tuple[int, int, int]) -> None:
    """Deterministic five-by-seven caption; no Pillow/system font needed."""
    for i, letter in enumerate(text):
        for y, pixels in enumerate(LABEL_FONT.get(letter, ("00000",) * 7)):
            for x, value in enumerate(pixels):
                if value == "0":
                    continue
                for dy in range(2):
                    for dx in range(2):
                        at = ((oy + y * 2 + dy) * dw + ox + i * 12
                              + x * 2 + dx) * 3
                        dst[at:at + 3] = bytes(color)


def png_chunk(name: bytes, payload: bytes) -> bytes:
    chunk = name + payload
    return struct.pack(">I", len(payload)) + chunk + struct.pack(
        ">I", binascii.crc32(chunk) & 0xffffffff)


def png(w: int, h: int, pixels: bytearray) -> bytes:
    """PNG truecolor, filter=0, reproducible zlib stored blocks (no version drift)."""
    assert len(pixels) == w * h * 3
    scanline = w * 3
    raw = b"".join(b"\0" + pixels[y * scanline:(y + 1) * scanline]
                   for y in range(h))
    segments = [b"\x78\x01"]
    for start in range(0, len(raw), 65535):
        block = raw[start:start + 65535]
        final = int(start + len(block) == len(raw))
        segments.append(bytes((final,)) + struct.pack("<HH", len(block),
                         65535 - len(block)) + block)
    segments.append(struct.pack(">I", zlib.adler32(raw) & 0xffffffff))
    payload = b"".join(segments)
    return (b"\x89PNG\r\n\x1a\n"
            + png_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + png_chunk(b"IDAT", payload)
            + png_chunk(b"IEND", b""))


def render_outputs() -> dict[Path, bytes]:
    outputs = {}
    for original, neutral, size, cell, transparent, expected_hash in SPECS:
        historical_bytes = (ASSETS / original).read_bytes()
        assert sha256(historical_bytes) == expected_hash, original + " changed!"
        if original == "a.bmp":
            source_bytes = historical_bytes
        else:
            # Preview's SOURCE is the new generated color sheet from a.bmp.
            # Historical larger_font.bmp stays untouched and is not portrayed
            # as the source of this new neutral style.
            source_bytes = scale_small_to_large((ASSETS / "a.bmp").read_bytes())
            outputs[LARGE_COLOR_PROOF] = source_bytes
        source = inspect_bmp(source_bytes, size)
        assert source["palette"][transparent] == (255, 0, 255)
        neutral_bytes = neutralize(source_bytes, transparent)
        neut = inspect_bmp(neutral_bytes, size)
        assert source_bytes[:54] == neutral_bytes[:54]
        if original == "a.bmp":
            assert source_bytes[118:] == neutral_bytes[118:]
        outputs[ASSETS / neutral] = neutral_bytes

        w, h = size
        cw, ch = cell
        scale = COMPARISON_SCALE
        bw = PADDING * 2 + w * scale * 3 + GAP * 2
        bh = PADDING * 2 + LABEL_HEIGHT + h * scale
        comparison = canvas(bw, bh)
        panel_x = [PADDING + i * (w * scale + GAP) for i in range(3)]
        panel_labels = ("ORIGINAL", "NEUTRE", "PHOSPHORE") if original == "a.bmp" else ("A AGRANDI", "NEUTRE", "PHOSPHORE")
        for x, label_text, label_color in zip(
                panel_x, panel_labels,
                ((255, 195, 94), (230, 232, 236), (112, 255, 172))):
            draw_label(comparison, bw, x, PADDING, label_text, label_color)
        top = PADDING + LABEL_HEIGHT
        draw(comparison, bw, source, source["palette"], transparent,
             panel_x[0], top, scale)
        draw(comparison, bw, neut, neut["palette"], transparent,
             panel_x[1], top, scale)
        draw(comparison, bw, neut, neut["palette"], transparent,
             panel_x[2], top, scale, stops=THEMES[2][1], cell_h=ch)
        stem = original.removesuffix(".bmp")
        outputs[PREVIEWS / (stem + "_compare.png")] = png(bw, bh, comparison)

        zoom_scale = 5 if original == "a.bmp" else 4
        spacing = 12
        panel_w, panel_h = cw * zoom_scale + spacing, ch * zoom_scale + spacing
        zw, zh = 8 * panel_w + spacing, 2 * panel_h + spacing
        zoom = canvas(zw, zh)
        for row in range(2):  # original above, neutral below
            palette = source["palette"] if row == 0 else neut["palette"]
            for i, glyph in enumerate(GLYPHS):
                index = ord(glyph) - 33  # authentic Doom 16x6 cell mapping
                assert 0 <= index < 96
                xywh = ((index % 16) * cw, (index // 16) * ch, cw, ch)
                draw(zoom, zw, source if row == 0 else neut,
                     palette, transparent,
                     spacing + i * panel_w, spacing + row * panel_h,
                     zoom_scale, xywh)
        outputs[PREVIEWS / (stem + "_zoom.png")] = png(zw, zh, zoom)

        tw, th = PADDING * 2 + w * THEME_SCALE, PADDING * 6 + h * THEME_SCALE * 5
        theme_image = canvas(tw, th)
        for i, (_theme_name, stops) in enumerate(THEMES):
            draw(theme_image, tw, neut, neut["palette"], transparent,
                 PADDING, PADDING + i * (h * THEME_SCALE + PADDING),
                 THEME_SCALE, stops=stops, cell_h=ch)
        outputs[PREVIEWS / (stem + "_themes.png")] = png(tw, th, theme_image)
    return outputs


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true", help="write generated artifacts")
    mode.add_argument("--check", action="store_true", help="compare artifacts byte-for-byte")
    args = parser.parse_args()
    results = render_outputs()
    for path, data in results.items():
        if args.write:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        else:
            assert path.is_file(), "missing: " + str(path)
            assert path.read_bytes() == data, "non-reproducible: " + str(path)
        print("FONT-NEUTRAL", "WRITE" if args.write else "PASS",
              path.relative_to(ROOT), len(data), sha256(data))
    print("FONT-NEUTRAL-P1: grayscale + thin large font rebuilt from a.bmp; no runtime changes")


if __name__ == "__main__":
    main()
