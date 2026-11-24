"""Strict inverse of BARREL-RADIUS-SFX additive code for frozen SDL-F25/F27 blobs.

Never rewrite historical SHA witnesses. Only remove exact source additions;
an unexpected production edit must make the old immutable-body check FAIL.
"""

MONSTER_ADDITIONS = (
    """/* 16 barrels per chain, at most 8 monster hits per blast: 256B static.
 * Resource IDs fit 15 bits; high bit encodes death priority 5 vs pain 2. */
#define MONSTER_SPLASH_AUDIO_CAPACITY (16U * ESP_NATIVE_GAMEPLAY_SPLASH_MAX_HITS)
#define MONSTER_SPLASH_AUDIO_DEATH_FLAG 0x8000U
""",
    """/* Non-allocating post-commit sound intents, never part of world mutation. */
static uint16_t splashAudioCues[MONSTER_SPLASH_AUDIO_CAPACITY];
static uint32_t splashAudioSequence;
static uint8_t splashAudioCount;
""",
    """    splashAudioSequence = 0U;
    splashAudioCount = 0U;
""",
    """    splashAudioSequence = 0U;
    splashAudioCount = 0U;
""",
    """    splashAudioSequence = sequence;
    splashAudioCount = 0U;
""",
    """/* A barrel may roll back later chain waves: never play a monster sound
 * while the splash is speculative. Release the exact already-selected legacy
 * SFX IDs only at the enclosing ActionEngine irreversible BARREL COMMIT.
 * No RNG, renderer, SD, PAK or heap work occurs in this audio publisher. */
int EspNativeGameplayMonsterCombat_publishSplashAudioAfterCommit(
    uint32_t sequence) {
    uint8_t i;
    uint8_t emitted = 0U;
    if (sequence == 0U || splashAudioSequence != sequence ||
        splashSequenceActive != 0U) return 0;
    for (i = 0U; i < splashAudioCount; ++i) {
        const uint16_t packed = splashAudioCues[i];
        const uint16_t resource = (uint16_t)(packed & 0x7fffU);
        const uint8_t priority =
            (packed & MONSTER_SPLASH_AUDIO_DEATH_FLAG) != 0U ? 5U : 2U;
        if (EspNativeAudioIntent_publish(resource, 0U, priority)) ++emitted;
        printf("[MONSTERRADIUS] SFX-COMMIT seq=%u ordinal=%u/%u resource=%u priority=%u emitted=%u rollback=closed\\n",
               (unsigned int)sequence, (unsigned int)(i + 1U),
               (unsigned int)splashAudioCount,
               (unsigned int)resource, (unsigned int)priority,
               (unsigned int)emitted);
    }
    splashAudioSequence = 0U;
    splashAudioCount = 0U;
    return 1;
}

""",
    """        splashAudioSequence = sequence;
        splashAudioCount = 0U;
""",
    """    /* Capacity is checked BEFORE mutating health, drops, or RNG. If more
     * victims than the bounded 16x8 contract, roll back the entire chain. */
    if (splashAudioSequence != sequence ||
        (uint16_t)splashAudioCount + count >
            MONSTER_SPLASH_AUDIO_CAPACITY) {
        i = 0U;
        goto splash_failed;
    }
""",
    """        /* Exactly the RNG-selected original cue: no second random draw. */
        if (sound != 0U) {
            splashAudioCues[splashAudioCount++] =
                (uint16_t)(sound | (hpAfter == 0
                    ? MONSTER_SPLASH_AUDIO_DEATH_FLAG : 0U));
        }
""",
)

ACTION_ADDITION = """            /* Only NOW is the full barrel + multi-wave monster transaction
             * irreversible. The native monster combat owner already consumed
             * the exact legacy death/pain sound RNG; publish its bounded
             * sound intents once, never on a rolled-back blast. The visible
             * 5061 explosion remains anchored to wave-1 frame 0 above. */
            if (barrelSplashCount != 0U ||
                barrelSplashTotalHits != 0U) {
                if (!EspNativeGameplayMonsterCombat_publishSplashAudioAfterCommit(
                        pending.sequence)) {
                    printf("[BARRELRADIUS] SFX-DEFER seq=%u reason=monster-audio-sequence-invalid worldCommitted=yes\\n",
                           (unsigned int)pending.sequence);
                }
                EspEntranceMusic_pumpGameplay();
            }
"""


def restore_pre_barrel_splash_audio(name: str, current: bytes) -> bytes:
    if name == "esp_native_gameplay_monster_combat.c":
        spans = MONSTER_ADDITIONS
    elif name == "esp_native_gameplay_action_engine.c":
        spans = (ACTION_ADDITION,)
    else:
        raise AssertionError("BARREL-RADIUS-SFX unknown source " + name)
    result = current
    for addition in spans:
        raw = addition.encode("utf-8")
        assert result.count(raw) >= 1, (
            name, "BARREL-RADIUS-SFX exact delta drift", raw[:70])
        result = result.replace(raw, b"", 1)
    return result
