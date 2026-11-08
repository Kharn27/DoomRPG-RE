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
assert "EspMainMenuSkin_paintModalTitleStencil(" in stats
assert "EspMainMenuSkin_paintPlate(" not in stats
assert "MISSION COMPLETE" in stats
assert stats.count("EspMainMenuSkin_uiAccent()") >= 3
assert "EspMainMenuSkin_getTheme()" in stats
assert "COLOR_GREEN : EspMainMenuSkin_uiAccent()" in stats
assert "__real_Esp32PlatformVideo_present()" in stats
assert "markCompleted(" not in stats
assert "LEVELSTATS] PRESENT" in stats

level = levelup.split("static int paint(void) {", 1)[1].split(
    "void EspNativeGameplayLevelUp_reset(", 1)[0]
assert "EspMainMenuSkin_paintModalTitleStencil(" in level
assert "EspMainMenuSkin_paintPlate(" not in level
assert "EspMainMenuSkin_uiAccent()" in level
assert "EspMainMenuSkin_uiAccentDim()" in skin
assert "MISSION COMPLETE" in stats
assert '"LEVEL UP", 80, 11' in level
assert '"MISSION COMPLETE",\n        80, 11' in stats
assert "COLOR_GREEN" in level
assert "TAP TO CONTINUE" in level
assert "__real_Esp32PlatformVideo_present()" in level

system = load("ESP32/src/esp_native_gameplay_save.cpp")

# Both page chevrons (LOAD GAME and HUB SAVE/LOAD) now use the same real
# industrial painter; their four touch rectangles and page predicates remain
# exactly as before. No extra framebuffer or read of slot data for styling.
nav = system.split("void drawSlotNav(", 1)[1].split("bool paintSaveOverlay(", 1)[0]
assert "EspMainMenuSkin_paintNavKey(" in nav
assert "fillRect(fb" not in nav and "drawRect(fb" not in nav
assert "118, top, 156, bottom, next ? 1 : 0, enabled ? 1 : 0" in nav
for call in (
    "drawSlotNav(fb, 34, 62, false, first != 1U);",
    "drawSlotNav(fb, 69, 98, true, first == 1U);",
    "drawSlotNav(fb, 49, 78, false, first != 1U);",
    "drawSlotNav(fb, 83, 112, true, first == 1U);",
):
    assert call in system
assert "if (x >= 118 && x <= 156 && y >= 34 && y <= 98)" in system
assert "if (x >= 118 && x <= 156)" in system
assert "const bool previous = y >= 49 && y <= 78;" in system
assert "const bool next = y >= 83 && y <= 112;" in system


# MENU_MAIN -> LOAD GAME is the only layout changed. The shared 5x9 skin
# painter gives the title and BACK their true 3-theme row gradient, while
# the label's non-clickable title rail has no triple-hatch side ornaments.
# The page fraction sits BELOW the unchanged DOWN key.
main_load = system.split("static void paintMainSlots(void) {", 1)[1].split(
    'extern "C" int EspNativeGameplaySave_mainSelectorBegin(', 1)[0]
main_tap = system.split('extern "C" int EspNativeGameplaySave_mainSelectorTap(', 1)[1].split(
    'extern "C" int EspNativeGameplaySave_slotSelectorActive(', 1)[0]
hub_slots = system.split("bool paintSaveOverlay(void) {", 1)[1].split(
    "}  // namespace", 1)[0]
assert "EspMainMenuSkin_paintModalTitleStencil(" not in main_load
assert 'fillRect(fb, 45, 27, 114, 27, EspMainMenuSkin_uiAccentDim());' in main_load
assert 'fillRect(fb, 73, 27, 86, 27, EspMainMenuSkin_uiAccent());' in main_load
assert '"LOAD GAME", 0, 1)) return;' in main_load
assert '"LOAD - PAGE' not in main_load
assert 'snprintf(text, sizeof(text), "%u/2", first == 1U ? 1U : 2U);' in main_load
assert "EspNativeGameplayHubTouchUi_drawMiniText(" in main_load
assert "fb, text, 137, 106, EspMainMenuSkin_uiAccent());" in main_load
assert main_load.index("drawSlotNav(fb, 69, 98, true, first == 1U);") < main_load.index(
    'snprintf(text, sizeof(text), "%u/2",')
assert 98 < 106 and 106 + 5 <= 118  # fraction under DOWN in safe 20px foot
assert (137 - 5, 137 + 5) == (132, 142)  # 3 glyphs x 3 + gaps=11px
assert 111 < 132  # fraction cannot collide with BACK on same y band
# Title and BACK both use the true 9-row skin palette and no flat color face.
assert main_load.count("EspMainMenuSkin_paintText(") == 2
assert 'drawCenteredWord(fb, 80, 11, "LOAD GAME"' not in main_load
assert 'drawCenteredWord(fb, 72, 107, "BACK"' not in main_load
assert "kMainLoadBackLeft = 34;" in system
assert "kMainLoadBackRight = 111;" in system
assert "kMainLoadBackTop = 102;" in system
assert "kMainLoadBackBottom = 118;" in system
assert "kMainLoadBackLeft, kMainLoadBackTop," in main_load
assert "kMainLoadBackRight, kMainLoadBackBottom," in main_load
assert "EspMainMenuSkin_paintPlate(" in main_load
assert '"BACK", 0, 1)) return;' in main_load
assert "fillRect(fb, 34, 105, 111, 118" not in main_load
assert "drawRect(fb, 34, 105, 111, 118" not in main_load
assert "if (y >= kMainLoadBackTop)" in main_tap
assert "if (y <= kMainLoadBackBottom &&" in main_tap
assert "x >= kMainLoadBackLeft && x <= kMainLoadBackRight" in main_tap
# All five original 107x13 row cards, their select/confirm cadence, page
# arrows, and the HUB's separate save overlay remain untouched.
for call in (
    "const int y = 32 + row * 14;",
    "fillRect(fb, 7, y, 113, y + 12,",
    "drawSlotNav(fb, 34, 62, false, first != 1U);",
    "drawSlotNav(fb, 69, 98, true, first == 1U);",
):
    assert call in main_load
assert "const uint8_t row = (uint8_t)((y - 32) / 14);" in main_tap
assert "if (x >= 118 && x <= 156 && y >= 34 && y <= 98)" in main_tap
assert "(!previous && (y < 69 || page != 1U))" in main_tap
assert "slotArmed = 0U;" in main_tap
assert "mainSlotReady = 1U;" in main_tap
assert "EspMainMenuSkin_paintModalTitleStencil(" not in hub_slots
assert '"%s - PAGE %u/2"' in hub_slots
assert "drawSlotNav(fb, 49, 78, false, first != 1U);" in hub_slots
assert "drawSlotNav(fb, 83, 112, true, first == 1U);" in hub_slots

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
