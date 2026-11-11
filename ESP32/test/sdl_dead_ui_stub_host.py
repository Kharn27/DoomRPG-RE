#!/usr/bin/env python3
"""SDL-B5: remove caller-free keyboard label and simple message facade.

Input is owned by native CYD touch. Keep actual native error status,
the native fatal Serial diagnostic, present diagnostics, indexed texture ownership
and RWops file/memory lifecycle exactly as before.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
RETIRED = (
    "SDL_GetScancodeName",
    "SDL_ShowSimpleMessageBox",
)
LIVE = (
    "SDL_GetError(", "SDL_ShowMessageBox(", "SDL_RWFromMem(", "SDL_RWFromFile(",
    "SDL_RWclose(", "SDL_CreateTextureFromSurface(",
     "SDL_RenderPresent(",
)


def main():
    files = 0
    for directory in ("src", "include"):
        for path in (ROOT / "ESP32" / directory).rglob("*"):
            if path.suffix not in (".c", ".cpp", ".h", ".hpp"):
                continue
            files += 1
            source = path.read_text(encoding="utf-8", errors="replace")
            for symbol in RETIRED:
                assert re.search(r"\b" + re.escape(symbol) + r"\s*\(", source) is None, (
                    f"SDL-B5 obsolete UI stub referenced: {symbol} at {path}"
                )

    shim = (ROOT / "ESP32/src/esp32_sdl.cpp").read_text(encoding="utf-8")
    header = (ROOT / "ESP32/include/SDL.h").read_text(encoding="utf-8")
    for symbol in LIVE:
        assert symbol in shim, f"SDL-B5 removed live implementation {symbol}"
        assert symbol in header, f"SDL-B5 removed live declaration {symbol}"
    native = (ROOT / "ESP32/src/esp_native_texture.cpp").read_text(encoding="utf-8")
    nativeHeader = (ROOT / "ESP32/include/esp_native_texture.h").read_text(encoding="utf-8")
    for api in ("EspNativeTexture_blit(", "EspNativeTexture_destroy(",
                "EspNativeTexture_setColorMod(", "EspNativeTexture_adoptIndexed("):
        assert api in native and api in nativeHeader, f"SDL-D1 native texture owner missing {api}"
    assert "setError(" in shim
    fatal = (ROOT / "ESP32/src/engine_stubs.c").read_text(encoding="utf-8")
    assert "SDL_ShowMessageBox(&messageboxdata, NULL);" not in fatal
    assert "[DOOM ERROR] DoomRPG Error:" in fatal
    assert 'Serial.println("[SDL] Platform video present failed")' in shim
    assert "PlatformVideo_present()" in shim
    assert "texture->indexed = true;" in shim
    assert "surface->pixels = nullptr;" in shim
    print(
        f"SDL-B5 PASS 2 caller-free keyboard/message-box stubs absent in {files} "
        "native files, native fatal diagnostic and active video/texture paths preserved"
    )


if __name__ == "__main__":
    main()
