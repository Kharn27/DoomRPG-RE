#!/usr/bin/env python3
"""Ensure automap PASS_TURN uses the real native executor, never silent-ignore."""
from pathlib import Path

source = Path("ESP32/src/esp_native_resident_gameplay.c").read_text()
start = source.index("    if (automapActive()) {", source.index("void EspNativeResidentGameplay_service("))
end = source.index("    /* HUB owns the input domain", start)
branch = source[start:end]
assert "case ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN:" in branch
assert "EspNativeGameplayPassTurn_execute(doomRpg, &intent)" in branch
assert "case ESP_NATIVE_GAMEPLAY_ACTION_MOVE_FORWARD:" in branch
assert "case ESP_NATIVE_GAMEPLAY_ACTION_SELECT:" in branch
assert "case ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP:" in branch
assert "pass-live otherLegacyActions=deferred" in branch
assert "passStatus != ESP_NATIVE_GAMEPLAY_PASS_TURN_OK" in branch
print("automap_pass_turn_routing_host: PASS full-screen pass action dispatches native tile-touch and one monster-turn producer")
