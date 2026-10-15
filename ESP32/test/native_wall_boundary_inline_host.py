#!/usr/bin/env python3
"""Compile the *actual* bounded renderer byte resolver and span sampler against
a deterministic PAK/mappings double. These are extracted, not reimplemented.
No ESP32 firmware or SD is emulated; physical CYD verification remains required.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "ESP32/src/esp_native_first_frame.c"
HARNESS = ROOT / "ESP32/test/native_wall_boundary_inline_host.c"

def extract(text, signature):
    start = text.find(signature)
    if start < 0 or text.count(signature) != 1:
        raise AssertionError("missing or duplicate renderer function: " + signature)
    brace = text.index("{", start)
    depth = 0
    for pos in range(brace, len(text)):
        if text[pos] == "{":
            depth += 1
        if text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[start:pos + 1]
    raise AssertionError("unterminated renderer body: " + signature)

def main():
    renderer = SOURCE.read_text()
    assert "renderFrameWithLegacyGuardRecovery(" not in renderer
    wrapper = extract(renderer, "static int renderFrameWithInlineBoundary(")
    assert "return renderFrame(" in wrapper and "for (;;)" not in wrapper
    resolver = extract(renderer, "static int prepareLegacyWallGuard(")
    sampler = extract(renderer, "static int sampleWallSpan(")
    assert "EspAssetPack_open(" not in resolver and "EspAssetPack_close(" not in resolver
    assert "!EspAssetPack_isOpen()" in resolver
    assert "prepareLegacyWallGuard(work, source)" in sampler
    assert "WALL_PACKED_BYTES" in sampler and "RECORD_FRAME_FAILURE" in sampler
    code = "\n\n".join([
        extract(renderer, "static uint16_t readLe16("),
        extract(renderer, "static uint32_t readLe32("),
        resolver, sampler
    ])
    source = HARNESS.read_text()
    assert source.count("/* PRODUCTION_FUNCTIONS */") == 1
    source = source.replace("/* PRODUCTION_FUNCTIONS */", code)
    with tempfile.TemporaryDirectory(prefix="doomrpg-wall-inline-") as tmp:
        path = Path(tmp) / "wall_boundary.c"
        binary = Path(tmp) / "wall_boundary"
        path.write_text(source)
        cc = shlex.split(os.environ.get("CC", "cc"))
        subprocess.run(cc + ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                             "-fwrapv", str(path), "-o", str(binary)], check=True, cwd=ROOT)
        subprocess.run([str(binary)], check=True, cwd=ROOT)

if __name__ == "__main__":
    main()
