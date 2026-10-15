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

assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_WIDTH") == 108
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT") == 74
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y") == 0
logo_end = (macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_Y") +
            macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_HEIGHT"))
rail_top = macro(layout, "DOOMRPG_ESP32_MAIN_MENU_DASH_RAIL_TOP")
assert logo_end <= rail_top
assert rail_top < macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_TOP")
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_ROW0_BOTTOM") < macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_TOP")
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_ROW1_BOTTOM") < 120
assert macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_COL0_RIGHT") < macro(layout, "DOOMRPG_ESP32_MAIN_MENU_CARD_COL1_LEFT")
assert "focused = selected && armed && enabled" in menu
card = function(menu, "static void drawDashboardCard(")
assert "MAIN_DOOM_RED_HOT" in card and "ESP_HUB_COLOR_AMBER" not in card
assert card.count("drawRect565(") == 1  # no white+orange nested border
assert "DoomRPG_esp32MainMenuPaintDashboardSelection" in touch
assert "68x19" in touch and "doom-red-single-border" in touch
assert "EspNativeGameplayPassword_serviceCursor(" in resident
assert "esp_timer_get_time() / 1000LL" in resident

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
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS native Doom UI: 108x74 title, red single-border, 5x9 modal caret, 450ms retry")
