#!/usr/bin/env python3
"""Strict integration proof: only MENU_MAIN is skinned, actions/touch untouched."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
menu = (root / "ESP32/src/native_main_menu_touch_layout.c").read_text()
skin = (root / "ESP32/src/esp_main_menu_skin.c").read_text()
touch = (root / "ESP32/src/native_main_menu_touch.c").read_text()
actions = (root / "ESP32/src/native_main_menu_actions.c").read_text()
layout = (root / "ESP32/include/native_main_menu_160x120_layout.h").read_text()

assert "const int industrialMain = labels == dashboardLabels;" in menu
assert "if (industrialMain) {" in menu
assert "EspMainMenuSkin_paint(" in menu
assert "} else {\n            drawDashboardCard(" in menu
assert "DoomRPG_esp32MainMenuPaintDashboardSelection(" in menu
assert "DoomRPG_esp32PaintBrightnessPresetCards(" in menu
assert "DoomRPG_esp32PaintOrientationPresetCards(" in menu
assert "DoomRPG_esp32MainMenuCardRect(item, left, top, right, bottom);" in menu
assert "return findHitItem(screenX / DOOMRPG_INTEGER_SCALE" in touch
assert "CONFIRM-PASS" in touch and "SELECT-ARM" in touch
assert "DoomRPG_esp32MainMenuTouchArmSelected(hit)" in touch
assert "executeConfirmedAction(hit);" in touch
for action in ("START", "LOAD", "OPTIONS", "HELP"):
    assert "case DOOMRPG_ESP32_MAIN_MENU_ACTION_" + action in actions
for label in ('first = "START"; second = "GAME"',
              'first = "LOAD"; second = "GAME"',
              'first = "OPTIONS"; second = NULL',
              'first = "HELP"; second = "ABOUT"'):
    assert label in skin
for unchanged in ("#define DOOMRPG_ESP32_MAIN_MENU_CARD_LEFT 104",
                  "#define DOOMRPG_ESP32_MAIN_MENU_CARD_RIGHT 157",
                  "#define DOOMRPG_ESP32_MAIN_MENU_CARD_HEIGHT 20"):
    assert unchanged in layout
# Main-only HEIGHT extension must be shared by paint + touch, not Options.
assert "#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_LEFT 104" in layout
assert "#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_RIGHT 157" in layout
assert "#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_HEIGHT 24" in layout
assert "#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_FIRST_TOP 8" in layout
assert "#define DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_PITCH 27" in layout
assert "DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_TOP(item)" in layout
assert "DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_TOP(0)" in menu
assert "DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_TOP(1)" in menu
assert "DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_PITCH" in menu
assert "fillRect565(render, 158, 17" not in menu
assert "DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_BOTTOM(item)" in layout
assert "DOOMRPG_ESP32_INDUSTRIAL_MAIN_CARD_HEIGHT" in skin
assert "DoomRPG_esp32IndustrialMainCardRect(item, &l, &t, &r, &b)" in skin
assert "DoomRPG_esp32IndustrialMainCardRect(item, left, top, right, bottom)" in touch
assert "DoomRPG_esp32MainMenuCardRect(item, left, top, right, bottom)" not in touch
assert "DoomRPG_esp32MainMenuCardRect(item, left, top, right, bottom)" in menu
for forbidden in ("malloc(", "calloc(", "realloc(", "free(", "esp_timer", "SDL_", "PlatformVideo_present", "setRotation(", "EspAssetPack"):
    assert forbidden not in skin, forbidden
assert "ESP_MAIN_MENU_SKIN_CONFIRM" in skin
assert "if (second != NULL)" in skin
# Single saturated-gold Idle glyph pass (no second shadow letter);
# selected white maintains its original two engraved shadow passes.
assert "glyphs[16][6]" in skin
assert "length * 5 - 1" in skin
assert "row < 6" in skin and "bit < 4" in skin
assert "0xfec0U" in skin and "0xffffU" in skin
assert "0xff29U" not in skin and "0x2800U" not in skin
assert "0xfff5U" not in skin
assert "for (pass = p == &palettes[ESP_MAIN_MENU_SKIN_IDLE] ? 2 : 0;" in skin
assert "pass == 0 ? p->shadow : p->ink" in skin
assert "0x7140U" not in skin
assert "else if (!idle)" in skin
assert "0xe902U" not in skin and "0xfdd7U" not in skin
assert "0xf102U" not in skin and "0xd6b3U" not in skin
assert "diagonalShadow" not in skin
assert "horizontal(fb, stride, l + 7, r - 7, t + 2," in skin
assert "horizontal(fb, stride, l + 7, r - 7, b," in skin
assert "vertical(fb, stride, l + 2, t + 6, b - 6," in skin
assert "vertical(fb, stride, r - 2, t + 6, b - 6," in skin
assert "if (state == ESP_MAIN_MENU_SKIN_ARMED)" not in skin
# Only Options keeps its historical top/bottom decoration; MENU_MAIN removes it.
rail = menu[menu.index("int DoomRPG_esp32PaintMenuDashboardCards("):
            menu.index("/* The child reuses the same protected right rail;")]
assert "if (!industrialMain) {" in rail
assert rail.index("if (!industrialMain) {") < rail.index(
    "fillRect565(render, 105, 7, 125, 8, MAIN_DOOM_RED);")
assert rail.index("if (!industrialMain) {") < rail.index(
    "fillRect565(render, 105, 112, 118, 112, MAIN_DOOM_RED);")
print("[MENUSKIN-INTEGRATION] PASS main-only, actions 0..3, untouched touch/Options, no heap or IO")
