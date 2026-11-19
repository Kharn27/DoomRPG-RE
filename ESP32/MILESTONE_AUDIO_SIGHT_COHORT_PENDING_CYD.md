# AUDIO-SIGHT-COHORT — first detection cry holds all same-frame attackers (PENDING CYD)

**Repository:** Kharn27/DoomRPG-RE; **base main exact SHA** `9c1d17b522fc9f6be0983581ce035fb05cf74f62` (19 November 2026, PR #390 already merged). Working branch `agent/audio-monster-sight-cohort`. Real device required before PASS.

## Field witness

`platformio-device-monitor-261119-123629.log`, classic CYD boot, 3701 lines / 479904 bytes. Source file does not include the Git firmware SHA nor PlatformIO environment, so log-to-branch attribution cannot be asserted from Serial alone.

- Lines 1397–1403: same rendered frame activates `sprite=220 subtype=0 mType=0` (zombie) and `sprite=264 subtype=3 mType=3` (imp). The native ambience emits the **one original highest-subtype winner** sight cry `5071` belonging to sprite264 (`newlyActive=2`). The next monster turn immediately attacks with zombie sprite220: lines 1417–1429 `MONSTERATKVIS ARM` and `MONSTERSFX ATTACK id=5047`. **No SIGHT-HOLD for sprite220**.
- Lines 2433–2460: Malwrath `sprite=322 subtype=6` activates alone, sound `5097`, and its first attack correctly observes `[MONSTERSFX] SIGHT-HOLD probe=1 sprite=322 guardMs=350`.
- Lines 3289–3322: same-frame activation of sprite272 subtype4 (no sight in original SFX matrix) and Pinky sprite279 subtype5 (sight `5093`); only one original cry, the Pinky attack occurs after door presentation. Other monster types have different original sight/no-sight entries.

**Important separate firmware provenance check:** 99 `[AUDIOINTENT] OVERFLOW drop=oldest capacity=16` and **zero** `[AUDIOINTENT] AUDIT` lines in this capture. It does **not** have the `ACK/AUDIT` signatures verified with PR #390 and therefore is **not valid evidence of a FIFO regression on the merged #390 firmware**. This sound timing fix does not change FIFO ownership; user must flash the new branch's normal `esp32-cyd` build for the next test.

## Root cause

The native `EspNativeMonsterAmbience_service` intentionally chooses exactly one highest-subtype new monster in a rendered activation group for sound, matching the original game. However its `sightSprite` / `sightHoldUntilMs` guard tracks **only that sound winner**. `EspNativeGameplayMonsterAttackVisual_service` calls `EspNativeMonsterAmbience_sightSettling(attackerSprite)`, so an concurrently newly activated zombie has no guard although it is still covered by that one detection cry.

## Fix / preserved invariants

- Preserve **one and only one highest-subtype sight sound per render activation batch**, same original 14×12 sound ID matrix, RNG private to audio, and 350ms presentation guard.
- Store four bounded, short-lived activation-ordinal ranges, not one winner sprite. When a sound is published, its entire newly activated ordinal cohort becomes eligible for sight settling. The existing AttackVisual guard now delays *any* attacking member of that cohort; attack probe and player turn remain pending, gameplay RNG untouched.
- Independent overlapping groups keep distinct deadlines. At bounded capacity, a fifth simultaneous group extends the most recently activated range (conservative hold instead of losing a live guard). Expired slots are reused. Zero heap allocations, no new threads, no audio file reads, no SD/PAK/ZIP work, no changes to monster world, render sprites or save ABI. SOUND OFF and restored activation prefix do not create sight holds; families with no sight cry do not gain artificial delay.
- Host `audio_monster_ambience_host.c` now checks the exact Zombie+Imp case (one Imp cry, two guarded monsters), same-frame Imp+Pinky, 350ms expiry, independent overlapping batch deadlines, capacity coalescing, SOUND OFF, SAVE/LOAD restored prefix, map reset. Existing `audio_monster_sfx_all_host.py` still asserts original sound IDs and unchanged animation sound timing.

## Validation contract

1. CI **push + PR** builds normal/inverted/trace/bringup and host suites.
2. Flash actual branch's **normal `esp32-cyd`** firmware. In Entrance, open the room with Zombie subtype0 and Imp subtype3 (the previous log's same-frame activation), listen for one detection cry clearly before the first Zombie attack animation; expect `[MONSTERAMBIENT] SIGHT sprite=264 ... newlyActive=2 guardOrdinals=0..1 guardMs=350` (ordinal positions are scenario-dependent), followed by `[MONSTERSFX] SIGHT-HOLD probe=... sprite=220 ...`, and only then `MONSTERATKVIS ARM sprite=220`.
3. Repeat Malwrath alone and Pinky plus other same-frame monsters. No extra sight cries on subsequent frames, no attack/retaliation duplication, no lag when no sight cry.
4. Watch `[AUDIOINTENT] AUDIT` and confirm no FIFO-overflow regression; `[SFXGEN] DROP`, music `underruns/recoveries`, heap8, reset, `shapeData=mediaTexels=NULL`. Do not treat old log with 99 overflows as a post-#390 test.
5. Post-test documentary commits **only after material PASS**. Do not merge automatically.

The user-reported sprite rendering anomaly remains outside this strictly audio-presentational milestone.
