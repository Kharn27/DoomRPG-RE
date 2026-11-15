#!/usr/bin/env python3
"""SDL-F21: remove the zone-call dependence on SDL.h from the native root.

The four actual zone wrappers still forward to libc, byte-for-byte. The desktop
Z_Zone.h declaration retains its historical SDLCALL convention. Restore both
complete pre-F21 Git blobs and compile/run the real C wrapper definitions with
the old and new headers in both ESP32 and desktop/oracle configurations.
"""
from pathlib import Path
from sdl_f30_compiler_header_closure_host import restore_pre_f30_engine
import hashlib
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ENGINE = ROOT / "ESP32/src/engine_stubs.c"
ZONE = ROOT / "src/Z_Zone.h"
PRE_ENGINE = "b4c23e282db3a16f8afad3cb4c1170524cba2199"
PRE_ZONE = "4a78d541f8da06f226090c89b8f5e00516e15828"
NAMES = ("Z_Malloc", "Z_Calloc", "Z_Realloc", "Z_Free")
GUARD = ("#if !defined(DOOMRPG_ESP32) || "
         "defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\n"
         "#include <SDL.h>\n#endif\n")
CRLF = "\r\n"
HEADER_BLOCK = CRLF.join((
    "#include <stddef.h>",
    "",
    "/* Native CYD uses the C ABI; desktop preserves the historical SDL calling convention. */",
    "#if defined(DOOMRPG_ESP32)",
    "#define DOOMRPG_ZONE_CALL",
    "#else",
    "#define DOOMRPG_ZONE_CALL SDLCALL",
    "#endif",
    "",
    "",
))


def blob_sha(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()


def restore_pre_f21_engine(current: str) -> str:
    # Undo F30 host/geometry-only ZIP header gate before historic F21 Git SHA.
    current = restore_pre_f30_engine(current)
    assert current.startswith(GUARD), "F21 engine SDL include guard changed"
    assert current.count("#include <SDL.h>") == 1
    restored = "#include <SDL.h>\n" + current[len(GUARD):]
    for name in NAMES:
        new = "DOOMRPG_ZONE_CALL " + name
        assert restored.count(new) == 1, (name, restored.count(new))
        restored = restored.replace(new, "SDLCALL " + name, 1)
    assert blob_sha(restored.encode("utf-8")) == PRE_ENGINE, "F21 engine Git blob drift"
    return restored


def restore_pre_f21_zone(current: bytes) -> bytes:
    source = current.decode("utf-8")
    anchor = "#define Z_ZONE_H__" + CRLF + CRLF
    assert source.startswith("#ifndef Z_ZONE_H__" + CRLF + anchor)
    assert source.count(HEADER_BLOCK) == 1
    source = source.replace(anchor + HEADER_BLOCK, anchor, 1)
    for name in NAMES:
        new = "DOOMRPG_ZONE_CALL " + name
        assert source.count(new) == 1, name
        source = source.replace(new, "SDLCALL " + name, 1)
    restored = source.encode("utf-8")
    assert blob_sha(restored) == PRE_ZONE, "F21 historical CRLF Z_Zone.h blob drift"
    return restored


def real_zone_definitions(src: str) -> str:
    lines = []
    for name in ("Z_Init",) + NAMES:
        pattern = (r"^(?:void\*|void)\s+(?:(?:SDLCALL|DOOMRPG_ZONE_CALL)\s+)?"
                   + name + r"\([^;\n]*\)\s*\{[^\n]*\}$")
        matches = re.findall(pattern, src, flags=re.M)
        assert len(matches) == 1, (name, matches)
        lines.append(matches[0])
    return "\n".join(lines) + "\n"


BODY = r"""
int main(void) {
    Z_Init();
    unsigned char* p = (unsigned char*)Z_Malloc(8);
    if (!p) return 1;
    for (int i = 0; i < 8; ++i) p[i] = (unsigned char)(0x20 + i);
    unsigned char* grown = (unsigned char*)Z_Realloc(p, 32);
    if (!grown) { Z_Free(p); return 2; }
    p = grown;
    for (int i = 0; i < 8; ++i) if (p[i] != (unsigned char)(0x20 + i)) return 3;
    unsigned char* zero = (unsigned char*)Z_Calloc(5, 7);
    if (!zero) return 4;
    for (int i = 0; i < 35; ++i) if (zero[i]) return 5;
    unsigned char* from_null = (unsigned char*)Z_Realloc(NULL, 4);
    if (!from_null) return 6;
    from_null[0] = 0x6a;
    if (from_null[0] != 0x6a) return 7;
    Z_Free(from_null);
    Z_Free(zero);
    Z_Free(p);
    puts("zone parity: alloc=8 grow=32 calloc=35 realloc-null=4 free=3");
    return 0;
}
"""


def check_compiled_wrappers(new_engine: str, old_engine: str,
                            new_zone: bytes, old_zone: bytes) -> None:
    results = {}
    with tempfile.TemporaryDirectory(prefix="sdl-f21-zone-") as td:
        tmp = Path(td)
        for profile in ("native", "desktop"):
            for version, engine, header in (
                    ("old", old_engine, old_zone),
                    ("new", new_engine, new_zone)):
                folder = tmp / (profile + "_" + version)
                folder.mkdir()
                (folder / "Z_Zone.h").write_bytes(header)
                source = ("#include <stddef.h>\n#include <stdio.h>\n"
                          "#include <stdlib.h>\n#include <string.h>\n")
                # The historical header requires the SDL calling convention.
                # On CYD the new header MUST compile without defining SDLCALL.
                if profile == "desktop" or version == "old":
                    source += "#define SDLCALL\n"
                source += '#include "Z_Zone.h"\n'
                source += real_zone_definitions(engine) + BODY
                code = folder / "check.c"
                binary = folder / "check"
                code.write_text(source, encoding="utf-8")
                cmd = ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-I", str(folder)]
                if profile == "native":
                    cmd.append("-DDOOMRPG_ESP32=1")
                subprocess.run(cmd + [str(code), "-o", str(binary)],
                               check=True, capture_output=True, text=True)
                results[(profile, version)] = subprocess.run(
                    [str(binary)], check=True, capture_output=True).stdout
        assert results[("native", "old")] == results[("native", "new")]
        assert results[("desktop", "old")] == results[("desktop", "new")]
        assert results[("native", "new")] == results[("desktop", "new")]
        assert results[("native", "new")].startswith(b"zone parity:")
    print("SDL-F21 PASS: genuine four Z_* libc wrappers compile/run identically "
          "with native and desktop ABI, no SDL header required on CYD")


def main() -> None:
    current_engine = ENGINE.read_text(encoding="utf-8")
    current_zone = ZONE.read_bytes()
    old_engine = restore_pre_f21_engine(current_engine)
    old_zone = restore_pre_f21_zone(current_zone)
    assert b"#define DOOMRPG_ZONE_CALL SDLCALL" in current_zone
    assert b"#if defined(DOOMRPG_ESP32)" in current_zone
    assert "#include <SDL.h>" not in current_engine[len(GUARD):]
    assert current_engine.count("DOOMRPG_ZONE_CALL Z_") == 4
    assert "-<esp32_sdl.cpp>" in (ROOT / "ESP32/platformio.ini").read_text()
    check_compiled_wrappers(current_engine, old_engine, current_zone, old_zone)
    print("SDL-F21 PASS: complete pre-F21 Git engine/zone blobs "
          + PRE_ENGINE + " / " + PRE_ZONE + "; desktop SDLCALL retained")


if __name__ == "__main__":
    main()
