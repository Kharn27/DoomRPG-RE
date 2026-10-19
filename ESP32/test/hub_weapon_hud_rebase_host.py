#!/usr/bin/env python3
"""Real-world weapon selection must keep strict HUD rebase; skip duplicate compass.

At a matching view/model angle, full HUD owns the compass. Only stale retained
angles need direction dirty-rectangle overlay; this avoids a redundant partial
pass over ammo/compass during weapon selection without weakening the FNV gate.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
hub = (root / "ESP32/src/esp_native_gameplay_hub.c").read_text()
begin = hub.index("static int repaintGameplayHud(void)")
end = hub.index("\nstatic void headerPixel(", begin)
repaint = hub[begin:end]
assert "EspNativeGameplayHud_repaint(state, &stats)" in repaint
assert "view->viewAngle != view->destAngle" in repaint
assert "if ((uint8_t)view->viewAngle == state->model.destAngle)" in repaint
assert "return 1; /* full native HUD is canonical" in repaint
assert "return EspNativeGameplayHudDirection_render(" in repaint
assert repaint.index("EspNativeGameplayHud_repaint(state, &stats)") < repaint.index(
    "if ((uint8_t)view->viewAngle == state->model.destAngle)") < repaint.index(
    "return EspNativeGameplayHudDirection_render(")
close = hub.split("if (action == ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN) {",1)[1].split(
    "if (action == ESP_NATIVE_GAMEPLAY_ACTION_SELECT)",1)[0]
assert "const int replayOk = repaintGameplayHud();" in close
assert "exactBottomTarget != restoredProtected" in close
assert "hudRepainted = 0;" in close
assert "return (playerExact && menuRestored && hudRepainted" in close
assert "HUBHUD] REBASE-FAILED" in close
assert "HUBWEAPON] GRID-CLOSE-ROLLBACK" in hub
assert "return ESP_NATIVE_GAMEPLAY_HUB_CLOSED;" in hub
print("PASS HUD close strict FNV preserved; full compass when same angle, live dirty-overlay only on stale angle")
