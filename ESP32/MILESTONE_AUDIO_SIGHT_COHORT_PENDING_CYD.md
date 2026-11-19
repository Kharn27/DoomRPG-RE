# AUDIO-SIGHT-COHORT — first detection cry holds all same-frame attackers (CYD PASS, bounded)

**Repository:** Kharn27/DoomRPG-RE; **base main exact SHA** `9c1d17b522fc9f6be0983581ce035fb05cf74f62` (19 November 2026, PR #390 already merged). Working branch `agent/audio-monster-sight-cohort`. Hardware validation achieved on the **exact bounded scenario** below; code CI success, no claimed proof of all families or injected failures.

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

## Hardware validation — 19 November 2026 (bounded CYD PASS)

User feedback: “Cool t'as meme corrigé le son du tir du zombie !! … pas entendu le cri du imp … pas sûr qu'il y ait un cri par monstre … c'est carré l'enchaînement cri et combat.” This is a **positive subjective pass of the ordering**, not a requirement to create per-monster additional cries. The attack sound WAV and its pose+150ms emission are **unchanged** by this patch: delaying the previously unguarded zombie first attack makes the existing sound distinct from the detection.

Capture: `platformio-device-monitor-261119-134918.log`; **1,630 lines / 203,315 bytes / SHA256 `1542312bd402fc70274ac1a9382c496883527abfc21585f82f0c7c755c31afa6`**. True classic CYD: ESP32-D0WD-V3 / 4 MiB flash / zero PSRAM (boot lines 12–18); displayed alternate panel inversion=off. The Serial has **no embedded git SHA or PIO environment**, so code HEAD attribution follows the user test context, not a firmware-build attestation.

### Exact source-to-playback route

1. L1304–1312: selecting the secret room snaps open two nonregular lines 68 and 67; `[MONSTERACT] ACTIVE sprite=220 subtype=0` (Zombie, ordinal0) and `sprite=264 subtype=3` (Imp, ordinal1) arrive **in the same render batch**. **Exactly one** `[MONSTERAMBIENT] SIGHT sprite=264 mType=3 id=5070 newlyActive=2 guardOrdinals=0..1 guardMs=350`. This is intentionally Imp's sample and not a missing separately scheduled Zombie cry; original one-highest-subtype selection unchanged.
2. L1313 and 1334: `[SFXGEN] START id=5070` then `END id=5070`, 27,342 output frames, no playback drop. At L1317–1330 **two doors each publish and play SFX 5063**, overlapping the sight interval. This is a plausible audible reason for not distinguishing the Imp cry; the log **proves the pipeline played the sample but cannot prove perceived audibility**.
3. L1331–1336: Zombie probe1 delivered, `[MONSTERSFX] SIGHT-HOLD probe=1 sprite=220 guardMs=350` before `[MONSTERATKVIS] ARM probe=1 sprite=220`; the Imp's cry also reaches `SFXGEN END` before Zombie pose ARM. This is the regression's core pre/post witness.
4. L1338–1344: Zombie emits **unchanged** attack `id=5046` `timing=pose+150ms`; `[SFXP1] MIX id=5046` covers all 11,466 output frames; then final-idle and probe complete. A second Zombie attack follows at L1381–1390, also `id=5046`, with no *second* sight cry (correct once-only behavior). L1348/1394 two successful native retaliation commits. L1406–1428 Zombie killed; L1603–1624 player attacks the Imp later, `5047 + 5085` START/END, but Imp itself does not attack in this capture.
5. Overall log: **one SIGHT batch / one SIGHT-HOLD / two ATTACK id5046**, 17 `SFXGEN START` and 17 matching `END`, **zero** `SFXGEN DROP`; 11 SFXP1 MIX and 3 SFXP2 MIX. `[AUDIOINTENT] OVERFLOW=0`, `CURSOR-GAP=0` occurrences, two handoff `AUDIT` both queueOverflow=0 consumerCursorGap=0 audibleSfxDropped=0 pending=0 (menu/intro, **not** final gameplay teardown); 33 `PUBLISH` plus one `STOP_ALL`. All nine ENTRANCEMUSIC STREAM probes `underflows=0 recoveries=0 maxSilentFrames=0 gapsOver500Ms=0`; two MENU/INTRO FINAL `underruns=0 recoveries=0`. `RAMBUDGET RESERVE ... HEADROOM_OK`; heap8 gameplay stable at 56,576 B over later sampled period; no panic/WDT/monster visual rollback marker found.
6. Coverage limitations: capture ends **in gameplay** after the Imp is hit, not at MENU/EXIT; no `[AUDIOINTENT] AUDIT` at gameplay track close, no teardown/memory-release proof. Only this **Zombie+Imp** cohort validates the new hardware guard; multiple later simultaneous cohorts/saturation, a solo Malwrath/Pinky on this firmware, SOUND OFF, LOAD restore and independent hardware attacks by each member are **host-tested or previous-firmware-only**, not proven by this one log. Serial can't instrument acoustic masking. Rendering anomalies outside this PR.

### Merge boundary

Code HEAD tested by user context: `0a9403a1b9399b03029e196802d86750063f5a92`. CI success at that HEAD:
- push [#44518564458](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44518564458)
- pull request [#44518631598](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44518631598).

After user test, **Markdown-only** report edit(s); no source changes. The repository's `main` advanced independently after PR creation (last observed `2e8f070cddbd4409c222e9d9c6f4fad4ec7e7193`); reconcile any actual GitHub PR mergeability/conflict before merging, retaining this exact-code hardware evidence distinct from newly integrated changes. **Do not merge without explicit user request.**
