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
assert "EspNativeGameplayHud_repaintLive(" in repaint
assert "EspNativeGameplayHud_repaint(&live, &stats)" not in repaint
assert "EspNativeGameplayHudDirection_render(" not in repaint
assert "model.weapon = player->weapon;" in hud
assert "model.ammo = noWeapon ? 0U : player->ammo[ammoType];" in hud
assert "model.weaponsPresent = noWeapon ? 0U : 1U;" in hud
assert "player->weapon != 0U || (player->param1 & 0xffU) != 0U" in hud
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

# All 3 modal restorations must use the same live PlayerState/camera owner.
resident = (root / "ESP32/src/esp_native_resident_gameplay.c").read_text()
helper = resident.split("static EspNativeGameplayHudStatus repaintLiveHudAtView(",1)[1].split(
    "static int renderAutomapCurrent(",1)[0]
assert "view->viewAngle != view->destAngle" in helper
assert "EspNativeGameplayPlayerState_view()" in helper
assert "EspNativeGameplayHud_repaintLive(" in helper
assert "EspNativeGameplayHud_view()" in helper
password = resident.split("static void servicePasswordCompletion(",1)[1].split(
    "static ",1)[0]
automap = resident.split("static int closeAutomap(",1)[1].split(
    "static int restoreWorldAfterHub(",1)[0]
levelup = resident.rsplit("if (EspNativeGameplayLevelUp_isActive()) {",1)[1].split(
    "if (EspNativeGameplayControls_isActive())",1)[0]
assert "repaintLiveHudAtView(view, &hudStats)" in password
assert "repaintLiveHudAtView(view, &hudStats)" in automap
assert "repaintLiveHudAtView(levelView, &levelHudStats)" in levelup
assert "EspNativeGameplayHud_repaint(" not in resident
assert "EspNativeGameplayHud_repaint(" not in password + automap + levelup
wrapper = hud.split("EspNativeGameplayHudStatus EspNativeGameplayHud_repaintLive(",1)[1]
assert "!EspNativeGameplayHud_snapshotLive(" in wrapper
assert "return EspNativeGameplayHud_repaint(&live, outStats);" in wrapper
assert "return ESP_NATIVE_GAMEPLAY_HUD_UNSUPPORTED_CONTEXT;" in wrapper
print("PASS automap/password/levelup modal restores use one live HUD snapshot, no stale retained repaint")
