#!/usr/bin/env python3
"""Compare five legacy-named native geometry primitives plus native projection
and occlusion against seven isolated desktop reference bodies.

Reference source is test-only, never a firmware build input. Integer wrapping
matches the inherited 32-bit fixed arithmetic. This is not a hardware render,
world/RNG/save test, or a proof of pixel parity for an entire frame.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
CC = shlex.split(os.environ.get("CC", "cc"))
FUNCTIONS = {
    "void Render_initColumnScale(Render_t* render)": 0xc33077ed,
    "boolean Render_cullBoundingBox(Render_t* render, Node_t* node)": 0x2b5b02ac,
    "void Render_transform2DVerts(Render_t* render, Vertex_t* vert)": 0x8c89a9a6,
    "boolean Render_clipLine(Render_t* render, Line_t* line)": 0x67f70ded,
    "void Render_clipVertex(Render_t* render, Vertex_t* vert, Line_t* line, int i, int i2)": 0x0a7ffb34,
    "void Render_projectVertex(Render_t* render, Vertex_t* vert)": 0xd92b8789,
    "void Render_occludeClippedLine(Render_t* render, Line_t* line)": 0x71aa6efe,
}


def extract(source, signature):
    assert source.count(signature + "\n{") == 1, signature
    start = source.index(signature + "\n{")
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError("unterminated reference: " + signature)


def run(args):
    subprocess.run(args, cwd=ROOT, check=True)


def main():
    original = (ROOT / "src/Render.c").read_text(encoding="latin-1")
    native = (ROOT / "ESP32/src/esp_render_geometry_primitives.c").read_text()
    bodies = []
    for signature, crc in FUNCTIONS.items():
        body = extract(original, signature)
        assert zlib.crc32(body.encode("latin-1")) == crc, "reference drift: " + signature
        bodies.append(body)
    names = [re.search(r"\b(Render_\w+)\(", s)[1] for s in FUNCTIONS]
    reference = '#include <stdint.h>\n#include <SDL.h>\n#include "DoomRPG.h"\n#include "Render.h"\n#define FIXED_VERSION 1\n'
    reference += "\n".join(bodies)
    reference = "\n".join("#define %s Reference_%s" % (n, n) for n in names) + "\n" + reference
    with tempfile.TemporaryDirectory(prefix="doomrpg-geometry-host-") as tmp:
        ref = Path(tmp) / "reference.c"
        ref.write_text(reference, encoding="latin-1")
        for bringup in (False, True):
            binary = str(Path(tmp) / ("bringup" if bringup else "normal"))
            flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-fwrapv",
                     "-DDOOMRPG_ESP32=1", "-IESP32/include", "-Isrc"]
            if bringup:
                flags += ["-DDOOMRPG_ESP32_BRINGUP_PROBES=1"]
            flags += shlex.split(os.environ.get("GEOMETRY_SANITIZE_FLAGS", ""))
            run(CC + flags + ["ESP32/src/esp_render_geometry_primitives.c",
                             "ESP32/test/render_geometry_host.c", str(ref), "-o", binary])
            run([binary])


if __name__ == "__main__":
    main()
