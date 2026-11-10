#!/usr/bin/env python3
"""SDL-D1: real draw/modulate/destructor migrated to permanent native texture.

This is a source ownership gate, not a pixel test: the real C++ pipeline and
RGB565 parity/clip/OOM are executed by sdl_shim_permanent_host.py.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
INC = ROOT / "ESP32/include"
LEGACY = ("SDL_RenderCopy", "SDL_DestroyTexture", "SDL_SetTextureColorMod")
NATIVE = ("EspNativeTexture_blit(", "EspNativeTexture_destroy(",
          "EspNativeTexture_setColorMod(", "EspNativeTexture_adoptIndexed(",
          "EspNativeTexture_setClipRect(")


def code_only(source):
    return re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", source)


def main():
    examined = 0
    for folder in (SRC, INC):
        for path in folder.rglob("*"):
            if path.suffix not in (".c", ".cpp", ".h", ".hpp"):
                continue
            examined += 1
            data = code_only(path.read_text(encoding="utf-8", errors="replace"))
            for api in LEGACY:
                assert not re.search(r"\b" + api + r"\s*\(", data), (
                    f"SDL-D1 abandoned live texture call/definition {api} in {path}"
                )

    header = (INC / "esp_native_texture.h").read_text()
    native = (SRC / "esp_native_texture.cpp").read_text()
    shim = (SRC / "esp32_sdl.cpp").read_text()
    sdl_header = (INC / "SDL.h").read_text()
    for api in NATIVE:
        assert api in header and api in native, f"SDL-D1 missing native API: {api}"
    assert "typedef EspNativeTexture SDL_Texture;" in sdl_header
    assert "typedef EspNativeRect SDL_Rect;" in sdl_header
    assert "struct SDL_Texture {" not in shim
    assert "EspNativeTexture_setClipRect(rect);" in shim
    assert "PlatformVideo_framebuffer()" in native
    assert "PlatformVideo_present()" in shim
    assert "SDL_RenderSetClipRect(" in shim
    assert "SDL_RenderDrawLine(" in shim
    assert "SDL_RenderFillRect(" in shim
    assert "SDL_RenderPresent(" in shim
    assert "static uint16_t framebuffer[" not in native
    assert "free(texture->indexedPixels);" in native
    assert "image->pixels = nullptr;" in native
    assert "image->palette = nullptr;" in native
    for filename, methods in (
        ("esp_native_canvas_text.c", ("EspNativeTexture_blit(", "EspNativeTexture_setColorMod(")),
        ("native_story_fit.c", ("EspNativeTexture_blit(", "EspNativeTexture_setColorMod(")),
        ("native_main_menu_actions.c", ("EspNativeTexture_blit(",)),
        ("native_main_menu_options_action.c", ("EspNativeTexture_blit(",)),
        ("esp_doomrpg_image_resources.c", ("EspNativeTexture_destroy(",)),
    ):
        body = (SRC / filename).read_text()
        for method in methods:
            assert method in body, f"SDL-D1 live native consumer missing: {filename}:{method}"
    print(f"SDL-D1 PASS {examined} source/headers scanned, 3 live SDL texture "
          "exports retired, packed native image lifecycle + renderer/palette "
          "and clip producers are explicit and bounded")


if __name__ == "__main__":
    main()
