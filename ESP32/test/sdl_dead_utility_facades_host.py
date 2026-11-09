#!/usr/bin/env python3
"""SDL-B6: six truly unlinked, caller-free SDL facades retired from native shim.

This is dead-code source retirement, NOT replacement of the active SDL ABI.
Preserve real BMP indexed read, native clip/color/blit/present and fatal diagnostics.
The RGB565 texture fallback remains in this milestone because the permanent SDL-A
host harness exercises it, even if current normal ELF GC does not link it.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
RETIRED = (
    "SDL_SwapLE16", "SDL_SwapLE32",
    "SDL_RWwrite", "SDL_LoadBMP",
    "SDL_SetRenderDrawBlendMode", "SDL_RenderClear",
)
LIVE = (
    "SDL_GetTicks(", "SDL_GetError(", "SDL_ShowMessageBox(",
    "SDL_RWFromMem(", "SDL_RWFromFile(", "SDL_RWread(", "SDL_RWseek(",
    "SDL_RWclose(", "SDL_LoadBMP_RW(", "SDL_FreeSurface(",
    "SDL_CreateTexture(", "SDL_UpdateTexture(",
    "SDL_CreateTextureFromSurface(", "SDL_DestroyTexture(",
    "SDL_RenderCopy(", "SDL_RenderPresent(", "SDL_RenderFillRect(",
    "SDL_RenderDrawLine(", "SDL_RenderSetClipRect(",
)

def main():
    files = 0
    for directory in ("src", "include"):
        for path in (ROOT / "ESP32" / directory).rglob("*"):
            if path.suffix not in (".c", ".cpp", ".h", ".hpp"):
                continue
            files += 1
            data = path.read_text(encoding="utf-8", errors="replace")
            # Only executable code counts; old commented references are not callers.
            # Preserve exact owner source body under its independent host oracle.
            code = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", data)
            for symbol in RETIRED:
                assert re.search(r"\b" + re.escape(symbol) + r"\s*\(", code) is None, (
                    f"SDL-B6 retired function still declared or called: {symbol} in {path}"
                )
    shim = (ROOT / "ESP32/src/esp32_sdl.cpp").read_text(encoding="utf-8")
    header = (ROOT / "ESP32/include/SDL.h").read_text(encoding="utf-8")
    for symbol in LIVE:
        assert symbol in shim, f"SDL-B6 unexpectedly removed live implementation {symbol}"
        assert symbol in header, f"SDL-B6 unexpectedly removed live declaration {symbol}"
    assert "PlatformVideo_present()" in shim
    assert "texture->indexed = true;" in shim
    assert "surface->pixels = nullptr;" in shim
    assert "setError(" in shim
    assert 'Serial.println("[SDL] Platform video present failed")' in shim
    fatal = (ROOT / "ESP32/src/engine_stubs.c").read_text(encoding="utf-8")
    assert "SDL_ShowMessageBox(&messageboxdata, NULL);" in fatal
    assert "SDL_BLENDMODE_NONE" in shim
    print(f"SDL-B6 PASS 6 dormant SDL facades absent from {files} native files, active paths retained")

if __name__ == "__main__":
    main()
