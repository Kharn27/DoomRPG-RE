#!/usr/bin/env python3
"""Generate UAC visual proofs from the actual final BMPs, host-side only.

Run from repo root:
    python artwork/menu-uac/tools/render_previews.py --write
    python artwork/menu-uac/tools/render_previews.py --check
"""
from __future__ import annotations

import argparse
import hashlib
import io
import struct
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[3]
SD = ROOT / "sdroot" / "doomrpg" / "img"
ART = ROOT / "artwork" / "menu-uac"
OUT = ART / "previews"
BUTTONS = ("start", "load", "options", "help")
STATES = ("active", "inactive")
SIZES = (120, 240)
WIDTH = {120: 59, 240: 118}
BUTTON_SIZE = {120: (55, 20), 240: (110, 40)}
POS = {120: ((2, 10), (2, 37), (2, 64), (2, 91)),
       240: ((4, 20), (4, 74), (4, 128), (4, 182))}
DARK = (26, 33, 43, 255)
SHEET = (15, 21, 30, 255)
LIGHT = (226, 230, 234, 255)
ORANGE = (255, 158, 62, 255)


def load_manifest() -> dict[str, str]:
    values = {}
    for line in (ART / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        digest, path = line.split(maxsplit=1)
        path = path.strip()
        assert path.startswith("sdroot/doomrpg/img/menu_"), path
        assert path not in values and len(digest) == 64, path
        values[path] = digest
    assert len(values) == 18, len(values)
    return values


def load_bmp(name: str, expected: tuple[int, int],
             manifest: dict[str, str]) -> Image.Image:
    raw = (SD / name).read_bytes()
    key = "sdroot/doomrpg/img/" + name
    assert hashlib.sha256(raw).hexdigest() == manifest[key], name + " SHA256"
    assert raw[:2] == b"BM", name
    assert struct.unpack_from("<I", raw, 2)[0] == len(raw), name
    assert struct.unpack_from("<I", raw, 10)[0] == 122, name
    assert struct.unpack_from("<I", raw, 14)[0] == 108, name
    assert struct.unpack_from("<ii", raw, 18) == expected, name
    assert struct.unpack_from("<H", raw, 28)[0] == 32, name
    assert struct.unpack_from("<I", raw, 30)[0] == 3, name
    assert struct.unpack_from("<IIII", raw, 54) == (
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
    ), name + " RGBA masks"
    with Image.open(io.BytesIO(raw)) as bmp:
        img = bmp.convert("RGBA")
    assert img.size == expected, (name, img.size)
    assert img.getchannel("A").getextrema() == (0, 255), name
    return img


def as_png(image: Image.Image) -> bytes:
    stream = io.BytesIO()
    image.save(stream, format="PNG", optimize=False)
    return stream.getvalue()


def preview_button(button: Image.Image, size: int) -> Image.Image:
    # Display backing only; unchanged pixels and alpha from final BMP.
    out = Image.new("RGBA", button.size, DARK)
    out.alpha_composite(button)
    scale = 2 if size == 120 else 1
    return out.resize((button.width * scale, button.height * scale),
                      Image.Resampling.NEAREST).convert("RGB")


def compose(frame: Image.Image, buttons: dict[str, Image.Image],
            size: int, selected: str) -> Image.Image:
    out = Image.new("RGBA", frame.size, DARK)
    w, h = BUTTON_SIZE[size]
    for name, (x, y) in zip(BUTTONS, POS[size]):
        assert x >= 0 and y >= 0
        assert x + w <= frame.width and y + h <= frame.height
        # The center of the button must be a transparent frame opening.
        assert frame.getpixel((x + w // 2, y + h // 2))[3] < 32
        state = "active" if name == selected else "inactive"
        out.alpha_composite(buttons[name + "_" + state], (x, y))
    # Frame last = metallic borders above button sprites, as documented.
    out.alpha_composite(frame)
    return out.convert("RGB")


def proof_sheet(compositions: dict[str, Image.Image], size: int) -> Image.Image:
    # Left to right: START, LOAD, OPTIONS, HELP, all exact final BMP art.
    scale = 2 if size == 120 else 1
    w, h = WIDTH[size] * scale, size * scale
    gap = 12
    page = Image.new("RGBA", (4 * w + 5 * gap, h + 2 * gap), SHEET)
    for idx, selected in enumerate(BUTTONS):
        panel = compositions[selected].resize((w, h), Image.Resampling.NEAREST)
        page.alpha_composite(panel.convert("RGBA"),
                             (gap + idx * (w + gap), gap))
    return page.convert("RGB")


def render() -> dict[str, bytes]:
    manifest = load_manifest()
    previews = {}
    for size in SIZES:
        frame = load_bmp("menu_frame_" + str(size) + ".bmp",
                         (WIDTH[size], size), manifest)
        buttons = {}
        for name in BUTTONS:
            for state in STATES:
                suffix = name + "_" + state
                bitmap = load_bmp("menu_" + suffix + "_" + str(size) + ".bmp",
                                  BUTTON_SIZE[size], manifest)
                buttons[suffix] = bitmap
                previews["button_" + suffix + "_" + str(size) + ".png"] = as_png(
                    preview_button(bitmap, size))
        composites = {name: compose(frame, buttons, size, name)
                      for name in BUTTONS}
        previews["composition_" + str(size) + "_start.png"] = as_png(
            composites["start"])
        previews["proof_" + str(size) + "_selections.png"] = as_png(
            proof_sheet(composites, size))
        print("UAC proof:", size, "frame", frame.size,
              "button", BUTTON_SIZE[size], "positions", POS[size])
    assert len(previews) == 20, len(previews)
    return previews


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", action="store_true")
    group.add_argument("--check", action="store_true")
    args = parser.parse_args()
    previews = render()
    if args.write:
        OUT.mkdir(parents=True, exist_ok=True)
        for name, data in sorted(previews.items()):
            (OUT / name).write_bytes(data)
    if args.check:
        found = {p.name for p in OUT.glob("*.png")}
        assert found == set(previews), ("unexpected/missing previews",
                                        sorted(found ^ set(previews)))
        for name, data in previews.items():
            with Image.open(OUT / name) as actual_img, Image.open(io.BytesIO(data)) as expected_img:
                actual = actual_img.convert("RGB")
                expected = expected_img.convert("RGB")
                assert actual.size == expected.size, name + " geometry differs"
                delta = ImageChops.difference(actual, expected)
                assert max(c[1] for c in delta.getextrema()) <= 2, name + " pixel mismatch"
    print("PASS: 18 BMP V4 alpha SHA256 matched, 16 buttons and "
          "2 composites plus 2 proof sheets, 20 reproducible PNGs")


if __name__ == "__main__":
    main()
