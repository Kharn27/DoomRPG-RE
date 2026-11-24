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
- \`inputResponses\`: semantic accepted touch through the end of the consuming resident service. This is **not raw physical touch→interrupt and not by itself a world commit**. `committedActions` separately times accepted touch→successful final MOVE, MOVE-DIALOG or TURN marker **after rendering and rollback checks**; this is an upper-bound confirmation latency, not the instant of the early logical commit. SELECT/PASS/weapon are not included. Combat-blocked taps and one-slot input BUSY are counted without policy changes.
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

The new host fixture tests observer accumulation, cohorts, aborted turns and input token attribution, **not the actual active-sequence implementation with mocked 7–24 live/dead members**. The existing \`monster_active_sequence_budget_host.c\` tests only the budget predicate, not complete AI/RNG/oracle behavior. Need to add true owner oracle, paired state/RNG/topology fingerprints, committed-action latency probes for SELECT/PASS/other actions, complete direct UI-present counts / redraw reasons, and numeric thresholds after a real baseline. Only then consider P2–P4; no guessed gains and no premature parallelism.


## P1b — completed-turn frame attribution (candidate)

First CYD acquisition prompted a narrower follow-up: actual delivered buckets 0 / 1–3 / 4–7 / 8+ (IDs 0–3), independent of historical prefix. [PERFGAMEP1B] records cumulative compositor time within sequencer calls, between calls while a turn is open, and outside the sequencer turn. LCD time is a subset of frame time; nonFrameServiceUs subtracts measured compositor time from service wall time, **not pure AI CPU**. Pre-existing logs, audio pumps after frame measurement, transactions, preemption remain in residual. Aborted turns excluded from completed buckets. Final attack retaliation may outlive sequencer completion; outside is not synonymous with ambient. No direct UI present census, no visibility decision, no gameplay or budget change. Same normal env and scenario for next CYD capture. The true simulation oracle and finer AI/transaction timings remain pending.

## P1b CYD acquisition 24 Nov 2026 / P1c candidate

Capture platformio-device-monitor-261124-125316.log SHA256 8fe7675d00a1e6f2d2a00c93a451db34c9d5cb602aafd48888616ef4fe74d920, slot05 Sector 1. Eight completed turns at report; bucket2 four turns, seven delivered each, service 9187251 us, 34 composed frames 4354019 us, residual 4833232 us, LCD subset 1329800 us; outside 111 frames 12884903 us. Ten completed turns in full capture, eleventh incomplete. Zero music underflow reported, heap8 51868 largest8 36852 at end; no exit audit. Not an A/B: different checkpoint/RNG from P1a. Candidate P1c splits selected immediate attack probe and movement transaction including post-goal chain via disjoint clocks and frame deltas. No scheduling or rendering change. Cumulative phases include failed attempts, not exclusively completed turns; residual is not pure CPU. Same scenario, seek 16 completed turns, inspect PERFGAMEP1C. Existing historical restoration remains exact. Further planner/publisher split and full oracle pending.

## P1c CYD / P1d candidate

Capture platformio-device-monitor-261124-130200.log SHA256 455a031c7f2799b17c590150b1a1758df7702905fb1ee3b310b8da49bb98399f. Slot05 Sector1 restored, RNG differs from P1b. 24 completed turns, 151 delivered, zero aborted. Immediate probe 50288 us / 151, max436 us; movement+post-goals 47715482 us, 167 frames 19766013 us, residual 27949469 us, max1457565 us. Bucket2 20 turns, 43895693 us service, 17893256 us compositor; bucket3 one turn, 3869116 us service and 1631874 us compositor between services. Worst sequence wall7657996 us, gap3788880 us, 171 paused calls. Music underflows3477 recoveries2 maxSilentFrames1915 maxPublishGapMs571; SFXGEN DROP 5081 reader-caught-up, not audio PASS. Heap8 51756 largest36852, menu return successful. P1d adds four nested call scopes: first planner, first publisher, continuation planner, continuation publisher. P1C parent includes these; do not sum parent and child. Residual still wall time (not CPU); unassigned parent remainder includes post-goal attack, RNG guards and logs. No behavior changes, no audio fix yet. Hardware and CI pending. Full deterministic oracle remains pending.

## P1d CYD / P1e candidate

Capture platformio-device-monitor-261124-131034.log SHA256 c1b15041a9d1b38507783852c98b96a342d4b7887919ac21574e53b7ffee8cf6. Slot05 Sector1. 24-turn report: 170 delivered, zero aborts, zero paused calls. Movement parent 46101972 us; initial planner 17958182 us/170; continuation planner 5372022 us/38; initial publication 17501127 us including 15599194 us compositor; continuation publication 4487972 us including 3963855 us compositor. Planners 23330204 us (50.6% parent). Worst sequence wall3773407 us. Last music stream 92173 ms, zero underflows/recoveries, max publish gap550 ms, one ISR gap over90us, heap8 51588 largest36852. No exit audit. NPC issue #444 updated independently with sprite93 event64 no eligible witness. P1e adds nested goal-plan and cardinal-trace timings, each trace counted even when returning early. Trace scopes include readiness LOS outside goal plan where applicable; cannot subtract aggregate trace totals from goal totals without matching scope. No gameplay change, no rebase, no merge. CI/hardware and deterministic oracle pending.

## P1f: bounded directed-trace memoization shadow

P1e hardware report at 8 turns: 712 initial +290 continuation traces, 5034097 us trace wall time, 5046162 us aiGoalPlan wall time. Both implementations currently scan global line and sprite arrays for each tested tile. First reversible experiment: memoize only identical directed trace requests within one synchronous aiGoalPlan; no carry across goals, monsters, doors, or turns. Full key includes endpoints, attacker, mask and strict-special. Only 0/1 stored; negative outcomes stay reference. Capacity 36 per planner, fixed BSS, no heap, full fallback. Every trace still executes reference body; shadow compares cached and actual result, returns actual result unconditionally. P1F reports calls/hits/mismatches. This is NOT the tile index or a speedup yet. Zero mismatches + useful hit rate required before enabling fast path. Reference logs retained. Host fixture covers directed key, mask, attacker, strict, negative, capacity and scope reset; CI and hardware pending. No merge/rebase. P1 deterministic full-turn oracle remains open.
