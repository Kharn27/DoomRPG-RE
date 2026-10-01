# Milestone ESP32 consolidation — interaction corpus V19

Date: 2026-10-01  
Branch: `agent/esp32-consolidation-interaction-diagnostics-v17`  
Base for V19: V18 docs head `eece43d5762c0ed9304f30a60cc000128fb1c08e`  
Hardware-tested code boundary: `a2dffc4243701a5f78fa02abf39a81da68b5c128`  
Status: **REAL-CYD PASS**

## Goal

Remove the one-shot interaction opcode corpus wrapper from normal INFO
production without touching the real resident gameplay or event-chain
semantics.

## Audit result

The production wrapper did exactly:

```text
EspNativeGameplayEventChain_logCorpus()
__real_EspNativeResidentGameplay_service(doomRpg)
```

The corpus scan only traversed resident events/bytecodes, counted opcode
families, printed `[INTERACTCORPUS]` lines and set a private completion byte.
No result was passed to gameplay and no gameplay/map owner was mutated.

The event-chain production APIs remain independent and unchanged:

```text
preflight
preflightRange
maskForDialogBegin / restoreDialogMask
beginDialogCommand
execute / executeRange
dialog resume / rollback
```

## Implementation

Normal `esp32-cyd` no longer links
`--wrap=EspNativeResidentGameplay_service`.

`esp32-cyd-bringup` retains that wrapper at TRACE level. The corpus function,
its wrapper and `corpusLogged` are compile-time gated at DEBUG/TRACE.

## CI / ELF

CI #1218: **SUCCESS**

```text
static RAM       45064 B
linked Flash    762021 B
firmware.bin    762384 B
artifact id     11155223388
artifact digest sha256:647af5bbbc5bda0f091b4a5de83ba05d961cd0774f5ed50a62cfda17d2df6e55
firmware sha256 b394438989fcfcfa4c1d63e5142fc7e56c8c017aa7134af65344d7faf336fffb
ELF sha256      540660ede5a2d4e5621e60c2d03c473f7c660ac09041e919e5c548187e7a96ba
active wraps    49
```

Versus V18:

```text
static RAM       45072 -> 45064 B     -8 B
linked Flash    762617 -> 762021 B   -596 B
firmware.bin    762976 -> 762384 B   -592 B
active wraps        50 -> 49           -1
```

Final normal ELF:

```text
EspNativeGameplayEventChain_logCorpus        ABSENT
__wrap_EspNativeResidentGameplay_service     ABSENT
corpusLogged                                 ABSENT
[INTERACTCORPUS] strings                     ABSENT
EspNativeResidentGameplay_service            PRESENT
EspNativeGameplayTransition_trySelect        PRESENT
EspNativeGameplayTransitionHandoff_service   PRESENT
```

## Real-CYD validation

Validated path:

```text
cold boot
 -> native MENU_MAIN
 -> START
 -> full intro
 -> bounded disposal
 -> Entrance bootstrap
 -> FIRST_FRAME 71ca7465
 -> ENGINESESSION READY shapeData=0x0 mediaTexels=0x0
 -> FORWARD committed
 -> TURN committed
 -> crate subtype-2 transform
 -> STRAFE committed
 -> scientist DIALOG event 88
 -> full dialog pagination
 -> DIALOGCHAIN RESUME opcode 19 mutation
 -> stable ALIVE
```

Before lazy dialog-chain allocation:

```text
heap=93448 heap8=27524 largest8=18420
```

After its documented 1020-byte owner allocation and related bounded state:

```text
heap=92412 heap8=26488 largest8=18420
```

The +8 B free-heap shift is visible throughout pre-dialog runtime relative to
V18 and matches the CI static-RAM reduction.

No `[INTERACTCORPUS]` line appears.

## Result

V19 is hardware-valid. Production resident gameplay is direct again; the
historical interaction corpus remains available only in explicit bringup.

The next consolidation audit should target hot success telemetry still emitted
per world redraw rather than another one-shot corpus: native-frame summaries,
weapon draw summaries, facing-label paint summaries and resident-frame timing
are visible candidates. Functional failures/recovery and ownership must remain
INFO/ERROR.
