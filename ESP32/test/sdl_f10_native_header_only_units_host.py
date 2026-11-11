#!/usr/bin/env python3
"""SDL-F10: remove only three unnecessary SDL.h includes from native C TUs.

Full-file FNV inverse restoration pins the *entire* pre-F10 sources, including
the SDL-C1 image ABI/function bodies. Desktop/oracle SDL types and the
non-compiled legacy shim remain untouched. PlatformIO builds all 3 profiles.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ROOT / "ESP32/src"
REFERENCE = {
    "esp_doomrpg_image_resources.c": (
        "#include <SDL.h> /* transitional DoomRPG.h type ABI; not a runtime SDL call */\n",
        0x2EC88848,
    ),
    "esp_native_startup.c": ("#include <SDL.h>\n", 0xF7991D5E),
    "native_first_frame_render_wrappers.c": ("#include <SDL.h>\n", 0x303FF885),
}


def fnv(text: str) -> int:
    h = 2166136261
    for ch in text.encode("utf-8"):
        h = ((h ^ ch) * 16777619) & 0xFFFFFFFF
    return h


def active_source(text: str) -> str:
    return re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", text)


def main() -> None:
    # The source behavior must remain *byte-for-byte* identical after
    # adding back exactly its retired first include.
    for file, (header, expected) in REFERENCE.items():
        source = (SOURCES / file).read_text(encoding="utf-8")
        assert not source.startswith("\ufeff"), file
        assert "#include <SDL.h>" not in source, file
        assert '#include "SDL_Video.h"' not in source, file
        assert not re.search(r"\bSDL_\w+\b", active_source(source)), file
        restored = header + source
        value = fnv(restored)
        assert value == expected, (
            f"{file} drift outside sole SDL-F10 header removal:"
            f" {value:08x} != {expected:08x}"
        )

    image = (SOURCES / "esp_doomrpg_image_resources.c").read_text()
    for required in (
        "EspNativeImage_loadPack(fileName, isTransparentMask, &decoded)",
        "EspNativeTexture_adoptIndexed(&decoded)",
        "EspNativeImage_dispose(&decoded)",
        "EspNativeTexture_destroy(image->imgBitmap)",
        "EspLegacyAssetSource_readAlloc(fileName, &fSize)",
    ):
        assert required in image, required
    print(
        "SDL-F10 PASS three real native sources SDL-header-free; "
        "full pre-F10 source FNVs exact (image, bootstrap, world frame); "
        "PAK/indexed texture ABI untouched"
    )


if __name__ == "__main__":
    main()
