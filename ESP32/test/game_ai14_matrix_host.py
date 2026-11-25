#!/usr/bin/env python3
"""GAME-AI-14 A0: source-backed inventory for EVERY enemy subtype.

This is deliberately NOT an AI-behavior parity test. It verifies all fourteen
original weapon/shot/timing metadata records against the actual compiled native
source files and explicitly inventories known unfinished behavior branches.
Known gaps are reported as observations, never asserted as desired behavior.
Next milestones will add real-C behavioral oracles.
"""
from __future__ import annotations
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "ESP32/src"
SOURCE = ROOT / "src"

def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")

def array(text: str, name: str, expected: int) -> list[int]:
    # Parse only simple static literal arrays; intentionally fail on an API or
    # representation change until the historical oracle has been re-reviewed.
    matches = list(re.finditer(r"\b" + re.escape(name) +
                               r"\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}",
                               text, re.S))
    assert len(matches) == 1, f"{name}: missing or ambiguous literal array"
    values = re.sub(r"/\*.*?\*/|//[^\n]*", "", matches[0].group(1),
                    flags=re.S)
    values = [v.strip() for v in values.split(",") if v.strip()]
    assert len(values) == expected, f"{name}: expected {expected}, found {len(values)}"
    for v in values:
        assert re.fullmatch(r"(?:0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*", v), (
            name, "nonliteral token", v)
    return [int(re.sub(r"[uUlL]+$", "", v), 0 if v.lower().startswith("0x") else 10)
            for v in values]

def require(code: str, marker: str, owner: str) -> None:
    assert marker in code, f"{owner}: historical branch or explicit gap changed: {marker}"

legacy_entity = read(SOURCE / "Entity.c")
legacy_combat = read(SOURCE / "Combat.c")
legacy_weapons = array(read(SOURCE / "CombatEntity.c"), "monsterAttacks", 28)
legacy_wp = array(legacy_combat, "monsterWpInfo", 28)
expected_shots = legacy_wp[::2]
expected_phase = [10 * delay for delay in legacy_wp[1::2]]

weapon_owners = (
    "esp_native_gameplay_monster_turn.c",
    "esp_native_gameplay_monster_movement.c",
    "esp_native_gameplay_monster_retaliation.c",
    "esp_native_gameplay_monster_movement_publish.c",
)
shot_owners = (
    "esp_native_gameplay_monster_turn.c",
    "esp_native_gameplay_monster_retaliation.c",
    "esp_native_gameplay_monster_attack_visual.c",
)
for name in weapon_owners:
    actual = array(read(NATIVE / name), "monsterAttacks", 28)
    assert actual == legacy_weapons, f"{name}: 14-subtype weapon parity drift"
for name in shot_owners:
    actual = array(read(NATIVE / name), "monsterShots", 14)
    assert actual == expected_shots, f"{name}: shot-count parity drift"
visual = read(NATIVE / "esp_native_gameplay_monster_attack_visual.c")
assert array(visual, "monsterPhaseMs", 14) == expected_phase, (
    "AttackVisual: 14-subtype attack-pose cadence parity drift"
)

sequence = read(NATIVE / "esp_native_gameplay_monster_active_sequence.c")
movement = read(NATIVE / "esp_native_gameplay_monster_movement.c")
publish = read(NATIVE / "esp_native_gameplay_monster_movement_publish.c")
turn = read(NATIVE / "esp_native_gameplay_monster_turn.c")
post = read(NATIVE / "esp_native_gameplay_monster_three_goal_turn.c")
combat = read(NATIVE / "esp_native_gameplay_monster_combat.c")
audio = read(NATIVE / "esp_native_monster_ambience.c")

# Exact original families, separate from source-backed capability status.
for marker in ("eSubType == 4 || entity->def->eSubType == 13",
               "eSubType == 6 || entity->def->eSubType == 7",
               "eSubType == 10", "iVar4 = 3", "i = (entity->def->eSubType == 4",
               "i4 == 0 || i4 > 0x4000", "entity->info ^= 0x800000"):
    require(legacy_entity, marker, "legacy Entity.c")

# Report historical gaps without requiring a bug to remain present.
# A changed marker is not evidence of a fix; it requires a behavioral oracle.
def observe(code: str, marker: str, owner: str) -> None:
    status = "MARKER-PRESENT" if marker in code else "MARKER-CHANGED-REVIEW"
    print(f"[GAMEAI14-GAP] {owner}: {status}; parity=UNPROVEN")
observe(sequence, "monster->subtype == ACTIVESEQ_SUBTYPE_SPECIAL_AI", "main ActiveSequence")
observe(movement, "monster->subtype == MOVE_SUBTYPE_SPECIAL_AI", "main Movement")
observe(publish, "monster->subtype == PUBLISH_MOVE_SPECIAL_AI", "main MovementPublish")
observe(turn, "monster->subtype == TURN_SUBTYPE_SPECIAL_AI", "main MonsterTurn")
observe(movement, "legacyFallbackAttack=deferred", "main Movement/no-move")
observe(turn, "targets a traced destructible instead of the player", "main MonsterTurn")
observe(post, "monster->subtype != 4U && monster->subtype != 13U", "main ThreeGoalTurn")
observe(post, "calcPath-special-plane-corpus-needed", "main ThreeGoalTurn")
for subtype in (7, 8, 12, 13):
    observe(combat, "(1U << %d)" % subtype, "main SPECIAL_DEATH_MASK")
observe(combat, "if (lethal && specialDeath(target->subtype))", "main MonsterCombat")
observe(visual, "EspNativeMonsterProjectile204_attackOwned", "main AttackVisual")
observe(audio, "nextAudioRandom(void)", "main monster audio RNG")

assert len(legacy_weapons) == len(legacy_wp) == 28
assert len(expected_shots) == len(expected_phase) == 14
assert all(1 <= n <= 3 for n in expected_shots)

special = {
    1: "post-move-melee", 4: "three-goal#439", 5: "post-move-melee",
    7: "special-death-blocked", 8: "special-death-blocked",
    10: "raise-unowned#438", 12: "special-death-blocked",
    13: "three-goal+special-death-blocked",
}
for subtype in range(14):
    weapon0, weapon1 = legacy_weapons[2 * subtype:2 * subtype + 2]
    flags = special.get(subtype, "generic-route")
    print(f"[GAMEAI14] subtype={subtype:02d} weapons={weapon0}/{weapon1} "
          f"shots={expected_shots[subtype]} phaseMs={expected_phase[subtype]} "
          f"family={flags} metadata=PARITY gameplayParity=UNPROVEN")

print("[GAMEAI14] PASS source metadata=14/14, weaponOwners=4, "
      "shotOwners=3, phaseOwner=1, historicalGaps=observed-not-asserted; "
      "turn/RNG/collision/death/CYD fidelity NOT YET PROVEN")
