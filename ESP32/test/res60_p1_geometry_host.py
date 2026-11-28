#!/usr/bin/env python3
"""RES60-P1: exact C11 NN mapping, strict profile isolation (not a 3D PASS)."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
video = (root / "ESP32/src/platform_video.cpp").read_text()
main = (root / "ESP32/src/main.cpp").read_text()
ini = (root / "ESP32/platformio.ini").read_text()
config = (root / "ESP32/include/platform_video_config.h").read_text()
hud = (root / "ESP32/src/esp_native_gameplay_hud.c").read_text()
sprite = (root / "ESP32/src/esp_native_sprite_renderer.c").read_text()
menu = (root / "ESP32/src/native_main_menu_touch_layout.c").read_text()

# All gameplay-side 160x120 contracts remain enabled. This milestone must
# never fake 60% 3D by rescaling an existing 160x120 frame.
assert "#define DOOMRPG_LOGICAL_WIDTH 160" in config
assert "#define DOOMRPG_LOGICAL_HEIGHT 120" in config
assert '#error "Classic CYD gameplay viewport must remain 160x80"' in config
assert '#error "The first native gameplay HUD contract is defined only for 160x120"' in hud
assert '#define SCREEN_W 160' in sprite and '#define SCREEN_H 80' in sprite
assert '#error "The ESP32 touch-ready main menu is defined only for 160x120"' in menu
assert 'default_envs = esp32-cyd' in ini
assert '[env:esp32-cyd-res60]' in ini
assert 'DOOMRPG_RES60_P1_DIAGNOSTIC=1' in ini
assert 'extends = env:esp32-cyd' in ini

# Real single framebuffer allocation: capacity changes only for the explicit
# pilot. Physical 320px LCD row is the sole bounded presentation scratch.
assert 'static_cast<uint16_t*>(calloc(kPixelCount, sizeof(uint16_t)))' in video
assert 'uint16_t outputRow[DOOMRPG_PHYSICAL_WIDTH];' in video
assert 'EspVideoNearest_source(' in video
assert 'framebuffer[(size_t)y * DOOMRPG_VIDEO_BUFFER_WIDTH + x] = color;' in video
assert '#if DOOMRPG_RES60_P1_DIAGNOSTIC' in video
assert '    for (int sourceY = 0; sourceY < DOOMRPG_LOGICAL_HEIGHT; ++sourceY)' in video
assert '            outputRow[outputX + 1] = color;' in video

# Render and SD/music owners cannot enter the probe path.
setup = main[main.index("void setup()"):main.index("void loop()")]
assert setup.index('    initializePlatformVideo();') < setup.index('#if DOOMRPG_RES60_P1_DIAGNOSTIC') < setup.index('    initializeSdCard();')
assert '    PlatformVideo_showTestPattern();' in setup
assert 'AUDIO=NOT_STARTED' in setup
assert 'map=NOT_LOADED' in setup
assert '    return;\n#endif\n    initializeSdCard();' in setup
assert '    delay(250);\n#endif\n    return;\n#else' in main[main.index("void loop()"):]

c_code = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "platform_video_config.h"
#include "esp_video_nearest.h"

_Static_assert(DOOMRPG_LOGICAL_WIDTH == 160, "world base width stays 160");
_Static_assert(DOOMRPG_LOGICAL_HEIGHT == 120, "world base height stays 120");
_Static_assert(DOOMRPG_VIEWPORT_HEIGHT == 80, "world base viewport stays 80");
#if DOOMRPG_RES60_P1_DIAGNOSTIC
_Static_assert(DOOMRPG_VIDEO_BUFFER_WIDTH == 192, "pilot width");
_Static_assert(DOOMRPG_VIDEO_BUFFER_HEIGHT == 144, "pilot height");
_Static_assert(DOOMRPG_VIDEO_BUFFER_WIDTH * DOOMRPG_VIDEO_BUFFER_HEIGHT * 2 == 55296, "single 55KiB framebuffer");
#else
_Static_assert(DOOMRPG_VIDEO_BUFFER_WIDTH == 160, "production buffer width");
_Static_assert(DOOMRPG_VIDEO_BUFFER_HEIGHT == 120, "production buffer height");
_Static_assert(DOOMRPG_VIDEO_BUFFER_WIDTH * DOOMRPG_VIDEO_BUFFER_HEIGHT * 2 == 38400, "production buffer size");
#endif

static void axis(unsigned src, unsigned dst) {
    unsigned counts[320] = {0};
    assert(src <= 320 && dst <= 320);
    for (unsigned d = 0; d < dst; ++d) {
        uint16_t s = EspVideoNearest_source(d, src, dst);
        assert(s == (uint16_t)(d * src / dst));
        assert(s < src);
        if (d > 0) assert(s >= EspVideoNearest_source(d - 1, src, dst));
        counts[s]++;
    }
    assert(EspVideoNearest_source(0, src, dst) == 0);
    assert(EspVideoNearest_source(dst - 1, src, dst) == src - 1);
    for (unsigned i = 0; i < src; ++i) {
        assert(counts[i] >= 1 && counts[i] <= 2);
    }
}
int main(void) {
    axis(160, 320);
    axis(120, 240);
    axis(192, 320);
    axis(144, 240);
    assert(EspVideoNearest_source(0, 0, 320) == UINT16_MAX);
    assert(EspVideoNearest_source(320, 192, 320) == UINT16_MAX);
    assert(EspVideoNearest_source(0, 192, 0) == UINT16_MAX);
    for (unsigned x = 0; x < 320; ++x) {
        assert(EspVideoNearest_source(x, 160, 320) == x / 2);
    }
    for (unsigned y = 0; y < 240; ++y) {
        assert(EspVideoNearest_source(y, 120, 240) == y / 2);
    }
    puts("PASS 50% parity + 60% fractional exact coverage, bounded output");
    return 0;
}
"""
with tempfile.TemporaryDirectory() as temp:
    cpath = Path(temp) / "res60_geometry.c"
    binary = Path(temp) / "res60_geometry"
    cpath.write_text(c_code)
    for diagnostic in (0, 1):
        subprocess.run(
            ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             f"-DDOOMRPG_RES60_P1_DIAGNOSTIC={diagnostic}",
             "-I", str(root / "ESP32/include"), str(cpath), "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
print("RES60-P1 HOST PASS: 50/60 geometry, no game alias, safety guards retained")
