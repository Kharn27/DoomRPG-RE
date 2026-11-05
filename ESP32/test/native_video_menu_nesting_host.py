#!/usr/bin/env python3
"""Nested native Options -> VIDEO geometry and navigation contract.
No SDL/ESP32 state, SD writes, or framebuffer allocation is needed.
The real layout header drives an exhaustive pixel hitbox check.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
inc = root / "ESP32/include"
src = root / "ESP32/src"
layout = (inc / "native_main_menu_160x120_layout.h").read_text()
model = (src / "native_main_menu_model.c").read_text()
touch = (src / "native_main_menu_options_back.c").read_text()
paint = (src / "native_main_menu_touch_layout.c").read_text()
action = (src / "native_main_menu_options_action.c").read_text()

assert '#define DOOMRPG_ESP32_OPTIONS_CARD_COUNT 3' in layout
assert '#define DOOMRPG_ESP32_VIDEO_CARD_COUNT 4' in layout
assert all(x in model for x in ('"Back"', '"Video"', '"Sound"',
                                'return menuSystem->numItems == DOOMRPG_ESP32_OPTIONS_CARD_COUNT;'))
assert '"Orientation", 0' not in model[model.index("static int buildFixedOptionsModel("):
                                    model.index("static int buildBoundedHelpModel(")]
assert 'const char* expectedOptionsItems[OPTIONS_ITEM_COUNT]' in action
assert '"Back", "Video", "Sound"' in action
assert 'DoomRPG_esp32OptionsCardRect(item, &left, &top, &right, &bottom);' in touch
assert 'DoomRPG_esp32OptionsCardRect(item, &left, &top, &right, &bottom);' in paint
assert 'DoomRPG_esp32VideoCardRect(item, &l, &t, &r, &b);' in touch
assert 'DoomRPG_esp32VideoCardRect(item, &l, &t, &r, &b);' in paint
assert 'DoomRPG_esp32PaintVideoMenuCards(' in paint
assert '"BACK", "LUMINOSITE", "ORIENTATION", "THEME"' in paint
assert 'mask |= (1U << OPTIONS_VIDEO_ITEM);' in action
assert 'DoomRPG_esp32PaintMainMenuVideoDashboardSelection(' in action

# Distinct pages, no unreachable root Sound, and no direct child->root Back.
route = touch[touch.index("static void optionsBackTap("):
              touch.index("int DoomRPG_esp32OptionsBackActivate(")]
assert route.index("if (luminositeOpen) {") < route.index("if (orientationOpen) {")
assert route.index("if (orientationOpen) {") < route.index("if (themeOpen) {")
assert route.index("if (themeOpen) {") < route.index("if (videoOpen) {")
assert 'luminositeOpen ? luminositeItemAt' in route
assert '(videoOpen ? videoItemAt(screenX, screenY)' in route
assert route.count("if (!paintVideoState(VIDEO_BACK_ITEM, 0))") >= 4
assert 'if (armedItem != item)' in route
assert 'videoOpen = 1U;' in route
assert 'videoOpen = 0U;' in route
assert 'if (item != OPTIONS_BACK_ITEM && item != OPTIONS_VIDEO_ITEM)' in route
assert 'printf("[OPTIONSVIDEO] BACK to=Options' in route
assert 'printf("[OPTIONSVIDEO] OPEN from=Options' in route
assert 'printf("[OPTIONSBRIGHTNESS] BACK to=VIDEO' in route
assert 'printf("[OPTIONSORIENTATION] BACK to=VIDEO' in route
assert 'printf("[OPTIONSTHEME] BACK to=VIDEO' in route
assert 'EspNativeConfig_saveTheme(after)' in route
assert 'EspNativeConfig_saveOrientation(after)' in route
assert 'EspNativeConfig_saveBrightness(after)' in route
assert 'DoomRPG_esp32MainMenuModelEnter' not in route
assert 'DoomRPG_esp32MainMenuReturnToMain(' in route
assert 'videoOpen != 0U || luminositeOpen != 0U' in touch

code = r"""
#include <assert.h>
#include <stdio.h>
#include "native_main_menu_160x120_layout.h"
typedef void (*rect_fn)(int, int*, int*, int*, int*);
static void inspect(rect_fn rect, int count, int firstTop, int pitch) {
    int previousBottom = -1;
    for (int i=0; i<count; ++i) {
        int l,t,r,b;
        rect(i,&l,&t,&r,&b);
        assert(l == 104 && r == 157 && t == firstTop+i*pitch);
        assert(b == t+19 && t > 4 && b < 116);
        assert(i == 0 || t - previousBottom >= 5);
        previousBottom = b;
    }
    /* Exhaustively compare each actual card rectangle to the generated
     * logical touch coordinates (physical pixel / 2). Every slot unique. */
    for (int py=0;py<240;++py) {
        for (int px=0;px<320;++px) {
            int x=px/2, y=py/2, match=-1;
            for (int i=0;i<count;++i) {
                int l,t,r,b;
                rect(i,&l,&t,&r,&b);
                if (x>=l && x<=r && y>=t && y<=b) {
                    assert(match == -1);
                    match=i;
                }
            }
            assert(x<104 || x>157 ? match == -1 : 1);
            if (match>=0) assert(x>=104 && x<=157 && y>4 && y<116);
        }
    }
}
int main(void) {
    assert(DOOMRPG_ESP32_OPTIONS_CARD_COUNT == 3);
    assert(DOOMRPG_ESP32_VIDEO_CARD_COUNT == 4);
    inspect(DoomRPG_esp32OptionsCardRect, 3, 19, 32);
    inspect(DoomRPG_esp32VideoCardRect, 4, 12, 26);
    puts("PASS VIDEO nested Options hitboxes=3/4 160x120 parity, no overlap");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-video-nested-") as temp:
    source = Path(temp) / "layout.c"
    binary = Path(temp) / "layout"
    source.write_text(code)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I", str(inc), str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS native VIDEO root/child/back contracts, SD intents and fail-closed SOUND")
