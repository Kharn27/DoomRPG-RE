#!/usr/bin/env python3
"""GAME-S1 subtype10 P1: pure compiled original-condition oracle + wiring audit."""
from pathlib import Path
import subprocess
import tempfile

esp = Path(__file__).resolve().parents[1]
repo = esp.parent
legacy = (repo / "src/Entity.c").read_text()
sequence = (esp / "src/esp_native_gameplay_monster_active_sequence.c").read_text()
turn = (esp / "src/esp_native_gameplay_monster_turn.c").read_text()
state = (esp / "src/esp_native_gameplay_monster_state.c").read_text()

assert "DoomRPG_randNextByte(&entity->doomRpg->random) < 84" in legacy
assert "i4 == 0 || i4 > 0x4000" in legacy
assert "Game_findMapEntityXYFlag(entity->doomRpg->game, spr->x, spr->y, 16263)" in legacy
assert "entity->info ^= 0x800000" in legacy
assert "EspNativeSubtype10_wantsRaise" in sequence
assert "EspNativeSubtype10_candidateEligible" in sequence
assert "EspNativeGameplayMonsterState_toggleAlternateAttack" in sequence
assert "EspNativeRngReplayGuard_beginProbeBoundary" in sequence
assert "cause=special-ai-unowned" not in sequence
assert "if (monster->subtype == TURN_SUBTYPE_SPECIAL_AI) {\n        return 1;" not in turn
assert "int EspNativeGameplayMonsterState_toggleAlternateAttack(" in state

with tempfile.TemporaryDirectory(prefix="subtype10_") as d:
    exe = Path(d) / "policy"
    subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
         "-I", str(esp / "include"),
         str(esp / "test/monster_subtype10_policy_host.c"),
         "-o", str(exe)], check=True
    )
    subprocess.run([str(exe)], check=True)
print("monster_subtype10_admission_host: PASS source-anchored wiring + compiled policy")
