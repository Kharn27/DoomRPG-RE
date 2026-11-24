# PERF-GAME-P1 — instrumentation candidate, PENDING CYD

Issue [#447](https://github.com/Kharn27/DoomRPG-RE/issues/447) in epic [#453](https://github.com/Kharn27/DoomRPG-RE/issues/453).
Base main: e84f2f9498fca0c2172af4c0ef03742e1d701b18.
Branch: agent/esp32-perf-game-p1-baseline.

**This is a measurement candidate, not a CYD PASS or an optimization.** No scheduling, AI, RNG, collision, render, SAVE, audio or input policy changes intended. One live member per service and serialized one-probe-in-flight remain intact. No framebuffer, PSRAM, ZIP, or heap allocation by the observer.

## Measurement contract

The normal \`esp32-cyd\` firmware has temporary \`DOOMRPG_PERF_GAME_P1=1\`. For a local A/B comparator, switch only the build flag to 0, rebuild the *same* normal environment and label firmware provenance; do not use bringup/trace as a memory/performance comparator. All observer hooks are compile-time disabled with inline no-ops when flag=0.

The observer has one loopTask writer, two fixed cohorts (\`pre\` historical activationCount < 16; \`mass\` >=16), bounded static histograms and **two cumulative aggregate log lines every 8 completed turns**. The 24 activations are historical, not 24 living or on-screen monsters. p50/p95 output is the *upper limit of a power-of-two millisecond bucket*, **not exact quantiles**; max values are exact microseconds to the clock resolution.

- \`wall\`: first active-sequence service entry through completion, including gaps and external work.
- \`service\`: sum of time within calls to \`ActiveSequence_service\` (wall-clock microseconds, may include RTOS preemption and pre-existing Serial logging, **not pure CPU cycles**).
- \`gap\`: sum of inter-service gaps after previous return until next entry. This may contain rendering, audio, input polling, other services, delay(5) and RTOS scheduling.
- \`member\`: immediate probe plus movement/attack transaction only. No added member printing.
- \`composedFrames\`/\`framePresents\`: calls to the native composed frame and their published physical frames, plus preview count and max LCD present microseconds. **Direct UI/feedback presents outside the frame compositor are not measured here**.
- \`inputResponses\`: semantic accepted touch through the end of the consuming resident service. This is **neither raw physical touch→interrupt nor a confirmed world commit**. Combat-blocked taps and one-slot input BUSY are counted without queuing or policy changes.
- \`aborted\`: explicitly terminated/fail-closed sequencer paths; not included as successful turn latency.

Measured clocks: ESP-IDF \`esp_timer_get_time()\`, monotonic microseconds since timer initialization. Calling the clock adds some cost. Initial first-member work may be recorded in a cohort before the sequence wrapper learns of a transition; member samples are categorized directly by its captured \`activeCount\`.

## Reproducible CYD protocol

1. Build and flash **normal** \`esp32-cyd\` at the PR's exact commit; record SHA, PlatformIO flags and serial log filename. The firmware does *not* currently self-report binary SHA in Serial.
2. From the same Sector 1 SAVE/checkpoint, follow an identical move/facing/weapon path and RNG/control sequence. Record baseline before wakeup then activate the clustered eight (about 24 historical activations / seven handled). Continue for **at least 8 full turns after the wakeup**.
3. Exercise on-screen, off-screen, occluded and entry/exit visibility, subtype4 three-goal, no-move collision, multiple attacks. Annotate cases from gameplay/serial, not inferred solely from \`activeCount\`.
4. Capture \`[PERFGAMEP1]\`, \`[MONSTERACTIVESEQ] BEGIN/COMPLETE\`, \`[MONSTER3GOAL]\`, \`[VIEWANIM]\`, \`[MONSTERRETAL]\`, \`[RAMBUDGET]\`, \`[AUDIO*]\` and state/RNG/topology fingerprints where already available.
5. If assessing measurement overhead, perform an independent A/B run with macro OFF and identical scenario at normal env; use pre-existing log landmarks since the OFF binary has no PERF lines. Note that exact gameplay/RNG equality is **not yet proven** and do not claim quantitative speedups from unlike runs.
6. Check heap8/largest8/DMA, \`shapeData/mediaTexels=NULL\`, watchdog/panic and music underflow/recovery and SFX gaps. Report any perceptible touch regression from instrumentation.

## Remaining P1 gates (do not close #447 yet)

The new host fixture tests observer accumulation, cohorts, aborted turns and input token attribution, **not the actual active-sequence implementation with mocked 7–24 live/dead members**. The existing \`monster_active_sequence_budget_host.c\` tests only the budget predicate, not complete AI/RNG/oracle behavior. Need to add true owner oracle, paired state/RNG/topology fingerprints, committed-action latency probes, complete direct UI-present counts / redraw reasons, and numeric thresholds after a real baseline. Only then consider P2–P4; no guessed gains and no premature parallelism.
