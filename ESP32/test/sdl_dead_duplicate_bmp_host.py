#!/usr/bin/env python3
"""SDL-B7: retire unused SDL 8-bit BMP decoder, preserve native indexed reader.

SDL-B7's 1/4/8-bpp reader stays as unlinked oracle; SDL-C1 now routes the
production image owner directly through the packed native PAK range reader. The duplicate SDL_LoadBMP_RW decoder and its
three private helper functions were not linked in historical/current firmwares.
Do not touch SDL_Surface, RWops read/seek/close, texture adoption or blitting.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
OLD = ("SDL_LoadBMP_RW", "rwSeek", "readLe16", "readLe32")
ACTIVE = ("SDL_RWFromMem(", "SDL_RWread(", "SDL_RWseek(", "SDL_RWclose(",
          "SDL_FreeSurface(", "SDL_CreateTextureFromSurface(", "SDL_RenderCopy(",
          "SDL_RenderPresent(", "SDL_GetError(", "SDL_ShowMessageBox(")


def main():
    count = 0
    for folder in ("src", "include"):
        for p in (ROOT / "ESP32" / folder).rglob("*"):
            if p.suffix not in (".c", ".cpp", ".h", ".hpp"):
                continue
            count += 1
            content = p.read_text(encoding="utf-8", errors="replace")
            # SDL_LoadBMP_RW must have no real caller; ignore historical comments.
            code = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", content)
            assert re.search(r"\bSDL_LoadBMP_RW\s*\(", code) is None, (
                f"SDL-B7 old BMP decoder still referenced in {p}"
            )
    shim = (ROOT / "ESP32/src/esp32_sdl.cpp").read_text(encoding="utf-8")
    header = (ROOT / "ESP32/include/SDL.h").read_text(encoding="utf-8")
    for name in OLD:
        assert not re.search(r"\b" + name + r"\s*\(", shim), (
            f"SDL-B7 obsolete helper retained: {name}"
        )
    assert "SDL_LoadBMP_RW" not in header, "obsolete declaration remains"
    for api in ACTIVE:
        assert api in shim, f"SDL-B7 removed a required SDL runtime entry: {api}"
        assert api in header, f"SDL-B7 removed active SDL header declaration: {api}"
    native = (ROOT / "ESP32/src/esp32_bmp.cpp").read_text(encoding="utf-8")
    owner = (ROOT / "ESP32/src/esp_doomrpg_image_resources.c").read_text(encoding="utf-8")
    assert "Esp32Bmp_LoadRW(SDL_RWops* source, int freeSource)" in native
    assert 'bitsPerPixel != 1 && bitsPerPixel != 4 && bitsPerPixel != 8' in native
    assert "surface->pitch = static_cast<int>(packedPitch)" in native
    assert "SDL_RWseek(source, 14 + dibSize, SEEK_SET)" in native
    assert "EspNativeImage_loadPack(fileName, isTransparentMask, &decoded)" in owner
    assert "EspNativeTexture_adoptIndexed(&decoded)" in owner
    assert "SDL_RWFromMem(" not in owner
    assert "SDL_CreateTextureFromSurface(" not in owner
    assert "SDL_FreeSurface(" not in owner
    assert "PlatformVideo_present()" in shim
    assert "SDL_ShowMessageBox(&messageboxdata, NULL);" in (
        ROOT / "ESP32/src/engine_stubs.c").read_text(encoding="utf-8")
    print(f"SDL-C1 PASS B7 decoder absent from {count} native files; production image loads via native pack-range owner")


if __name__ == "__main__":
    main()
