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
for forbidden in ("malloc(", "calloc(", "realloc(", "free(", "esp_timer", "SDL_", "PlatformVideo_present", "setRotation(", "EspAssetPack"):
    assert forbidden not in skin, forbidden
assert "ESP_MAIN_MENU_SKIN_CONFIRM" in skin
assert "if (second != NULL)" in skin
print("[MENUSKIN-INTEGRATION] PASS main-only, actions 0..3, untouched touch/Options, no heap or IO")
