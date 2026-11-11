#!/usr/bin/env python3
"""SDL-F2: prove all production calls route directly through the existing C
platform-video bridge, without touching presentation cadence, RGB565 or TFT.
The historical SDL_RenderPresent body remains unlinked in the host oracle.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
EXPECTED = {
    "esp_doomrpg_presentation.c": 1,
    "native_main_menu_actions.c": 2,
    "native_main_menu_options_action.c": 1,
    "native_main_menu_options_back.c": 7,  # Sound volume child added one native present
    "native_main_menu_touch.c": 2,
    "native_main_menu_touch_layout.c": 1,
}
for path in SRC.rglob("*"):
    if path.suffix not in (".c", ".cpp"):
        continue
    content = path.read_text(encoding="utf-8")
    if path.name == "esp32_sdl.cpp":
        assert "void SDL_RenderPresent(SDL_Renderer*)" in content
        assert "if (!PlatformVideo_present())" in content
        continue  # historical host oracle, not production linked
    # Remove comments and literals only for the legacy-call inventory.
    code = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", content)
    assert not re.search(r"\bSDL_RenderPresent\s*\(", code), path

for name, count in EXPECTED.items():
    content = (SRC / name).read_text(encoding="utf-8")
    assert content.count("(void)Esp32PlatformVideo_present();") == count, name
    assert '#include "platform_video_c_bridge.h"' in content, name

bridge = (SRC / "platform_video_c_bridge.cpp").read_text(encoding="utf-8")
header = (ROOT / "ESP32/include/platform_video_c_bridge.h").read_text()
display = (SRC / "platform_video.cpp").read_text(encoding="utf-8")
assert "int Esp32PlatformVideo_present(void)" in header
assert "return PlatformVideo_present() ? 1 : 0;" in bridge
assert "bool PlatformVideo_present()" in display
assert "uint16_t outputRow[DOOMRPG_PHYSICAL_WIDTH];" in display
assert "DOOMRPG_INTEGER_SCALE" in display
assert "platformDisplay->pushPixels(outputRow, DOOMRPG_PHYSICAL_WIDTH);" in display
assert len(EXPECTED) == 6 and sum(EXPECTED.values()) == 14
print("SDL-F2 PASS 14 production present sites including Sound -> native C video bridge;"
      " historical SDL oracle retained, real TFT presenter untouched")
