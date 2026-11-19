#!/usr/bin/env python3
"""Produce the lossless, RGB24 SD override from the repository's credits PNG.

  python ESP32/tools/credits_png_to_bmp.py
  # default output: sdroot/doomrpg/img/credits.bmp (copy this to SD)

The converter runs on the development machine only. The CYD reads BMP rows
without any PNG decoder or image-wide RAM allocation.
"""
from pathlib import Path
import sys

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Pillow is required on the PC: python -m pip install pillow") from exc

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "sdroot" / "doomrpg" / "img" / "credits.png"
TARGET = ROOT / "sdroot" / "doomrpg" / "img" / "credits.bmp"

def convert(source: Path, target: Path) -> None:
    with Image.open(source) as image:
        if image.size != (128, 640) or image.mode not in ("RGB", "RGBA"):
            raise ValueError("Expected a 128x640 RGB/RGBA PNG, got " +
                             str((image.size, image.mode)))
        if image.mode == "RGBA":
            raise ValueError("Alpha PNG not supported: supply a flattened RGB image")
        image.save(target, format="BMP")
    bmp = target.read_bytes()
    if len(bmp) != 245814 or bmp[:2] != b"BM" or bmp[28:30] != b"\x18\x00":
        raise ValueError("Unexpected BMP layout; expected uncompressed RGB24")
    print(f"{target}: 128x640 RGB24, {len(bmp)} bytes, exact RGB pixels preserved")

if __name__ == "__main__":
    if len(sys.argv) not in (1, 3):
        raise SystemExit("Usage: credits_png_to_bmp.py [input.png output.bmp]")
    convert(Path(sys.argv[1]) if len(sys.argv) == 3 else SOURCE,
            Path(sys.argv[2]) if len(sys.argv) == 3 else TARGET)
