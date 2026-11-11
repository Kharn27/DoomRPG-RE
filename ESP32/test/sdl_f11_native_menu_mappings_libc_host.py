#!/usr/bin/env python3
"""SDL-F11: libc aliases are identical to old ESP32 SDL.h in two native owners.

Restore the exact pre-F11 source of both C translation units and compare full
UTF-8 FNV-1a; this pins not only malloc/calloc/free/memset behavior but also
allocation order, sizes, fail-closed paths, PAK reads, and cleanup topology.
The SDL.h shim is an uncompiled source oracle, never a runtime requirement.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
SHIM = (ROOT / "ESP32/include/SDL.h").read_text(encoding="utf-8")
for old, new in (
    ("SDL_malloc", "malloc"),
    ("SDL_calloc", "calloc"),
    ("SDL_free", "free"),
    ("SDL_memset", "memset"),
):
    assert re.search(r"^#define\s+" + old + r"\s+" + new + r"\s*$",
                     SHIM, re.M), (old, new)


def restore_menu(text: str) -> str:
    new = "#include <stdlib.h>\n#include <string.h>\n"
    assert text.startswith(new)
    text = "#include <SDL.h>\n" + text[len(new):]
    for current, previous in (
        ("calloc(1, sizeof(EspNativeMenuState_t))", "SDL_calloc(1, sizeof(EspNativeMenuState_t))"),
        ("memset(storage, 0, sizeof(EspNativeMenuState_t))", "SDL_memset(storage, 0, sizeof(EspNativeMenuState_t))"),
        ("free(storage);", "SDL_free(storage);"),
    ):
        assert text.count(current) == 1, (current, text.count(current))
        text = text.replace(current, previous, 1)
    return text


def restore_mappings(text: str) -> str:
    assert text.startswith("#include <stdlib.h>\n")
    text = "#include <SDL.h>\n" + text[len("#include <stdlib.h>\n"):]
    line = '#include "esp_legacy_asset_source.h"\n'
    assert text.count(line) == 1
    text = text.replace(line, '#include "SDL_Video.h"\n' + line, 1)
    assert text.count("free(render->media") == 4
    text = text.replace("free(render->media", "SDL_free(render->media")
    assert text.count("malloc(plan.") == 4
    text = text.replace("malloc(plan.", "SDL_malloc(plan.")
    return text


def fnv(text: str) -> int:
    h = 2166136261
    for byte in text.encode("utf-8"):
        h = ((h ^ byte) * 16777619) & 0xFFFFFFFF
    return h


for filename, reverse, original in (
    ("esp_native_menu_storage.c", restore_menu, 0x6C8E1881),
    ("esp_legacy_config_mappings_startup.c", restore_mappings, 0x4C38AD34),
):
    current = (SRC / filename).read_text(encoding="utf-8")
    assert "#include <SDL.h>" not in current, filename
    assert '#include "SDL_Video.h"' not in current, filename
    active = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", current)
    assert not re.search(r"\bSDL_\w+\b", active), filename
    restored = reverse(current)
    assert fnv(restored) == original, (
        f"{filename} drift outside SDL-F11 alias boundary: "
        f"{fnv(restored):08x} != {original:08x}"
    )

menu = (SRC / "esp_native_menu_storage.c").read_text()
assert "if (storage == NULL) return NULL;" in menu
assert "if (doomRpg != NULL)" in menu
assert "if (freePtr) free(storage);" in menu
mapping = (SRC / "esp_legacy_config_mappings_startup.c").read_text()
for required in (
    "if (render->mediaTexelOffsets == NULL ||",
    "releaseMappings(render);",
    'EspLegacyAssetSource_readInto("mappings.bin",',
    "if (root == NULL || EspNativeRootOwners_live()->render == NULL)",
    "if (configMappingsAttempted)",
):
    assert required in mapping, required

print("SDL-F11 PASS native menu+PAK mapping allocation owners libc-only;"
      " complete original C source parity exact; rollback, PAK, Root-J4 frozen")
