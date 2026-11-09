#!/usr/bin/env python3
"""SDL-B2 ownership guard for retired full-screen demo path.

No ESP32 source may call, declare, or define the diagnostic pattern or its
circle-only helpers. The live SDL indexed blitter remains covered by the
compiled C++ pixel/lifetime test, and normal firmware linkage by the ELF gate.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
RETIRED = (
    "Esp32Sdl_showTestPattern",
    "SDL_RenderDrawCircle",
    "SDL_RenderDrawFillCircle",
)


def main():
    count = 0
    for folder in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
        for path in folder.rglob("*"):
            if path.suffix not in (".c", ".cpp", ".h", ".hpp"):
                continue
            count += 1
            body = path.read_text(encoding="utf-8", errors="replace")
            for symbol in RETIRED:
                assert not re.search(r"\b" + re.escape(symbol) + r"\s*\(", body), (
                    f"SDL-B2 retired demo reference: {symbol} in {path}"
                )

    src = (ROOT / "ESP32/src/esp32_sdl.cpp").read_text()
    hdr = (ROOT / "ESP32/include/esp32_sdl_platform.h").read_text()
    for live in (
        "void Esp32Sdl_attachDisplay(", "SDL_RenderCopy(", "SDL_RenderDrawLine(",
        "SDL_RenderFillRect(", "SDL_RenderPresent(", "SDL_RenderSetClipRect(",
        "SDL_CreateTextureFromSurface(", "SDL_UpdateTexture(", "SDL_DestroyTexture(",
    ):
        assert live in src, f"Lost live SDL rendering/texture owner: {live}"
    assert "void Esp32Sdl_attachDisplay(" in hdr
    assert "PlatformVideo_present()" in src
    assert "surface->pixels = nullptr;" in src
    assert "texture->indexed = true;" in src
    assert "free(texture->indexedPixels);" in src
    print(
        f"SDL-B2 PASS demo-only pattern + circle helpers retired; "
        f"{count} native files verified, live blitter/framebuffer ABI preserved"
    )


if __name__ == "__main__":
    main()
