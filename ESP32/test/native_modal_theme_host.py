#!/usr/bin/env python3
"""Native MENU theme coverage: same byte-owned palette, no new UI layer.

Pixel values and full-red INFERNO track are exercised by the real compiled
skin in main_menu_skin_host.c; this source-level integration guard ensures
all modal paint owners read that palette and retain their existing gameplay/
touch contracts. Actual visual acceptance still belongs to the classic CYD.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
def load(path):
    return (ROOT / path).read_text(encoding="utf-8")

skin = load("ESP32/src/esp_main_menu_skin.c")
header = load("ESP32/include/esp_main_menu_skin.h")
loading = load("ESP32/src/esp_native_transition_presentation.c")
levelup = load("ESP32/src/esp_native_gameplay_level_up.c")
hub = load("ESP32/src/esp_native_gameplay_hub.c")
hubui = load("ESP32/src/esp_native_gameplay_hub_touch_ui.c")
weapons = load("ESP32/src/esp_native_gameplay_hub_weapon_grid.c")
system = load("ESP32/src/esp_native_gameplay_save.cpp")
death = load("ESP32/src/esp_native_gameplay_player_death.c")

for name in ("EspMainMenuSkin_uiAccent", "EspMainMenuSkin_uiAccentDim",
             "EspMainMenuSkin_uiInk"):
    assert name in skin and name in header
assert "static uint8_t selectedLetterTheme" in skin
assert "EspMainMenuSkin_setTheme(uint8_t theme)" in skin
assert "malloc(" not in skin and "calloc(" not in skin

# Only filled 6-row track is retinted. Neither width nor 100%-edge contract
# nor the starfield/PAK backing-store/present gate is modified.
assert "EspMainMenuSkin_progressTone(row, trackHeight)" in loading
assert "const int trackWidth = TRANSITION_PROGRESS_WIDTH - 8;" in loading
assert "fillWidth = (trackWidth * (int)percent) / 100" in loading
assert "if (!EspMainMenuSkin_paintPlate(" in loading
assert "int __real_Esp32PlatformVideo_present(void);" in loading

stats = loading.split("int EspNativeTransitionPresentation_showStats(", 1)[1].split(
    "int EspNativeTransitionPresentation_beginLoading(", 1)[0]
assert "EspMainMenuSkin_paintPlate(" in stats
assert "MISSION COMPLETE" in stats
assert stats.count("EspMainMenuSkin_uiAccent()") >= 4
assert "EspMainMenuSkin_getTheme()" in stats
assert "COLOR_GREEN : EspMainMenuSkin_uiAccent()" in stats
assert "__real_Esp32PlatformVideo_present()" in stats
assert "markCompleted(" not in stats
assert "LEVELSTATS] PRESENT" in stats

level = levelup.split("static int paint(void) {", 1)[1].split(
    "void EspNativeGameplayLevelUp_reset(", 1)[0]
assert "EspMainMenuSkin_paintPlate(" in level
assert "EspMainMenuSkin_uiAccent()" in level
assert "EspMainMenuSkin_uiAccentDim()" in level
assert "COLOR_GREEN" in level
assert "TAP TO CONTINUE" in level
assert "__real_Esp32PlatformVideo_present()" in level

# HUB owns exactly its 160x120 framebuffer, and navigation remains within
# the pre-existing tabs, inventory, notebook, weapons and SYSTEM controls.
assert "#define HUB_UI_FOCUS EspMainMenuSkin_uiAccent()" in hubui
assert "EspNativeGameplayHubTouchUi_headerTabBounds" in hubui
assert "HUB_UI_DANGER ESP_HUB_COLOR_RED" in hubui
assert "HUB_UI_ARMOR ESP_HUB_COLOR_BLUE" in hubui
assert "#define HUB_MENU_INNER EspMainMenuSkin_uiAccent()" in hub
assert "45, EspMainMenuSkin_uiAccent());" in hub
assert "#define GRID_EQUIPPED EspMainMenuSkin_uiAccent()" in weapons
assert "GRID_DIM_BORDER ESP_HUB_COLOR_STEEL_DARK" in weapons

# SAVE/LOAD focused cards and navigation change theme, but warnings and
# successful confirmations keep their semantic red/green.
assert "EspMainMenuSkin_uiAccent()" in system
assert "ESP_HUB_COLOR_AMBER" not in system
assert "ESP_HUB_COLOR_RED" in system and "ESP_HUB_COLOR_GREEN" in system
assert "confirmationTarget == kStatusExit" in system

assert "EspMainMenuSkin_uiInk()" in death
assert "EspMainMenuSkin_uiAccentDim()" in death
assert '"YOU DIED"' in death and "ESP_HUB_COLOR_RED" in death
assert "EspMainMenuSkin_paintPlate(" in death

print("PASS theme native modal owners: stats/levelup/HUB/SYSTEM/death + red INFERNO loading; semantic colors and owners retained")
