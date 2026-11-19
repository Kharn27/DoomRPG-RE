# AUDIO #377 — AudioIntent FIFO consumer ACK (pre-hardware design)

**STATUS: CYD SERIAL PASS — superseded by the actual hardware report**
[MILESTONE_PR390_AUDIO_INTENT_FIFO_CYD_PASS.md](MILESTONE_PR390_AUDIO_INTENT_FIFO_CYD_PASS.md),
test `platformio-device-monitor-261119-111849.log`. Original acceptance
checklist below is kept as the immutable **pre-test plan**, not as current
pending status. The tested code HEAD is `81a93de01c755491f26f6a1346d6f33e9fca17a0`;
this file is modified documentation-only after the hardware test.

## Evidence and baseline (real hardware, not this code)

The user capture `platformio-device-monitor-261119-020828.log` is **1986
lines / 257251 bytes**, SHA256
`570088d78f287d98994d5304e5d79c8617775957892bd92815c89f09b5f5219c`.
It contains **29** `[AUDIOINTENT] OVERFLOW drop=oldest capacity=16`
(lines 1305..1958), all after `LOAD V11 slot04`. Its first overflow is
`PUBLISH seq=17` and its last is `PUBLISH seq=45`. There are 26
`SFXGEN START`, 26 `SFXGEN END`, zero `SFXGEN DROP`, and music
`underruns=0 / recoveries=0`. This is a real queue eviction, **not
evidence of 29 audible SFX losses**.

The main baseline is exact SHA
`73520c22afa0e849807cd75de3540d9a5f826d45` (PR #387 merged;
PR #388 included), before these edits.

## Root cause and permanent fix

`esp_native_audio_intent.c` was retaining the 16 newest intent records
even when the sole real consumer in `esp_audio_entrance_music.cpp`
(`mixPendingSfxOne` via `nextAfter(sfxSeenSequence)`) had already
processed them. Every publish beyond sequence 16 thus evicted the oldest
historical entry and emitted a false-positive `OVERFLOW` warning even
while SFX successfully mixed.

Now:
- `nextAfter` remains a **non-consuming peek**. Three playback
  paths ACK only after a record was handled: STOP_ALL, generic PCM, and
  specialized 5045/5046/5060. On a PAK lease deferral, no ACK occurs.
- Explicit track/session `discardThrough` on music close and at the
  next track's initialization prevents stale effects/STOP_ALL replay.
  This is counted separately from playback ACK and error.
- Saturating, allocation-free counters are independent:
  `queueOverflow` (genuinely unprocessed record evicted on full ring),
  `consumerCursorGap` (missed IDs on an actual consumer advance),
  `audibleSfxDropped` (existing confirmed SFX PCM rejection/partial
  mix paths; **not** proof of audible DAC dropout);
  `consumed`, `handoffDiscarded` and `pending`.
- Final music teardown emits `[AUDIOINTENT] AUDIT ...`.
  A **real** >16 burst with stalled consumer continues to print
  `OVERFLOW`; it is not silently suppressed or hidden by adding RAM.
- Same 16 x 8-byte queue, unchanged resource IDs, priority metadata,
  call ordering, STOP_ALL, sound volume and master SOUND state. No new
  thread/ISR, second SD reader, heap/PSRAM buffer, ZIP, RNG or game turn.

## Tests / scope

`ESP32/test/audio_monster_sfx_queue_host.c` is compiled by the
already-wired `audio_monster_sfx_all_host.py` CI test. It covers
64 continuous ACKed crate/pickup/door publishes with **zero overflow**,
deferred non-consuming peeks, priority order, STOP_ALL then post-STOP
play, EXIT/MENU/LOAD V11 discard and monotonic sequence, a deliberately
stalled 20-event burst yielding exactly four true overflows and
four cursor skips, known PCM rejection counters, and reset.

Static checks in the same CI runner ensure ACK/discard integration at
the actual music consumer sites. Native CYD builds and all existing host
tests must be checked in GitHub CI; do **not** claim a local PlatformIO
build or physical validation before it happens.

## Real CYD acceptance (normal `esp32-cyd`)

1. Start new Entrance game; crate attack, pistol, pickup, open/close door
   (5063/5064), monster attack/damage, dialogue and HUB EXIT.
2. LOAD V11 slot04; repeat extended monster fights, pickups,
   opening/closing doors, then EXIT to MENU.
3. Collect full Serial including `[AUDIOINTENT] AUDIT`,
   `[SFXGEN] START/END/DROP`, `[SFXP1]`,
   `[ENTRANCEMUSIC] STREAM/FINAL`, heap and resident cleanup.
4. Expected if playback consumer keeps up:
   `queueOverflow=0`, `consumerCursorGap=0`,
   `audibleSfxDropped=0`, no `SFXGEN DROP`,
   and no music underruns; audio is perceptually checked by tester.
   A confirmed genuine burst/PCM loss must still be reported, not
   ignored. STOP_ALL and EXIT must not replay previous session effects.
5. Keep #377 **open** until CYD PASS and post-test documentation-only
   follow-up. No cross-issue audio timing claim (#382).

This milestone neither fixes nor claims to fix the separate music/ring
underflow case in #382.
