#!/usr/bin/env python3
"""RES60-P2 geometry gate: native 24/96/24 bands, no 160x80 renderer alias."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
config = (ROOT / "ESP32/include/platform_video_config.h").read_text()
layout = (ROOT / "ESP32/include/esp_res60_layout.h").read_text()
main = (ROOT / "ESP32/src/main.cpp").read_text()
video = (ROOT / "ESP32/src/platform_video.cpp").read_text()
ini = (ROOT / "ESP32/platformio.ini").read_text()

assert "#define DOOMRPG_LOGICAL_WIDTH 160" in config
assert "#define DOOMRPG_LOGICAL_HEIGHT 120" in config
assert "#define DOOMRPG_VIEWPORT_HEIGHT" in config
assert '#error "Classic CYD gameplay viewport must remain 160x80"' in config
assert '#error "RES60-P2 layout requires isolated RES60 video-only admission"' in config
assert "-D DOOMRPG_RES60_P2_LAYOUT=1" in ini
assert "extends = env:esp32-cyd" in ini
assert "ESP_RES60_WORLD_ROWS 96U" in layout
assert "ESP_RES60_HUD_TOP_ROWS 24U" in layout
assert "ESP_RES60_HUD_BOTTOM_ROWS 24U" in layout
assert 'EspRes60Layout_paintDiagnostic(framebuffer, kFramebufferBytes)' in video
assert "PlatformVideo_showRes60LayoutPattern();" in main
setup = main[main.index("void setup()"):main.index("void loop()")]
# P3 is a separate gated PAK-backed preview path. P2 itself still starts
# no SD/PAK: it remains in the #elif arm and returns before normal boot.
assert '#elif DOOMRPG_RES60_P2_LAYOUT' in setup
p2_arm = setup.split('#elif DOOMRPG_RES60_P2_LAYOUT', 1)[1].split('#else', 1)[0]
assert 'PlatformVideo_showRes60LayoutPattern();' in p2_arm
assert 'initializeSdCard();' not in p2_arm
assert "AUDIO=NOT_STARTED" in setup and "map=NOT_LOADED" in setup
assert "world3d=NOT_STARTED" in video
assert "uint16_t outputRow[DOOMRPG_PHYSICAL_WIDTH];" in video
assert "EspVideoNearest_source(" in video

code = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "platform_video_config.h"
#include "esp_res60_layout.h"
#include "esp_video_nearest.h"

_Static_assert(ESP_RES60_WIDTH == 192, "true 192 px");
_Static_assert(ESP_RES60_HEIGHT == 144, "true 144 px");
_Static_assert(ESP_RES60_WORLD_ROWS == 96, "true 96 row world region");
_Static_assert(ESP_RES60_HUD_TOP_ROWS == 24, "top region");
_Static_assert(ESP_RES60_HUD_BOTTOM_ROWS == 24, "bottom region");
_Static_assert(ESP_RES60_FRAMEBUFFER_BYTES == 55296, "one RGB565 framebuffer");
_Static_assert(DOOMRPG_LOGICAL_WIDTH == 160, "production geometry unchanged");
_Static_assert(DOOMRPG_VIEWPORT_HEIGHT == 80, "production world unchanged");
#if DOOMRPG_RES60_P2_LAYOUT
_Static_assert(DOOMRPG_RES60_P1_DIAGNOSTIC == 1, "only video pilot");
_Static_assert(DOOMRPG_VIDEO_BUFFER_WIDTH == 192, "pilot only");
#else
_Static_assert(DOOMRPG_VIDEO_BUFFER_WIDTH == 160, "normal only");
#endif

int main(void) {
    uint16_t data[ESP_RES60_WIDTH * ESP_RES60_HEIGHT + 2U] = {0};
    size_t count = ESP_RES60_WIDTH * ESP_RES60_HEIGHT;
    data[0] = 0xbabeU;
    data[count + 1U] = 0xbeefU;
    assert(!EspRes60Layout_paintDiagnostic(NULL, ESP_RES60_FRAMEBUFFER_BYTES));
    assert(!EspRes60Layout_paintDiagnostic(data + 1U, ESP_RES60_FRAMEBUFFER_BYTES - 2U));
    assert(data[1] == 0U);
    assert(EspRes60Layout_paintDiagnostic(data + 1U, ESP_RES60_FRAMEBUFFER_BYTES));
    assert(data[0] == 0xbabeU && data[count + 1U] == 0xbeefU);
    assert(EspRes60Layout_fnv(NULL, ESP_RES60_FRAMEBUFFER_BYTES) == 0U);
    assert(EspRes60Layout_fnv(data + 1U, ESP_RES60_FRAMEBUFFER_BYTES) == UINT32_C(0x9efd2d18));
    unsigned counts[4] = {0};
    for (int y = -1; y <= 144; ++y) {
        for (int x = -1; x <= 192; ++x) {
            EspRes60Region region = EspRes60Layout_regionAt(x, y);
            if (x < 0 || x >= 192 || y < 0 || y >= 144) {
                assert(region == ESP_RES60_OUTSIDE);
            } else {
                counts[region]++;
            }
        }
    }
    assert(counts[ESP_RES60_TOP_HUD] == 192U * 24U);
    assert(counts[ESP_RES60_WORLD] == 192U * 96U);
    assert(counts[ESP_RES60_BOTTOM_HUD] == 192U * 24U);
    assert(data[1U + 23U * 192U + 27U] == UINT16_C(0xffff));
    assert(data[1U + 120U * 192U + 27U] == UINT16_C(0xffff));
    assert(data[1U + 24U * 192U + 96U] == UINT16_C(0xffff));
    assert(data[1U + 72U * 192U + 96U] == UINT16_C(0xffff));
    /* Physical nearest 5/3 produces exactly 40/160/40 LCD rows. */
    for (unsigned py = 0; py < 240U; ++py) {
        uint16_t logicalY = EspVideoNearest_source(py, 144U, 240U);
        EspRes60Region r = EspRes60Layout_regionAt(0, logicalY);
        assert(r == (py < 40U ? ESP_RES60_TOP_HUD :
                     py < 200U ? ESP_RES60_WORLD : ESP_RES60_BOTTOM_HUD));
    }
    for (unsigned px = 0; px < 320U; ++px) {
        assert(EspVideoNearest_source(px, 192U, 320U) < 192U);
    }
    puts("RES60-P2 C11 PASS 24/96/24, FNV, fail-closed, 320x240 mapping");
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="res60-p2-") as tmp:
    path = Path(tmp) / "check.c"
    executable = Path(tmp) / "res60-p2"
    path.write_text(code)
    for p1, p2 in ((0, 0), (1, 1)):
        compile_args = [
            "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
            f"-DDOOMRPG_RES60_P1_DIAGNOSTIC={p1}",
            f"-DDOOMRPG_RES60_P2_LAYOUT={p2}",
            "-I", str(ROOT / "ESP32/include"),
            str(path), "-o", str(executable),
        ]
        subprocess.run(compile_args, check=True)
        subprocess.run([str(executable)], check=True)
    # Invalid admission must produce a hard compile error, not 160x120 alias.
    rejected = subprocess.run(
        ["cc", "-std=c11", "-DDOOMRPG_RES60_P1_DIAGNOSTIC=0",
         "-DDOOMRPG_RES60_P2_LAYOUT=1", "-I", str(ROOT / "ESP32/include"),
         str(path), "-o", str(executable)],
        capture_output=True, text=True,
    )
    assert rejected.returncode != 0
    assert "RES60-P2 layout requires isolated" in rejected.stderr
print("RES60-P2 HOST PASS: bounded native raster and profile fence")
