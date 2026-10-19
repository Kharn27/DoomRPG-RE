#!/usr/bin/env python3
"""Fail closed if a HUB weapon close stops using current PlayerState pixels."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
hub = (root / "ESP32/src/esp_native_gameplay_hub.c").read_text()
hud = (root / "ESP32/src/esp_native_gameplay_hud.c").read_text()
frame = (root / "ESP32/src/esp_native_gameplay_frame.c").read_text()
a = hub.index("static int repaintGameplayHud(void)")
b = hub.index("\nstatic void headerPixel(",a)
repaint = hub[a:b]
assert "view->viewAngle != view->destAngle" in repaint
assert "EspNativeGameplayHud_snapshotLive(" in repaint
assert "EspNativeGameplayHud_repaint(&live, &stats)" in repaint
assert "EspNativeGameplayHudDirection_render(" not in repaint
assert "model.weapon = player->weapon;" in hud
assert "model.ammo = player->ammo[ammoType];" in hud
assert "model.destAngle = angle;" in hud
assert "EspNativeGameplayHud_snapshotLive(" in frame
assert "weaponHud->model.weapon" in frame
assert "hudState->model.weapon," not in frame
close = hub.split("if (action == ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN) {",1)[1].split(
    "if (action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT)",1)[0]
assert "const int replayOk = repaintGameplayHud();" in close
assert "exactBottomTarget != restoredProtected" in close
assert "hudRepainted = 0;" in close
assert "return (playerExact && menuRestored && hudRepainted" in close
assert "HUBHUD] REBASE-FAILED" in close
assert "HUBWEAPON] GRID-CLOSE-ROLLBACK" in hub
assert "return ESP_NATIVE_GAMEPLAY_HUB_CLOSED;" in hub
print("PASS HUD PlayerState live snapshot, strict exact replay, viewport weapon is current")
