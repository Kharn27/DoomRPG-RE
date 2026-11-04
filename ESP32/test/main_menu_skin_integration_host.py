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
for label in ('case 0: first = "START"; break;',
              'case 1: first = "LOAD"; break;',
              'case 2: first = "OPTIONS"; break;',
              'default: first = "HELP"; break;'):
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
assert 'second = "GAME"' not in skin
assert 'second = "ABOUT"' not in skin
assert 'word(fb, stride, l, t, r, b, first, p,' in skin
# The final UI follows the physical reference: one bold word in each
# forged card, not two small yellow terminal-style lines.
assert "glyphs[12][9]" in skin
assert "length * 6 - 1" in skin
assert "row < 9" in skin and "bit < 5" in skin
assert "0xf6afU" in skin and "0xe4e9U" in skin and "0xaa85U" in skin
assert "0xffffU" in skin and "p->ink" in skin
assert "for (pass = armed ? 0 : 1; pass < 2; ++pass)" in skin
assert 'p == &palettes[ESP_MAIN_MENU_SKIN_CONFIRM]' in skin
assert "0xd5a0U" not in skin
assert "0xfff5U" not in skin and "0x2800U" not in skin
assert "diagonalShadow" not in skin
assert "horizontal(fb, stride, l + 9, r - 9, t + 3," in skin
assert "horizontal(fb, stride, l + 9, r - 9, b - 3," in skin
assert "vertical(fb, stride, l + 3, t + 7, b - 7," in skin
assert "vertical(fb, stride, r - 3, t + 7, b - 7," in skin
assert "industrialMain ? 0x0800U : MAIN_DOOM_METAL" in menu
assert "industrialMain ? 0x4a48U : ESP_HUB_COLOR_STEEL_DARK" in menu
# Only Options keeps its historical top/bottom decoration; MENU_MAIN removes it.
rail = menu[menu.index("int DoomRPG_esp32PaintMenuDashboardCards("):
            menu.index("/* The child reuses the same protected right rail;")]
assert "if (!industrialMain) {" in rail
assert rail.index("if (!industrialMain) {") < rail.index(
    "fillRect565(render, 105, 7, 125, 8, MAIN_DOOM_RED);")
assert rail.index("if (!industrialMain) {") < rail.index(
    "fillRect565(render, 105, 112, 118, 112, MAIN_DOOM_RED);")
print("[MENUSKIN-INTEGRATION] PASS main-only, actions 0..3, untouched touch/Options, no heap or IO")
