#!/usr/bin/env python3
"""SDL-F5: compile the same Image_t in desktop, ESP32 and geometry-oracle C.

Verify that source and layout are unchanged for legacy, with a native
pointer type in firmware and no SDL dependency in the texture bridge.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
header = (ROOT / "src/DoomRPG.h").read_text(encoding="latin-1")
bridge = (ROOT / "ESP32/include/esp_native_texture_bridge.h").read_text()
image = (ROOT / "ESP32/src/esp_doomrpg_image_resources.c").read_text()
native = (ROOT / "ESP32/include/esp_native_texture.h").read_text()
ini = (ROOT / "ESP32/platformio.ini").read_text()

guard = "#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)"
assert header.count(guard) == 2
assert "EspNativeTexture* imgBitmap;" in header
assert "SDL_Texture* imgBitmap;" in header
assert '#include "esp_native_texture.h"' in header
assert '#include "esp_native_texture.h"' in bridge
active_bridge = re.sub(r"/\*[\s\S]*?\*/", "", bridge)
assert "SDL_" not in active_bridge and "<SDL.h>" not in bridge
assert "EspNativeTexture* EspNativeTexture_adoptIndexed(" in active_bridge
assert "EspNativeTexture* EspNativeTexture_adoptIndexed(" in native
assert "EspNativeTexture* texture;" in image
assert "img->imgBitmap = texture;" in image
assert "EspNativeTexture_destroy(image->imgBitmap);" in image
assert "-<esp32_sdl.cpp>" in ini

harness = r'''
#include <stddef.h>
#include <stdio.h>
#include <SDL.h>
#include "DoomRPG.h"
#include "esp_native_texture_bridge.h"
_Static_assert(offsetof(Image_t, imgBitmap) == 0, "image pointer first");
_Static_assert(sizeof(((Image_t*)0)->imgBitmap) == sizeof(void*),
               "one unchanged pointer");
_Static_assert(sizeof(((Image_t*)0)->width) == sizeof(int), "width");
_Static_assert(sizeof(((Image_t*)0)->height) == sizeof(int), "height");
_Static_assert(sizeof(((Image_t*)0)->isTransparentMask) == sizeof(boolean),
               "mask");
#if defined(DOOMRPG_ESP32) && !defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)
_Static_assert(__builtin_types_compatible_p(
    __typeof__(((Image_t*)0)->imgBitmap), EspNativeTexture*),
    "native pointer ABI");
#else
_Static_assert(__builtin_types_compatible_p(
    __typeof__(((Image_t*)0)->imgBitmap), SDL_Texture*),
    "desktop/oracle pointer ABI");
#endif
int main(void) {
    printf("%zu/%zu/%zu/%zu\n", sizeof(Image_t),
        offsetof(Image_t, width), offsetof(Image_t, height),
        offsetof(Image_t, isTransparentMask));
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="sdl-f5-image-abi-") as tmp:
    source = Path(tmp) / "abi.c"
    source.write_text(harness)
    outputs = []
    modes = ([], ["-DDOOMRPG_ESP32=1"],
             ["-DDOOMRPG_ESP32=1", "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"])
    for n, flags in enumerate(modes):
        exe = Path(tmp) / ("abi" + str(n))
        cmd = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-IESP32/include", "-Isrc"] + flags + [str(source), "-o", str(exe)]
        subprocess.run(cmd, cwd=ROOT, check=True)
        outputs.append(subprocess.check_output([str(exe)], text=True).strip())
    assert len(set(outputs)) == 1, "Image_t layout differs: " + repr(outputs)
print("SDL-F5 PASS native Image_t + SDL-free texture bridge;"
      " desktop and geometry oracle ABI/layout preserved: " + outputs[0])
