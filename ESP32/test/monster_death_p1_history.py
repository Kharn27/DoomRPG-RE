"""Strict inverse of MONSTER-DEATH-P1, preserving frozen SDL-F25 oracles.

Every edited span must be unique and present; restore the exact original
source bytes before existing audio/architecture history inverses are applied.
Do not relax historical Git blob identity checks.
"""

EDITS = (
    ("#include \"esp_native_gameplay_monster_combat.h\"\n#include \"esp_native_monster_death_consequence.h\"\n",
     "#include \"esp_native_gameplay_monster_combat.h\"\n"),
    ("/* MONSTER-DEATH-P1: shared post-XP, pre-drop-commit stage for the direct\n * and barrel-radius pathways. The CALLER has already applied XP (including\n * any level-up RNG), and still owns the complete world/player/RNG rollback.\n * This prepares no persistent drops and publishes no sound: strictly\n * death-sound RNG (or gib 5091 without sound RNG), then one drop RNG word,\n * then a pure drop plan. A failed plan MUST be undone by the caller's\n * existing transaction snapshot. Special deaths are rejected upstream. */\nstatic int prepareDeathAfterXp(\n    DoomRPG_t* runtime,\n    const EspNativeGameplayMonsterRecord* before,\n    uint8_t gib,\n    uint32_t* ioRngCalls,\n    EspNativeGameplayMonsterDropSpawnPlan* outPlan,\n    uint16_t* outSound,\n    uint32_t* outDropRoll) {\n    if (runtime == NULL || before == NULL || ioRngCalls == NULL ||\n        outPlan == NULL || outSound == NULL || outDropRoll == NULL ||\n        before->subtype >= MONSTER_SUBTYPE_COUNT || gib > 1U ||\n        specialDeath(before->subtype)) return 0;\n    *outSound = gib != 0U\n        ? 5091U : consumeDeathSound(runtime, before->subtype, ioRngCalls);\n    *outDropRoll = (uint32_t)DoomRPG_randNextInt(EspNativeProcessRng_live());\n    ++(*ioRngCalls);\n    return EspNativeGameplayMonsterDrop_prepare(*outDropRoll, before, outPlan);\n}\n\nstatic void logFrame(const MonsterCombatPending* pending,\n",
     "static void logFrame(const MonsterCombatPending* pending,\n"),
    ("            if (!prepareDeathAfterXp(\n                    runtime, &targetBefore, (uint8_t)gib, &rngCalls,\n                    &dropPlan, &consequenceSound, &dropRoll)) {",
     "            if (gib) {\n                consequenceSound = 5091U;\n            }\n            else {\n                consequenceSound = consumeDeathSound(runtime,\n                                                      target->subtype,\n                                                      &rngCalls);\n            }\n            dropRoll = (uint32_t)DoomRPG_randNextInt(EspNativeProcessRng_live());\n            ++rngCalls;\n            if (!EspNativeGameplayMonsterDrop_prepare(\n                    dropRoll, &targetBefore, &dropPlan)) {"),
    ("            memset(&drop, 0, sizeof(drop));\n            if (!prepareDeathAfterXp(\n                    runtime, &before, 0U, &rngCalls,\n                    &drop, &sound, &dropRoll) ||",
     "            sound = consumeDeathSound(runtime, before.subtype, &rngCalls);\n            dropRoll = (uint32_t)DoomRPG_randNextInt(EspNativeProcessRng_live());\n            ++rngCalls;\n            memset(&drop, 0, sizeof(drop));\n            if (!EspNativeGameplayMonsterDrop_prepare(\n                    dropRoll, &before, &drop) ||"),
    ("(void)EspNativeAudioIntent_publish(consequenceSound,0U,\n                EspNativeMonsterDeathConsequence_priority(\n                    (uint8_t)lethal, (uint8_t)gib));",
     "(void)EspNativeAudioIntent_publish(consequenceSound,0U,lethal?5U:2U);"),
    ("        const uint8_t priority = EspNativeMonsterDeathConsequence_priority(\n            (uint8_t)((packed & MONSTER_SPLASH_AUDIO_DEATH_FLAG) != 0U),\n            0U);",
     "        const uint8_t priority =\n            (packed & MONSTER_SPLASH_AUDIO_DEATH_FLAG) != 0U ? 5U : 2U;"),
)


def restore_pre_monster_death_p1(current: str) -> str:
    for new, old in EDITS:
        assert current.count(new) == 1, (
            "MONSTER-DEATH-P1 source drift", new[:100])
        current = current.replace(new, old, 1)
    return current
