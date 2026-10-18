#!/usr/bin/env python3
"""Exercise actual password block-caret functions + menu geometry contracts.

The production units are still compiled by PlatformIO for the three CYD
profiles; this host regression probes timing, frame boundaries and retry.
It cannot replace visual acceptance on the physical display.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / "ESP32/include"
SRC = ROOT / "ESP32/src"
menu = (SRC / "native_main_menu_touch_layout.c").read_text()
layout = (INC / "native_main_menu_160x120_layout.h").read_text()
touch = (SRC / "native_main_menu_touch.c").read_text()
password = (SRC / "esp_native_gameplay_password.c").read_text()
resident = (SRC / "esp_native_resident_gameplay.c").read_text()
load = (SRC / "native_main_menu_actions.c").read_text()
options = (SRC / "native_main_menu_options_back.c").read_text()
options_painter = (SRC / "native_main_menu_options_action.c").read_text()
video = (SRC / "platform_video.cpp").read_text()
backlight_header = (INC / "esp_native_backlight.h").read_text()
platformio = (ROOT / "ESP32/platformio.ini").read_text()

def macro(src, name):
    m = re.search(r"^#define\s+" + re.escape(name) + r"\s+([0-9xa-fA-FU]+)", src, re.M)
    assert m, name
    return int(m.group(1).rstrip("Uu"), 0)

def function(src, signature):
    start = src.index(signature)
    brace = src.index("{", start)
    depth = 0
    for end in range(brace, len(src)):
        if src[end] == "{":
            depth += 1
        elif src[end] == "}":
            depth -= 1
            if depth == 0:
                return src[start:end + 1]
    raise AssertionError("Unclosed native function " + signature)

# Native 160x120 split geometry. Test the *shared* contract instead of
# duplicating a second touch-hit table in the test suite.
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_X") == 0
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y") == 0
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH") == 102
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT") == 120
assert (macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y") +
        macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT")) == 120
# Intentional anisotropic nearest-neighbor scaling rather than fitted aspect ratio.
assert (macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH") *
        macro(layout, "DOOMRPG_ESP32_MAIN_MENU_LOGO_SRC_HEIGHT")) != (
        macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT") *
        macro(layout, "DOOMRPG_ESP32_MAIN_MENU_LOGO_SRC_WIDTH"))
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_LEFT") == 102
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_RIGHT") == 159
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT") == 104
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT") == 157
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_FIRST_TOP") == 13
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_HEIGHT") == 20
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_PITCH") == 25
assert "DoomRPG_esp32MainMenuCardRect" in layout
assert menu.count("DoomRPG_esp32MainMenuCardRect(") == 1
assert touch.count("DoomRPG_esp32MainMenuCardRect(") == 1
assert "return findHitItem(screenX / DOOMRPG_INTEGER_SCALE" in touch
assert "DoomRPG_esp32MainMenuCardRect(item, &left, &top, &right, &bottom)" in options
assert "DoomRPG_esp32MainMenuCardRect(OPTIONS_BACK_ITEM" in options
assert "DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_LEFT" not in options
assert "DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP" not in options
assert "cardRect(item, &left, &top, &right, &bottom)" in touch
assert "DOOMRPG_ESP32_MAIN_MENU_CARD_COL0" not in touch
assert "DOOMRPG_ESP32_MAIN_MENU_CARD_COL1" not in touch
assert "DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0" not in touch
assert "DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1" not in menu
assert "focused = selected && armed && enabled" in menu
card = function(menu, "static void drawDashboardCard(")
assert "MAIN_DOOM_RED_HOT" in card and "ESP_HUB_COLOR_AMBER" not in card
assert card.count("drawRect565(") == 1  # no orange/white nested ring
drawCards = function(menu, "int DoomRPG_esp32PaintMenuDashboardCards(")
assert "DASH_RAIL_LEFT" in drawCards and "DASH_RAIL_RIGHT" in drawCards
assert "DASH_RAIL_TOP" in drawCards and "DASH_RAIL_BOTTOM" in drawCards
drawMain = function(menu, "static int drawTouchReadyMainMenuOpaque(")
assert "drawRect565(" not in drawMain
assert "fillRect565(" not in drawMain
assert "logoDst.x = DOOMRPG_CANVAS_X + DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_X" in drawMain
assert "logo=102x120@0,0 stretched=yes unframed=yes" in menu
# Regression: Options previously centered the full-height logo at x=29,
# painting beneath the right rail and creating a blank strip on the left.
# Both compositions must use the same logical origin and destination size.
paintOptions = function(options_painter, "static int paintOptionsBounded(")
logoFields = (
    "logoDst.x = DOOMRPG_CANVAS_X + DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_X;",
    "logoDst.y = DOOMRPG_CANVAS_Y +",
    "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y;",
    "logoDst.w = DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH;",
    "logoDst.h = DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT;",
)
for field in logoFields:
    assert field in paintOptions and field in drawMain, field
assert "DOOMRPG_CANVAS_WIDTH -" not in paintOptions
assert "DoomRPG_esp32PaintMainMenuOptionsDashboard(" in paintOptions

# Classic CYD GPIO21 backlight owner stays entirely separate from RGB565.
assert "-D TFT_BL=21" in platformio
assert "-D TFT_BACKLIGHT_ON=HIGH" in platformio
assert "#include \"esp_native_backlight.h\"" in video
assert "#if !defined(TFT_BL) || TFT_BL != 21" in video
assert "ledcSetup(kBacklightChannel, kBacklightFrequencyHz" in video
assert "ledcAttachPin(TFT_BL, kBacklightChannel)" in video
assert "ledcWrite(kBacklightChannel, duty)" in video
assert "Esp32Backlight_setPercent(uint8_t percent)" in backlight_header
assert "percent != 25U && percent != 50U" in video
assert "percent != 75U && percent != 100U" in video
assert "const uint32_t duty = ((uint32_t)percent * 255U + 50U) / 100U;" in video
assert "framebuffer[" not in function(video, 'extern "C" int Esp32Backlight_setPercent(')

# Luminosité is a real child screen. Root keeps the original 2-tap navigation
# while the five child presets are directly selectable with one released tap.
assert "DoomRPG_esp32PaintMainMenuOptionsDashboardSelection(" in options_painter
assert '"LUMINOSITE"' in options_painter
assert '"BRT %u"' not in options_painter
assert 'strcmp(label, "LUMINOSITE") == 0' in menu
smallLabel = function(menu, "static void drawLuminositeLabel(")
assert "static const uint8_t glyphs[10][7]" in smallLabel
assert "putPixel(render, x + ch * 5 + col, y + row, color)" in smallLabel
assert "Esp32Backlight_isReady()" in options_painter
assert "DoomRPG_esp32PaintBrightnessPresetCards(" in options_painter
presets = function(menu, "int DoomRPG_esp32PaintBrightnessPresetCards(")
assert '"BACK", "25%", "50%", "75%", "100%"' in presets
assert "DoomRPG_esp32BrightnessCardRect(item," in presets
assert "DASH_RAIL_LEFT" in presets and "DASH_RAIL_RIGHT" in presets
assert "item != 0 && activePercent == percent" in presets
assert "DoomRPG_esp32MainMenuCardRect(item, &left, &top, &right, &bottom)" in options
assert "DoomRPG_esp32BrightnessCardRect(item, &left, &top, &right, &bottom)" in options
assert "luminositeOpen ? luminositeItemAt" in options
assert "if (luminositeOpen)" in options
assert "if (armedItem != item)" in options
assert "if (item == OPTIONS_VIDEO_ITEM)" in options
assert "luminositeOpen = 1U;" in options
assert "if (item == 0)" in options
assert "luminositeOpen = 0U;" in options
assert "const uint8_t after = (uint8_t)(item * 25U);" in options
assert "Esp32Backlight_setPercent(after)" in options
assert "if (!paintLuminositeState())" in options
assert "nextBrightnessPercent" not in options
assert "OPTIONSBRIGHTNESS] OPEN" in options
assert "OPTIONSBRIGHTNESS] BACK" in options
assert "DoomRPG_esp32MainMenuReturnToMain(" in options
assert "optionsExpectedFrameFNV" in options
assert "Input/Sound stay fail-closed" in options
assert "logoDst.x" in paintOptions and "SDL_RenderCopy(" in paintOptions

assert 'static char noSaveLabel[] = "NO SAVE";' in load
loadFeedback = function(load, "static void showNoSaveFeedback(")
assert "DoomRPG_esp32MainMenuCardRect(MAIN_LOAD_ITEM_INDEX" in loadFeedback
assert "DoomRPG_esp32MainMenuTouchRebaseFrame(" in loadFeedback
assert len("NO SAVE") * 7 <= (
    macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT") -
    macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT") + 1)
assert "DoomRPG_esp32MainMenuPaintDashboardSelection" in touch
assert "dashboard=split-logo-left+rail-right" in touch
assert "EspNativeGameplayPassword_serviceCursor(" in resident
assert "esp_timer_get_time() / 1000LL" in resident

# Original two red fragments came from these drawCode decorations, not TFT
# gamma or a bus fault. Guard their actual removal and non-overlapping rows.
prompt = function(password, "static int drawPrompt(")
drawCode = function(password, "static int drawCode(")
paint = function(password, "static int paintKeypad(")
assert "int y = 3;" in prompt and "y += 13" in prompt
assert "y <= 16" in prompt
assert "PASSWORD_COLOR_RED" not in drawCode
assert "PASSWORD_COLOR_STEEL_DIM" in drawCode
assert "fillRect(framebuffer, 0, 29, DOOMRPG_LOGICAL_WIDTH, 1," in paint
assert "fillRect(framebuffer, 0, 30, DOOMRPG_LOGICAL_WIDTH, 14," in drawCode
assert "PASSWORD_COLOR_VALID" in paint and "PASSWORD_COLOR_DEL" in paint
assert "row * PASSWORD_KEY_ROW_HEIGHT" in paint
assert "const int labelY = (row == 3 && (col == 0 || col == 2))" in paint
assert "? y + 5 : y + 2;" in paint
assert macro(password, "PASSWORD_CURSOR_Y") == 32
assert macro(password, "PASSWORD_CURSOR_WIDTH") == 5
assert macro(password, "PASSWORD_CURSOR_HEIGHT") == 9

names = ["PASSWORD_FONT_ADVANCE", "PASSWORD_COLOR_RED_HOT", "PASSWORD_COLOR_INPUT",
         "PASSWORD_CURSOR_PERIOD_MS", "PASSWORD_CURSOR_Y",
         "PASSWORD_CURSOR_WIDTH", "PASSWORD_CURSOR_HEIGHT"]
defines = "\n".join("#define %s %s" % (name, hex(macro(password, name)) + "U")
                    for name in names)
code = "\n\n".join(function(password, sig) for sig in (
    "static int codeCursorX(void)",
    "static void paintCursorBlock(",
    "int EspNativeGameplayPassword_serviceCursor(",
))
assert "EspNativeIndexedBmp_" not in code
assert "paintKeypad(" not in code
assert "EspAssetPack_read" not in code

harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define DOOMRPG_LOGICAL_WIDTH 160
#define DOOMRPG_LOGICAL_HEIGHT 120
#include "native_main_menu_160x120_layout.h"

typedef struct {
    uint32_t cursorEpochMs;
    uint16_t expectedLength;
    uint8_t active, packOwned, cursorVisible;
    char entered[17];
} Pw;
static Pw s;
static Pw* passwordState(void) { return &s; }
#define password (*passwordState())
static uint16_t pixels[160 * 120];
static uint32_t presents;
static int presentFails;
static int packOpen = 1;
static int EspAssetPack_isOpen(void) { return packOpen; }
static void* Esp32PlatformVideo_framebuffer(void) { return pixels; }
static size_t Esp32PlatformVideo_framebufferSizeBytes(void) {
    return sizeof(pixels);
}
static int Esp32PlatformVideo_present(void) {
    ++presents;
    return !presentFails;
}
static void fillRect(uint16_t* framebuffer, int x, int y, int w, int h, uint16_t color) {
    assert(x >= 0 && y >= 0 && x+w <= 160 && y+h <= 120);
    for (int yy=y; yy<y+h; ++yy)
        for (int xx=x; xx<x+w; ++xx)
            framebuffer[yy*160+xx] = color;
}
"""
tests = r"""
int main(void) {
    assert(DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_X +
           DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH <=
           DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_LEFT);
    assert(DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y == 0);
    assert(DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y +
           DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT ==
           DOOMRPG_LOGICAL_HEIGHT);
    for (int i = 0; i < 4; ++i) {
        int l, t, r, b;
        DoomRPG_esp32MainMenuCardRect(i, &l, &t, &r, &b);
        assert(l == 104 && r == 157);
        assert(t == 13 + 25 * i && b == 32 + 25 * i);
        assert(t > DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_TOP);
        assert(b < DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_BOTTOM);
        if (i < 3) assert(b < DOOMRPG_ESP32_MAIN_MENU_CARD_TOP(i+1));
    }
    assert(DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT == 5);
    assert(10*5-1 <= DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT -
                     DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT + 1);
    for (int i = 0; i < DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT; ++i) {
        int l, t, r, b;
        DoomRPG_esp32BrightnessCardRect(i, &l, &t, &r, &b);
        assert(l == 104 && r == 157);
        assert(t == 6 + i*22 && b == 25 + i*22);
        assert(t > DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_TOP);
        assert(b < DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_BOTTOM);
        if (i < DOOMRPG_ESP32_BRIGHTNESS_CARD_COUNT-1) {
            assert(b < DOOMRPG_ESP32_BRIGHTNESS_CARD_TOP(i+1));
        }
    }
    s.active = s.packOwned = s.cursorVisible = 1;
    s.expectedLength = 16;
    memset(pixels, 0x55, sizeof(pixels));
    assert(codeCursorX() >= 0 && codeCursorX()+PASSWORD_CURSOR_WIDTH < 160);
    int x = codeCursorX();
    paintCursorBlock(pixels, 1);
    assert(pixels[PASSWORD_CURSOR_Y*160+x] == PASSWORD_COLOR_RED_HOT);
    assert(pixels[PASSWORD_CURSOR_Y*160+x-1] == 0x5555);
    assert(pixels[(PASSWORD_CURSOR_Y-1)*160+x] == 0x5555);
    assert(EspNativeGameplayPassword_serviceCursor(1000) == 1);
    assert(presents == 0 && s.cursorEpochMs == 1000);
    assert(EspNativeGameplayPassword_serviceCursor(1449) == 1);
    assert(presents == 0 && s.cursorVisible == 1);
    assert(EspNativeGameplayPassword_serviceCursor(1450) == 1);
    assert(presents == 1 && s.cursorVisible == 0);
    assert(pixels[PASSWORD_CURSOR_Y*160+x] == PASSWORD_COLOR_INPUT);
    assert(EspNativeGameplayPassword_serviceCursor(1900) == 1);
    assert(presents == 2 && s.cursorVisible == 1);
    assert(pixels[PASSWORD_CURSOR_Y*160+x] == PASSWORD_COLOR_RED_HOT);
    presentFails = 1;
    assert(EspNativeGameplayPassword_serviceCursor(2350) == 1);
    assert(s.cursorVisible == 1 && s.cursorEpochMs == 1900);
    assert(pixels[PASSWORD_CURSOR_Y*160+x] == PASSWORD_COLOR_RED_HOT);
    presentFails = 0;
    assert(EspNativeGameplayPassword_serviceCursor(2350) == 1);
    assert(s.cursorVisible == 0 && s.cursorEpochMs == 2350);
    strcpy(s.entered, "1234567890123456");
    assert(codeCursorX()+PASSWORD_CURSOR_WIDTH < 160);
    s.active = 0;
    assert(EspNativeGameplayPassword_serviceCursor(2800) == 1);
    packOpen = 0;
    s.active = 1;
    assert(EspNativeGameplayPassword_serviceCursor(2800) == 0);
    return 0;
}
"""
cc = shutil.which("cc")
assert cc, "Host C compiler required"
with tempfile.TemporaryDirectory() as d:
    src = Path(d) / "caret.c"
    exe = Path(d) / "caret"
    src.write_text(harness + "\n" + defines + "\n" + code + "\n" + tests)
    subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(INC), str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS native Doom UI: full-height logo, Luminosite 5-card child, keypad caret")
