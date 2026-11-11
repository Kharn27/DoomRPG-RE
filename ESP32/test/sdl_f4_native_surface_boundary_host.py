#!/usr/bin/env python3
"""SDL-F4: two native ABI adapters use native local types, not SDL aliases.

Historical oracle types remain in the desktop sources, the host fixtures,
and the transitional SDL.h compatibility typedefs (not a firmware dependency).
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
src = ROOT / "ESP32/src"
present = (src / "esp_doomrpg_presentation.c").read_text()
images = (src / "esp_doomrpg_image_resources.c").read_text()
texture = (ROOT / "ESP32/include/esp_native_texture.h").read_text()
legacy = (ROOT / "ESP32/include/SDL.h").read_text()
ini = (ROOT / "ESP32/platformio.ini").read_text()
diff = (ROOT / "ESP32/test/doomrpg_presentation_host.py").read_text()

# SDL-F7 removed the presentation include. SDL-F10 also removes the
# image adapter's unused include; its full pre-F10 source is FNV-locked
# by the inverse-header oracle, while SDL-C1 keeps the real function bodies.
assert "#include <SDL.h>" not in present
assert "#include <SDL.h>" not in images
for path, text in (("presentation", present), ("image owner", images)):
    assert '#include "SDL_Video.h"' not in text, path
    stripped = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", text)
    assert not re.search(r"\bSDL_\w*", stripped), path

assert "EspNativeRect clip;" in present and "EspNativeRect rect;" in present
assert "SDL_Rect clip;" not in present and "SDL_Rect rect;" not in present
assert "EspNativeTexture_setClipRect(&clip);" in present
assert "EspNativeTexture_fillRect(&rect);" in present
assert "EspNativeTexture_drawLine(sx, sy, ex, ey);" in present
assert "(void)Esp32PlatformVideo_present();" in present
assert "EspNativeTexture* texture;" in images
assert "SDL_Texture* texture;" not in images
assert "texture = EspNativeTexture_adoptIndexed(&decoded);" in images
assert "img->imgBitmap = texture;" in images
assert "EspNativeTexture_destroy(image->imgBitmap);" in images
assert 'typedef EspNativeRect SDL_Rect;' in legacy
assert 'typedef EspNativeTexture SDL_Texture;' in legacy
assert "typedef struct EspNativeRect_s {" in texture
assert "typedef struct EspNativeTexture_s {" in texture
assert 'expected.replace("SDL_Rect clip;", "EspNativeRect clip;")' in diff
assert 'expected.replace("SDL_Rect rect;", "EspNativeRect rect;")' in diff
assert "-<esp32_sdl.cpp>" in ini
print("SDL-F4 PASS native locals use EspNativeRect/EspNativeTexture;"
      " active SDL calls absent, legacy SDL.h type ABI explicit, pixel oracle pinned")
