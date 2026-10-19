#!/usr/bin/env python3
"""Static ownership guard for standalone HUD weapon selection and HUB tabs."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
def source(p):
    return (ROOT / p).read_text(encoding="utf-8")
hub = source("ESP32/src/esp_native_gameplay_hub.c")
ui = source("ESP32/src/esp_native_gameplay_hub_touch_ui.c")
resident = source("ESP32/src/esp_native_resident_gameplay.c")
hud = source("ESP32/src/esp_native_gameplay_hud.c")
inp = source("ESP32/src/esp_native_gameplay_input.c")
assert "EspNativeGameplayHub_openWeapons(void)" in hub
assert "return openPage(ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS);" in hub
assert "hub.page = firstPage;" in hub
assert "hubNextMenuPage(hub.page)" in hub and "hubPrevMenuPage(hub.page)" in hub
assert 'HUB_UI_WPN_LEFT' not in ui
assert '"INV"' in ui and '"STAT"' in ui and '"SYS"' in ui
assert '"WEAPONS"' in ui and '"BACK"' in ui
assert "ESP_NATIVE_GAMEPLAY_ACTION_NONE;" in ui
pages = ui.split("int EspNativeGameplayHubTouchUi_consumedPageTarget(",1)[1].split(
    "int EspNativeGameplayHubTouchUi_consumedInventoryTarget",1)[0]
assert "ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS" not in pages
selection = hub.split("static EspNativeGameplayHubStatus handleWeaponGridSelect(void)",1)[1].split(
    "EspNativeGameplayHubStatus EspNativeGameplayHub_handleAction",1)[0]
assert "GRID-DEFER" in selection and "cause=not-owned" in selection
assert "ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN" in selection
assert "return ESP_NATIVE_GAMEPLAY_HUB_CLOSED;" in selection
assert "paintCurrentPage()" not in selection
assert "EspNativeGameplayPlayerState_restore(&before)" in selection
assert "EspNativeGameplayHub_openWeapons()" in resident
assert 'weaponPanel ? "hud-ammo-compass"' in resident
assert "ESP_NATIVE_GAMEPLAY_ACTION_WEAPON_MENU" in inp
assert "drawWeaponTouchCorners(framebuffer, stats);" in hud
controls = source("ESP32/src/esp_native_gameplay_controls.c")
glyph = controls.split("static int drawActionGlyph(",1)[1].split(
    "static void fillStats(",1)[0]
assert "case ESP_NATIVE_GAMEPLAY_ACTION_WEAPON_MENU:" in glyph
assert "drawFeedbackLine(framebuffer, cx - 6, cy - 4," in glyph
assert "case ESP_NATIVE_GAMEPLAY_ACTION_WEAPON_MENU:" in controls.split(
    "static FeedbackPalette feedbackPalette(",1)[1].split(
        "static uint16_t glowAdd565(",1)[0]
print("PASS HUD direct weapon grid, weapon-menu feedback glyph, 3 tabs, owned selection closes without turn")
