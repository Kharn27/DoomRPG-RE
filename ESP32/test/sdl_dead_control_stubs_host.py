#!/usr/bin/env python3
"""SDL-B: retired control/window shim stubs have no source-side users.

The live indexed SDL texture blitter is exercised by sdl_shim_permanent_host.py.
The CI's compiledb/ELF gates separately prove that all 19 live SDL symbols
and platform video routing remain linked, including diagnostic profiles.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
OWNERS = (ROOT / "ESP32/src", ROOT / "ESP32/include")
RETIRED = (
    "SDL_SetHint",
    "SDL_ShowCursor",
    "SDL_SetWindowFullscreen",
    "SDL_NumJoysticks",
    "SDL_IsGameController",
    "SDL_GameControllerRumble",
    "SDL_HapticRumblePlay",
    "SDL_HapticRumbleStop",
)

def main():
    scanned = 0
    for directory in OWNERS:
        for file in directory.rglob("*"):
            if file.suffix not in (".c", ".cpp", ".h", ".hpp"):
                continue
            scanned += 1
            body = file.read_text(encoding="utf-8", errors="replace")
            for name in RETIRED:
                hits = re.search(r"\b" + re.escape(name) + r"\s*\(", body)
                assert hits is None, f"Retired SDL control entry point revived: {name} in {file}"
    src = (ROOT / "ESP32/src/esp32_sdl.cpp").read_text()
    hdr = (ROOT / "ESP32/include/SDL.h").read_text()
    for live in (
        "SDL_CreateTextureFromSurface(", 
        "SDL_UpdateTexture(",  "SDL_RenderPresent(",
        "SDL_RenderSetClipRect(", "SDL_RenderFillRect(",
    ):
        assert live in src and live in hdr, f"SDL-B removed live API: {live}"
    native = (ROOT / "ESP32/src/esp_native_texture.cpp").read_text(encoding="utf-8")
    nativeHeader = (ROOT / "ESP32/include/esp_native_texture.h").read_text(encoding="utf-8")
    for api in ("EspNativeTexture_blit(", "EspNativeTexture_destroy(",
                "EspNativeTexture_setColorMod(", "EspNativeTexture_adoptIndexed("):
        assert api in native and api in nativeHeader, f"SDL-D1 native texture owner missing {api}"
    assert "texture->indexed = true;" in src
    assert "surface->pixels = nullptr;" in src
    assert "free(texture->indexedPixels);" in native
    assert "PlatformVideo_present()" in src
    print(f"SDL-B PASS retired {len(RETIRED)} never-linked control/window/haptic stubs, "
          f"no users across {scanned} native C/C++/headers, active indexed SDL unchanged")

if __name__ == "__main__":
    main()
