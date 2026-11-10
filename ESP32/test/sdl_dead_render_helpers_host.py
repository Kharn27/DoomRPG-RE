#!/usr/bin/env python3
"""SDL-B3: remove truly unused outline and integer-scale ESP32 shims.

No call/definition/declaration is allowed in the native ESP32 tree.
The compiled SDL-A shim test and the three-ELF symbol audit guard all
active render/presentation ownership separately.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
RETIRED = ("SDL_RenderDrawRect", "SDL_RenderSetIntegerScale")
ACTIVE = (
     "SDL_RenderDrawLine(", "SDL_RenderFillRect(",
    "SDL_RenderPresent(", "SDL_RenderSetClipRect(",
    "SDL_CreateTextureFromSurface(", 
    "SDL_UpdateTexture(",
)


def main():
    files = 0
    for directory in ("src", "include"):
        for path in (ROOT / "ESP32" / directory).rglob("*"):
            if path.suffix not in (".c", ".cpp", ".h", ".hpp"):
                continue
            files += 1
            content = path.read_text(encoding="utf-8", errors="replace")
            for symbol in RETIRED:
                assert re.search(r"\b" + re.escape(symbol) + r"\s*\(", content) is None, (
                    f"SDL-B3 dormant symbol still referenced: {symbol} at {path}"
                )

    shim = (ROOT / "ESP32/src/esp32_sdl.cpp").read_text(encoding="utf-8")
    hdr = (ROOT / "ESP32/include/SDL.h").read_text(encoding="utf-8")
    for symbol in ACTIVE:
        assert symbol in shim, f"SDL-B3 lost live renderer: {symbol}"
    native = (ROOT / "ESP32/src/esp_native_texture.cpp").read_text(encoding="utf-8")
    nativeHeader = (ROOT / "ESP32/include/esp_native_texture.h").read_text(encoding="utf-8")
    for api in ("EspNativeTexture_blit(", "EspNativeTexture_destroy(",
                "EspNativeTexture_setColorMod(", "EspNativeTexture_adoptIndexed("):
        assert api in native and api in nativeHeader, f"SDL-D1 native texture owner missing {api}"
    assert "PlatformVideo_present()" in shim
    assert "SDL_RenderDrawLine(" in hdr
    assert "SDL_RenderFillRect(" in hdr
    assert "texture->indexed = true;" in shim
    assert "surface->pixels = nullptr;" in shim
    print(
        f"SDL-B3 PASS dormant SDL rectangle-outline / no-op integer-scale "
        f"APIs absent in {files} native files, live renderer/texture ABI guarded"
    )


if __name__ == "__main__":
    main()
