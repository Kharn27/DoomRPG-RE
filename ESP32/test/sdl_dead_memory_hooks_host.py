#!/usr/bin/env python3
"""SDL-B4: reject stale SDL allocation hook entry points in native ESP32 tree.

The two hook facades were inert for this firmware. Real RWops memory/file
allocation/free, packed-index texture transfers, and framebuffer presentation
are permanent ownership boundaries and must stay.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
RETIRED = ("SDL_SetMemoryFunctions", "SDL_GetMemoryFunctions")
LIVE = (
    "SDL_RWFromMem(", "SDL_RWFromFile(", "SDL_RWread(",
    "SDL_RWclose(", "SDL_CreateTextureFromSurface(",
    "SDL_FreeSurface(", "SDL_RenderCopy(", "SDL_RenderPresent(",
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
                    f"SDL-B4 retired hook still referenced: {symbol} in {path}"
                )

    shim = (ROOT / "ESP32/src/esp32_sdl.cpp").read_text(encoding="utf-8")
    hdr = (ROOT / "ESP32/include/SDL.h").read_text(encoding="utf-8")
    for symbol in LIVE:
        assert symbol in shim, f"SDL-B4 lost live SDL owner: {symbol}"
        assert symbol in hdr, f"SDL-B4 lost live SDL declaration: {symbol}"
    assert shim.count("static_cast<SDL_RWops*>(calloc(1, sizeof(SDL_RWops)))") == 2
    assert "free(context);" in shim
    assert "result = fclose(context->file);" in shim
    assert "PlatformVideo_present()" in shim
    assert "texture->indexed = true;" in shim
    assert "surface->pixels = nullptr;" in shim
    print(
        f"SDL-B4 PASS retired 2 inert memory hooks across {files} native files; "
        "RWops allocation/free, indexed textures, and presentation preserved"
    )


if __name__ == "__main__":
    main()
