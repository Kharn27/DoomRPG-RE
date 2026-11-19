#!/usr/bin/env python3
"""Verify UAC-P1 immutable SD BMP fingerprints embedded in production C++.

Host-only: proves the 18 exact payloads, order of the 2x9 preflight table,
and detection of same-size pixel damage without reading entire images on CYD.
"""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "ESP32/src/esp_uac_menu_preview.cpp"
MANIFEST = ROOT / "artwork/menu-uac/SHA256SUMS.txt"
SD = ROOT / "sdroot/doomrpg/img"
src = SOURCE.read_text()
table = src.split("const char* const kSha256[2][9] =", 1)[1].split("\n};", 1)[0]
embedded = re.findall(r'"([0-9a-f]{64})"', table)
assert len(embedded) == 18, "exact 2x9 native digests required"
manifest = dict(
    (path.split("/")[-1], digest)
    for line in MANIFEST.read_text().splitlines()
    if line and not line.startswith("#")
    for digest, path in [line.split(maxsplit=1)]
)
assert len(manifest) == 18
names = []
for size in (120, 240):
    names.append(f"menu_frame_{size}.bmp")
    for button in ("start", "load", "options", "help"):
        for state in ("inactive", "active"):
            names.append(f"menu_{button}_{state}_{size}.bmp")
assert len(names) == 18
for i, name in enumerate(names):
    raw = (SD / name).read_bytes()
    assert embedded[i] == manifest[name], ("firmware digest drift", name)
    assert hashlib.sha256(raw).hexdigest() == embedded[i], ("SD final corrupt", name)
    damaged = bytearray(raw)
    damaged[-5] ^= 0x01
    assert hashlib.sha256(damaged).hexdigest() != embedded[i], (
        "same-size pixel damage bypassed SHA-256", name
    )
print("UAC-P1 PASS 18 BMP production SHA256 fingerprints and 18 same-size pixel damage mutants")
