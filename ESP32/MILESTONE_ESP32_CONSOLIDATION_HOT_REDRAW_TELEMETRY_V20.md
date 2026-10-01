# Milestone ESP32 consolidation — hot redraw telemetry V20

Date: 2026-10-01  
Branch: `agent/esp32-consolidation-interaction-diagnostics-v17`  
Base for V20: V19 docs head `746052e51c07d886a482cc5b7ced3a88a484885b`  
Hardware-tested code boundary: `552ec9c529d1bd31a656c9847b0b267bacd713fa`  
Status: **REAL-CYD PASS**

## Goal

Remove serial-heavy success telemetry from the normal per-redraw path while
preserving all rendering/gameplay behavior and all important failure/recovery
witnesses.

## Scope

Only log classification changed. No FNV guard, rendering decision, cache
decision, input behavior, gameplay mutation, rollback owner or presentation
algorithm was removed.

Moved to TRACE:

```text
[NATIVEFRAME] BSP / WALL
[WEAPON] DRAW
[FACINGLABEL] REFRESH / PAINT / CLEAR
[RESIDENTGAMEPLAY] FRAME
[ACTIONENGINE] FRAME
[ACTIONFEEDBACK] PAINT / CLEAR / REFRESH
[DOORANIM] FRAME
[DYNAMICLINES] FRAME
```

Kept visible at INFO/ERROR as appropriate:

```text
[NATIVEFRAME] FAILED
[NATIVEFRAME] WALL-CACHE-FAILED / FALLBACK / READ-FAILED
[NATIVEFRAME] LEGACY_GUARD
[NATIVEFRAME] RETRY
[NATIVEFRAME] RECOVERED
[RESIDENTGAMEPLAY] RENDER-FAILED
[ACTIONFEEDBACK] FAILED
[VIEWFLASH] FAILED
[DOORANIM] ARM / COMPLETE
```

## CI / ELF

CI #1220: **SUCCESS**

```text
static RAM       45064 B
linked Flash    759197 B
firmware.bin    759568 B
artifact id     11157510597
artifact digest sha256:f891d1f121611cade66ed43eb9064919fa2da5336d39ac27d8a97f924802aded
firmware sha256 956d0593fa16366448736ee602eba321f217a94b106c4dbcf7286503afe976e8
ELF sha256      559f03b35ab5950ce4e9ee51c4fe4c128e81975c708001b6c7aa52952a3b4c10
active wraps    49
```

Versus V19:

```text
static RAM       45064 -> 45064 B       0 B
linked Flash    762021 -> 759197 B   -2824 B
firmware.bin    762384 -> 759568 B   -2816 B
active wraps        49 -> 49             0
```

Local hardware build:

```text
static RAM       45064 B
linked Flash    759213 B
firmware.bin    759584 B
```

The 16-byte local/CI flash-image difference matches the prior environment
pattern and does not affect static RAM.

## Real-CYD validation

Validated path includes:

```text
cold boot
 -> MENU_MAIN
 -> START
 -> full intro
 -> bounded disposal
 -> Entrance bootstrap
 -> FIRST_FRAME 71ca7465
 -> ENGINESESSION READY shapeData=0x0 mediaTexels=0x0
 -> repeated FORWARD
 -> TURN
 -> resource pickup x2
 -> regular door open
 -> movement through doorway
 -> stable ALIVE
```

The INFO transcript is materially smaller. The targeted success redraw families
are absent.

A real legacy compact renderer recovery still occurs and proves that important
diagnostic visibility was preserved:

```text
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED ...
```

Stable gameplay memory:

```text
heap=93448 heap8=27524 largest8=18420
```

## Result

V20 is hardware-valid. Normal INFO no longer pays the serial cost of repeated
redraw success summaries, while failure/recovery witnesses remain observable.
The user reports a clearly noticeable improvement in interaction smoothness
across the cumulative consolidation sequence.

This branch now spans V17 through V20 and should be merged before the next
consolidation family is started.
