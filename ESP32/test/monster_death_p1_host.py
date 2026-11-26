#!/usr/bin/env python3
"""MONSTER-DEATH-P1: compile real native C death preparation and cue policy.

No model of the target algorithm is substituted: extract the real C function,
the real 14-family sound matrix and the actual inline header into a C11
adversarial host binary. Existing SDL-F25 and SFX historical oracles stay strict.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "ESP32/src/esp_native_gameplay_monster_combat.c").read_text()
HEADER = ROOT / "ESP32/include/esp_native_monster_death_consequence.h"
LEGACY = (ROOT / "src/Entity.c").read_text()
SFX = (ROOT / "ESP32/test/audio_monster_sfx_all_host.py").read_text()


def body(signature: str) -> str:
    assert SOURCE.count(signature) == 1, ("ambiguous native C function", signature)
    start = SOURCE.index(signature)
    left = SOURCE.index("{", start)
    depth = 0
    for pos in range(left, len(SOURCE)):
        if SOURCE[pos] == "{":
            depth += 1
        elif SOURCE[pos] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[start:pos + 1]
    raise AssertionError("unbalanced function: " + signature)


table = re.search(
    r"static const uint16_t deathSounds\[MONSTER_SUBTYPE_COUNT\]\[3\] = "
    r"\{.*?\n\};", SOURCE, re.S
)
assert table, "lost native 14-family table"
consume = body("static uint16_t consumeDeathSound(")
prepare = body("static int prepareDeathAfterXp(")
assert "EntityMonster_getSoundRnd(sprite->ent->monster, 8)" in LEGACY
assert "Sound_playSound(entity->doomRpg->sound, 5091, 0, 3);" in LEGACY
assert "Sound_playSound(entity->doomRpg->sound, snd, 0, 5);" in LEGACY
assert "Entity_spawnDropItem(entity);" in LEGACY
assert LEGACY.index("Player_addXP(player, CombatEntity_getEXP") < LEGACY.index(
    "EntityMonster_getSoundRnd(sprite->ent->monster, 8)") < LEGACY.index(
    "Entity_spawnDropItem(entity);")
assert "14 exact families" in SFX
assert "specialDeath(before->subtype)" in prepare
assert prepare.index("consumeDeathSound(") < prepare.index(
    "DoomRPG_randNextInt(") < prepare.index(
    "EspNativeGameplayMonsterDrop_prepare(")
assert "EspNativeAudioIntent_publish(" not in prepare
assert "EspNativeGameplayMonsterDrop_commit(" not in prepare
assert "PlayerState_applyXp(" not in prepare
assert not any(word in prepare for word in ("malloc(", "calloc(", "EspAssetPack_", "SDL_"))

direct = SOURCE[SOURCE.index("static int servicePending("):
                SOURCE.index("typedef struct MonsterSplashRollback_s")]
splash = SOURCE[SOURCE.index("int EspNativeGameplayMonsterCombat_applySplash("):
                SOURCE.index("int EspNativeGameplayMonsterCombat_finishSplashPresentation(")]
assert direct.index("EspNativeGameplayPlayerState_applyXp(") < direct.index(
    "if (!prepareDeathAfterXp(") < direct.index("EspNativeGameplayFrame_renderTurn(",
                                                  direct.index("if (!prepareDeathAfterXp("))
assert splash.index("EspNativeGameplayPlayerState_applyXp(") < splash.index(
    "if (!prepareDeathAfterXp(") < splash.index(
    "EspNativeGameplayMonsterDrop_commit(&drop)")
assert "(*EspNativeProcessRng_live()) = randomBefore;" in direct
assert "*target = targetBefore;" in direct
assert "(void)EspNativeGameplayPlayerState_restore(&playerBefore);" in direct
assert "combatOwner = combatRollbackOwner;" in direct
rollback = SOURCE[SOURCE.index("static int restoreSplashSnapshot("):
                  SOURCE.index("int EspNativeGameplayMonsterCombat_beginSplashSequence(")]
for checkpoint in ("EspNativeGameplayMonsterDrop_restore(",
                   "EspNativeGameplayMonsterState_restoreInPlace(",
                   "EspNativeGameplayPlayerState_restore(",
                   "(*EspNativeProcessRng_live()) = splashRollback.random;",
                   "splashAudioCount = 0U;",
                   "splashAudioSequence = 0U;"):
    assert checkpoint in rollback, checkpoint
assert "goto splash_failed;" in splash
assert "EspNativeGameplayMonsterCombat_publishSplashAudioAfterCommit(" in SOURCE
assert "splashSequenceActive != 0U) return 0;" in SOURCE
assert "splashAudioSequence = 0U;" in SOURCE
assert "EspNativeMonsterDeathConsequence_priority(" in SOURCE
assert "EspNativeAudioIntent_publish(consequenceSound,0U," in direct
assert direct.index("logFrame(&pending, \"attack\", &frame)") < direct.index(
    "EspNativeAudioIntent_publish(consequenceSound,0U,")
assert "SPECIAL_DEATH_MASK ((1U << 7) | (1U << 8) | (1U << 12) | (1U << 13))" in SOURCE

host = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_monster_death_consequence.h"
#define MONSTER_SUBTYPE_COUNT 14U
typedef struct { uint32_t cursor; } Random_t;
typedef struct { uint8_t subtype; } EspNativeGameplayMonsterRecord;
typedef struct { uint32_t dropRoll; uint8_t outcome; } EspNativeGameplayMonsterDropSpawnPlan;
typedef struct { int valid; } DoomRPG_t;
static Random_t live;
static DoomRPG_t runtime = {1};
static char calls[12];
static unsigned callCount;
static uint8_t byteChoice;
static uint32_t wordChoice = 0x1234abcdU;
static int failDrop;
static void note(char type) {assert(callCount < sizeof calls); calls[callCount++] = type;}
static Random_t* EspNativeProcessRng_live(void) {return &live;}
static uint8_t DoomRPG_randNextByte(Random_t* r) {
    note('B'); r->cursor++; return byteChoice;
}
static int32_t DoomRPG_randNextInt(Random_t* r) {
    note('W'); r->cursor++; return (int32_t)wordChoice;
}
static int specialDeath(uint8_t subtype) {
    return subtype == 7U || subtype == 8U || subtype == 12U || subtype == 13U;
}
static int EspNativeGameplayMonsterDrop_prepare(
    uint32_t roll, const EspNativeGameplayMonsterRecord* before,
    EspNativeGameplayMonsterDropSpawnPlan* out) {
    assert(before != NULL && out != NULL);
    note('D');
    if (failDrop) return 0;
    out->dropRoll = roll;
    out->outcome = 1U;
    return 1;
}
__TABLE__
__CONSUME__
__PREPARE__
static void clear(uint8_t choice) {
    memset(&live, 0, sizeof live);
    memset(calls, 0, sizeof calls);
    callCount = 0;
    byteChoice = choice;
    failDrop = 0;
}
static void check(uint8_t subtype, uint8_t choice, uint8_t gib) {
    EspNativeGameplayMonsterRecord before = {subtype};
    EspNativeGameplayMonsterDropSpawnPlan plan = {0};
    uint16_t sound = 0;
    uint32_t roll = 0, rngCalls = 0;
    unsigned count = 0;
    clear(choice);
    /* X: XP's own RNG has already been consumed; do not move it. */
    note('X');
    assert(prepareDeathAfterXp(&runtime, &before, gib, &rngCalls,
                               &plan, &sound, &roll) == 1);
    assert(roll == wordChoice && plan.dropRoll == wordChoice);
    assert(calls[0] == 'X');
    if (gib) {
        assert(sound == 5091U && rngCalls == 1U && live.cursor == 1U);
        assert(callCount == 3 && calls[1] == 'W' && calls[2] == 'D');
        assert(EspNativeMonsterDeathConsequence_priority(1U, 1U) == 3U);
    } else {
        while (count < 3 && deathSounds[subtype][count] != 0U) ++count;
        assert(count != 0 && sound == deathSounds[subtype][choice % count]);
        assert(rngCalls == 2U && live.cursor == 2U);
        assert(callCount == 4 && calls[1] == 'B' &&
               calls[2] == 'W' && calls[3] == 'D');
        assert(EspNativeMonsterDeathConsequence_priority(1U, 0U) == 5U);
    }
}
int main(void) {
    const uint8_t picks[] = {0U, 1U, 2U, 3U, 255U};
    unsigned cases = 0U;
    for (uint8_t subtype = 0U; subtype < MONSTER_SUBTYPE_COUNT; ++subtype) {
        EspNativeGameplayMonsterRecord monster = {subtype};
        uint32_t rngCalls = 0, roll = 0;
        uint16_t sound = 0;
        EspNativeGameplayMonsterDropSpawnPlan plan = {0};
        if (specialDeath(subtype)) {
            clear(0U);
            assert(!prepareDeathAfterXp(&runtime, &monster, 0U, &rngCalls,
                                        &plan, &sound, &roll));
            assert(callCount == 0 && live.cursor == 0); /* fail closed */
            continue;
        }
        for (unsigned i = 0; i < sizeof(picks); ++i) {
            check(subtype, picks[i], 0U); ++cases;
            check(subtype, picks[i], 1U); ++cases;
        }
    }
    assert(cases == 100U);
    assert(EspNativeMonsterDeathConsequence_priority(0U, 0U) == 2U);
    assert(EspNativeMonsterDeathConsequence_priority(0U, 1U) == 0U);
    assert(EspNativeMonsterDeathConsequence_priority(1U, 1U) == 3U);
    assert(EspNativeMonsterDeathConsequence_priority(1U, 0U) == 5U);
    /* Invalid inputs must not draw or plan a drop. */
    {
        EspNativeGameplayMonsterRecord m = {0};
        EspNativeGameplayMonsterDropSpawnPlan plan = {0};
        uint32_t rngCalls = 0, roll = 0;
        uint16_t sound = 0;
        clear(2U);
        assert(!prepareDeathAfterXp(NULL, &m, 0U, &rngCalls,
                                    &plan, &sound, &roll));
        assert(!prepareDeathAfterXp(&runtime, NULL, 0U, &rngCalls,
                                    &plan, &sound, &roll));
        assert(!prepareDeathAfterXp(&runtime, &m, 2U, &rngCalls,
                                    &plan, &sound, &roll));
        m.subtype = 14U;
        assert(!prepareDeathAfterXp(&runtime, &m, 0U, &rngCalls,
                                    &plan, &sound, &roll));
        assert(callCount == 0U && live.cursor == 0U);
    }
    /* Adversarial plan failure: staging draws but existing direct/splash
     * world snapshots must restore the RNG and prevent sound publication. */
    {
        EspNativeGameplayMonsterRecord m = {5};
        EspNativeGameplayMonsterDropSpawnPlan plan = {0};
        uint32_t rngCalls = 0, roll = 0;
        uint16_t sound = 0;
        clear(0U);
        Random_t snapshot = live;
        failDrop = 1;
        assert(!prepareDeathAfterXp(&runtime, &m, 0U, &rngCalls,
                                    &plan, &sound, &roll));
        assert(sound == 5095U && callCount == 3U && live.cursor == 2U);
        live = snapshot; /* same snapshot operation used by both call sites */
        assert(live.cursor == 0U);
        assert(plan.outcome == 0U);
    }
    puts("MONSTER-DEATH-P1 PASS: 100 real C normal/gib cases, 14 families "
         "(special fail closed), RNG order, XP boundary, drop rollback, P2/P3/P5");
    return 0;
}
"""
host = (host.replace("__TABLE__", table.group(0))
            .replace("__CONSUME__", consume)
            .replace("__PREPARE__", prepare))

with tempfile.TemporaryDirectory(prefix="doom-monster-death-p1-") as directory:
    src = Path(directory) / "owner.c"
    exe = Path(directory) / "owner"
    src.write_text(host)
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
         "-pedantic", "-I", str(HEADER.parent), str(src), "-o", str(exe)],
        check=True
    )
    subprocess.run([str(exe)], check=True)

print("MONSTER-DEATH-P1 PASS: source gates, rollback/commit frontier and real compiled C")

# GAME-AI-14 A2-P0: permanent original special-death contract (host only).
subprocess.run(["python3", str(ROOT / "ESP32/test/game_ai14_special_death_host.py")], check=True)
