# Milestone ESP32 consolidation — hot input / turn telemetry V21

Date: 2026-10-01  
Branch: `agent/esp32-consolidation-hot-input-turn-telemetry-v21`  
Base main: `4a46ea17e3e397bc9870e10c06eda53786ae48b8`  
Hardware-tested code boundary: `ac5e11127f294a5e2d7d1127febb21214be94458`  
Status: **REAL-CYD PASS**

## Goal

Reduce the remaining serial cost on the normal input/movement/idle-turn path
without changing gameplay, renderer behavior, event semantics, monster ordering,
RNG consumption, rollback ownership, timers or presentation.

The normal `esp32-cyd` build remains INFO. Repetitive success-only telemetry is
compiled out through `DRPG_LOGT`; important semantic, failure, deferral and
recovery witnesses remain visible at INFO/ERROR.

## Code scope

Exactly five runtime translation units changed:

```text
ESP32/src/esp_native_gameplay_move_events.c
ESP32/src/esp_native_resident_gameplay.c
ESP32/src/esp_native_gameplay_monster_turn.c
ESP32/src/esp_native_gameplay_monster_movement.c
ESP32/src/esp_native_gameplay_monster_active_sequence.c
```

No gameplay/rendering algorithm or owner layout was changed.

### Generic move phases

The generic `logPhase(...)` summary in the move-event engine is now TRACE,
covering the repetitive normal phase names:

```text
EXIT-PREFLIGHT
ENTER-PREFLIGHT
EXIT
ENTER
```

Separate semantic/error witnesses remain INFO, including:

```text
[MOVEEVENT] BLOCK
[MOVEEVENT] DEFER
[MOVEEVENT] ROLLBACK
[MOVEEVENT] FRAME-FAILED
[MOVEEVENT] WORLD-READY
[MOVEEVENT] COMMIT
[MOVEEVENT] MESSAGE
```

### Resident touch and automap success

Moved to TRACE:

```text
[TOUCHFEEDBACK] FLASH
[TOUCHFEEDBACK] RESTORE
[AUTOMAP] UNCOVER reason=MOVE
[AUTOMAP] UNCOVER reason=MOVE-DIALOG
```

Failure paths such as touch-feedback SKIP, resident BUSY/FAILED/RENDER-FAILED,
MOVE-BLOCKED, MOVE-DEFER and rollback/defer paths remain visible.

### Idle monster-turn summaries

Moved to TRACE:

```text
[MONSTERTURN] ROTATE-NO-TURN
[MONSTERTURN] SCHEDULE
```

Three other families are classified conditionally rather than hidden wholesale:

- `[MONSTERTURN] COMPLETE ... candidates=0` is TRACE only when
  `specialAIDeferred == 0`; a nonzero deferred-special-AI count stays INFO.
- `[MONSTERMOVE] DEFER ... active-order-not-owned` is TRACE only for the strict
  `candidates == 0 && activeCount == 0` idle case; ambiguity/nonzero activity
  stays INFO.
- `[MONSTERACTIVESEQ] COMPLETE` is TRACE only when
  `activeCount == 0 && delivered == 0`; real delivered/active work stays INFO.

Movement probes, live movement commits, RNG-boundary failures, fatal probes,
active-sequence MEMBER/RANGED-MEMBER and attack/retaliation diagnostics are
unchanged.

## CI / ELF

Code commit:

```text
ac5e11127f294a5e2d7d1127febb21214be94458
```

CI #1225: **SUCCESS**

```text
static RAM       45064 B
linked Flash    757789 B
firmware.bin    758160 B
artifact id     11165147980
artifact digest sha256:2cff56fe041a090e89ef7a2b718d51feaae2240676db9c4932fc545c9e94c6bd
firmware sha256 2ea793c91c6fa23563d434f776b4c85f2d9e156c5374c5a5c07fc25c3d7552ca
ELF sha256      5dd4b8500282a1bd2cfe6d4e740469fb39b6171eb337ede79978e5b7c257ccac
active wraps    49
```

Versus V20:

```text
                         V20              V21           delta
static RAM               45064 B          45064 B          0 B
linked Flash            759197 B         757789 B      -1408 B
firmware.bin            759568 B         758160 B      -1408 B
active __wrap_*              49               49            0
```

Final INFO ELF inspection confirms that the unconditional hot target strings are
absent. The three conditional monster format strings remain in the ELF by
design because nontrivial branches still emit the same formats at INFO.

Critical INFO strings remain present, including
`NATIVEFRAME LEGACY_GUARD/RETRY/RECOVERED`,
`MOVEEVENT BLOCK/DEFER/ROLLBACK/FRAME-FAILED/WORLD-READY/COMMIT/MESSAGE`,
resident failure/defer witnesses, `MONSTERMOVE RNG-BOUNDARY-DEFER/FATAL-PROBE`
and active-sequence member diagnostics.

Local hardware build:

```text
static RAM       45064 B
linked Flash    757805 B
firmware.bin    758176 B
```

The +16 B local image difference matches the established environment pattern and
does not affect static RAM.

## Real-CYD validation

### Cold boot / Start Game / Entrance

The real classic CYD boots normally from the native PAK, enters MENU_MAIN,
executes START, runs the full intro and disposal/load handoff, then produces:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 ...
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93448 heap8=27524 largest8=18420
```

A banal Entrance MOVE now has the intended compact INFO shape:

```text
[RESIDENTGAMEPLAY] QUEUE ... action=FORWARD ...
[RESIDENTGAMEPLAY] MOVE ... committed=yes
```

The generic move-event phase summaries, touch FLASH/RESTORE and MOVE automap
success summary are absent. Normal TURN is likewise committed without the
rotation/scheduling noise.

### Semantic gameplay remains visible

The same run exercises:

```text
blocked movement
regular door open + movement through doorway
door-close move event COMMIT
opcode-26 enter dialog
dialog fast-forward/page/close/resume
armor, medkit, ammo and weapon pickups
crate transform/player attack
```

Meaningful diagnostics remain visible. Examples include:

```text
[LINECOLLISION] BLOCK ...
[RESIDENTGAMEPLAY] MOVE-BLOCKED ...
[MOVEEVENT] WORLD-READY ... enterDialog=opcode26 ...
[MOVEEVENT] COMMIT ... dialog=opened ...
[DOORANIM] ARM ...
[DOORANIM] COMPLETE ...
```

### Renderer recovery remains hardware-visible

Two real compact-renderer guard incidents occur during ordinary gameplay and
both recover successfully:

```text
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED ...
```

This directly proves that the V20/V21 telemetry cleanup did not hide the
critical renderer recovery chain.

### Live monster activity remains visible

A V9 Sector 1 LOAD restores state/topology/position/activation exactly and then
a committed MOVE services four active monsters in order.

Ordinary monster movement emits its probe and live COMMIT logs. Both subtype-4
members run their bounded three-goal continuations, and the turn closes with:

```text
[MONSTERACTIVESEQ] COMPLETE turn=1 reason=1 activeCount=4 delivered=4
sameMonsterTurn=yes ordered=yes publication=per-member multiAttack=deferred
```

The preceding no-immediate-attack probe also emits:

```text
[MONSTERTURN] COMPLETE reason=MOVE candidates=0 specialAIDeferred=2 ...
```

That line remaining visible is intentional: V21 suppresses this format only
when `specialAIDeferred == 0`.

### Memory stability

Before any lazy dialog-chain allocation, Entrance ALIVE is stable at:

```text
heap=93448 heap8=27524 largest8=18420
```

The first dialog allocates the bounded chain owner:

```text
[DIALOGCHAIN] OWNER bytes=1020 allocation=lazy-gameplay
```

After that allocation, repeated ALIVE samples throughout moves, pickups, doors,
dialog and renderer recovery remain stable at:

```text
heap=92412 heap8=26488 largest8=18420
```

There is no per-action heap drift in the tested path.

## Result

V21 is hardware-valid. The normal INFO firmware now keeps the hot movement,
touch-feedback and strictly idle monster-turn success chatter out of the serial
path, while real gameplay mutation, deferred/special AI state, collision,
dialogs, doors, live monster work and renderer recovery remain observable.

The hardware-tested code boundary is
`ac5e11127f294a5e2d7d1127febb21214be94458`. All commits after that boundary
for this milestone must be documentation-only before merge.
