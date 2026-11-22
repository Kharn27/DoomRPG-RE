#!/usr/bin/env python3
"""Validate the user-selected neutral Doom atlases on SD without touching assets.

The archived colored BMPs are artist references. Only a.bmp and
larger_font.bmp are read by the native font owner at runtime.
"""
from pathlib import Path
import hashlib
import struct

ROOT = Path(__file__).resolve().parents[2]
SD = ROOT / "sdroot/doomrpg/img"
CASES = {
    "a.bmp": ((144, 72), (9, 12), 0,
              "1724b300816bad83aba5017395a51666df532f3f19fcfd4a3958f9e72bf5657f"),
    "larger_font.bmp": ((208, 102), (13, 17), 2,
              "cb974827c206ab80342af9af91680c2c871a69cc855d36ed0d99f39e5e1a879c"),
}

def load(name, expected):
    dims, cell, transparent, sha = expected
    data = (SD / name).read_bytes()
    assert hashlib.sha256(data).hexdigest() == sha, (name, "SHA-256")
    assert data[:2] == b"BM"
    assert len(data) == struct.unpack_from("<I", data, 2)[0]
    assert struct.unpack_from("<I", data, 10)[0] == 118
    assert struct.unpack_from("<IiiHHI", data, 14) == (40, *dims, 1, 4, 0)
    assert struct.unpack_from("<I", data, 46)[0] == 16
    assert dims == (16*cell[0], 6*cell[1])
    palette = [tuple(data[54+i*4:58+i*4]) for i in range(16)]
    assert palette[transparent] == (255, 0, 255, 0)
    for index, (blue, green, red, reserved) in enumerate(palette):
        if index != transparent:
            assert red == green == blue, (name, index, "non-neutral")
        assert reserved == 0
    pitch = ((dims[0] * 4 + 31) // 32) * 4
    assert len(data) == 118 + pitch * dims[1]
    def pixel(x, y):
        pos = 118 + (dims[1] - 1 - y)*pitch + x//2
        packed = data[pos]
        return packed >> 4 if x%2 == 0 else packed & 15
    assert any(pixel(x,y)!=transparent for y in range(dims[1]) for x in range(dims[0]))
    print(f"FONT-SD NEUTRAL PASS {name} {dims[0]}x{dims[1]} 4bpp 16 colors SHA={sha}")
    return data, pixel

small, small_pixel = load("a.bmp", CASES["a.bmp"])
large, large_pixel = load("larger_font.bmp", CASES["larger_font.bmp"])
# Full 96-glyph source-grid oracle from FONT-NEUTRAL-P1: nearest
# 9x12 -> 11x15, centered in a 13x17 tile; magenta index 0 <-> 2.
for y in range(102):
    for x in range(208):
        cx, cy = x % 13, y % 17
        expected = 2
        if 1 <= cx <= 11 and 1 <= cy <= 15:
            sx = (2 * (cx - 1) + 1)*9//22
            sy = (2 * (cy - 1) + 1)*12//30
            index = small_pixel((x//13)*9+sx, (y//17)*12+sy)
            expected = 2 if index == 0 else (0 if index == 2 else index)
        assert large_pixel(x, y) == expected, (x, y, "large projection drift")
for char, expected_count in (("a", 80), ("o", 81)):
    index = ord(char)-33
    x0,y0 = (index%16)*13, (index//16)*17
    count = sum(large_pixel(x0+x,y0+y)!=2 for y in range(17) for x in range(13))
    assert count == expected_count,(char,count)
# These are optional on-card reference aliases, not selected by the
# runtime font owner. Do not delete assets the user intentionally uploaded.
ALIASES = {
    "a_doom.bmp": "e088bb3f65f9314e394443dc659c643fd4cca0cd10fd5782d9b698ca16f8deae",
    "larger_font_doom.bmp": "faa893a06e63a9ca0e223cf2bcc5216950db600a437b01ab68a156eb1b8a4908",
}
for name, sha in ALIASES.items():
    p = SD/name
    if p.exists():
        assert hashlib.sha256(p.read_bytes()).hexdigest() == sha, name
native = (ROOT/"ESP32/src/esp_native_font_sd.cpp").read_text()
assert '"/doomrpg/img/a.bmp"' in native and '"/doomrpg/img/larger_font.bmp"' in native
assert "larger_font_doom" not in native and "a_doom" not in native
print("FONT-SD ASSET PASS: 96 neutral cells, 21,216 projected pixels, transparent indexes, original aliases ignored")
