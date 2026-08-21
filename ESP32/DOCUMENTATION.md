# ESP32 documentation map

This file defines how the ESP32 CYD port documentation is organized and which file is authoritative for each kind of information.

## Source-of-truth hierarchy

### `README.md` — stable port guide

Use [`README.md`](README.md) for build/flash instructions, target hardware, stable architecture, SD/native-pack layout, touch model and high-level execution flow.

### `PORTING_STATUS.md` — authoritative current recovery point

Use [`PORTING_STATUS.md`](PORTING_STATUS.md) when resuming development. It owns the latest merged baseline, active branch, exact RAM/state boundary, current native contracts and next bounded milestone.

### Milestone archives — detailed evidence

Merged milestones:

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`START_GAME.md`](START_GAME.md) | fresh Start action + lifecycle cleanup | #36 | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) | first fitted `ST_INTRO` frame | #37 | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| [`INTRO_CLOCK.md`](INTRO_CLOCK.md) | bounded intro clock | #38 | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| [`INTRO_INPUT.md`](INTRO_INPUT.md) | intro input progression | #39 | `7ba68955a9b0979924c5e759736fb483589be744` |
| [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md) | intro teardown + RAM recovery | #41 | `897e982f4b37039d984b13265beaa68a83dce98b` |
| [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) | native BSP inventory + compact plan | #42 | `c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e` |
| [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md) | 14,095-byte immutable arena | #43 | `503fdd66fae625a45446fb4ea0853abc71d7dda3` |
| [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md) | allocation-free compact accessors | #44 | `ddcf19e6166f210a6f63fec1c608234ee3e253ea` |
| [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md) | 1,024-byte mutable tile state | #45 | `feec8a7fcb839dbd9f6de708f56f26b69a1e79e9` |
| [`MAP1_NATIVE_EVENTS.md`](MAP1_NATIVE_EVENTS.md) | allocation-free tile -> event lookup | #46 | `438cffabaaaaa3dc3b45486f56eacec1a047edcf` |
| [`MAP1_NATIVE_EVENT_DESCRIPTOR.md`](MAP1_NATIVE_EVENT_DESCRIPTOR.md) | event descriptor + exact bytecode linkage | #47 | `a3e629ba0be6b4dcc6329b17f18a0c3ca9828958` |
| [`MAP1_NATIVE_EVENT_FILTER.md`](MAP1_NATIVE_EVENT_FILTER.md) | 81-byte mutable script state + side-effect-free Game_runEvent filtering | #48 | `0c8a52549ebb436139f7cd5c8b4ee63bdd175907` |

Current merge-ready milestone:

- [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md) — exhaustive audit of all 265 real MAP_INTRO bytecodes plus first actual native Doom RPG opcode execution. Real-CYD corpus has 16 unique IDs (`2,7,8,9,10,11,13,15,16,18,19,24,26,27,40,41`), `opcodeAuditFNV=6f28df45`; state family has 41× `EV_CHANGESTATE`, 35× `EV_NEXTSTATE`, 0× `EV_PREVSTATE`; real command #50 (`EV_NEXTSTATE`) mutated target tile 226/event 16 from state 0→1, `execFNV=646b565c`, then rolled back to `scriptFNV=f9e3d9df`; zero heap/largest/frame/arena/map-state drift; **MERGE-READY**.

## Architecture rule

DoomRPG-RE is the executable specification/reference, **not the permanent engine architecture**.

Long-term direction:

```text
Doom RPG data / recovered behavior
 -> compact immutable native map
 -> allocation-free accessors
 -> small explicit mutable spatial state
 -> native event lookup + descriptor/linkage
 -> compact mutable script state
 -> side-effect-free event filtering
 -> fail-closed native opcode execution
 -> bounded native gameplay effects
 -> ESP32-native gameplay + renderer
```

Do not promote desktop `Render_t`, `DoomCanvas_t`, pointer-heavy map structs or legacy resource ownership into permanent requirements.

## Current recovery point

Latest merged hardware baseline:

```text
PR   = #48
main = 0c8a52549ebb436139f7cd5c8b4ee63bdd175907
```

Current merge-ready branch:

```text
agent/esp32-map1-native-opcode-exec1
hardware-tested firmware content = 3e07f1b0c4c6f609da8008f20dc667c7bbe58af6
status = REAL-CYD HARDWARE PASS; FIRST REAL NATIVE OPCODE EXECUTED; MERGE-READY
```

Hardware-proven fingerprints now include:

```text
arenaFNV       = c3882516
decodedFNV     = a426dd18
mapStateFNV    = cd99b98e
lookupFNV      = 63430151
descriptorFNV  = 27115328
linkageFNV     = 5727902c
scriptFNV      = f9e3d9df
filterFNV      = a5923b21
resumeFNV      = b98452da
opcodeAuditFNV = 6f28df45
firstExecFNV   = 646b565c
```

Persistent native map/world/script heap remains:

```text
15252 B
```

The opcode executor adds **0 persistent bytes**.

First true native script side effect:

```text
real command #50
EV_NEXTSTATE
arg1=00000702 arg2=00000100
 -> target tile 226 / event 16
 -> state 0 -> 1
 -> scriptFNV temporary 9b636dec
 -> rollback to f9e3d9df
```

Integrity remained exact:

```text
heap8       = 68828 -> 68828
largest8    = 36852 -> 36852
frameFNV    = 10f53ffb -> 10f53ffb
arenaFNV    = c3882516 -> c3882516
mapStateFNV = cd99b98e -> cd99b98e
scriptFNV   = f9e3d9df -> f9e3d9df
entities    = 0
monsters    = 0
ST_PLAYING  = no
```

## Milestone workflow

1. Branch from exact latest hardware-validated `main`.
2. Keep one bounded objective per branch.
3. Fail closed before unimplemented or unsafe ownership.
4. Validate normal optimized firmware on the real classic CYD.
5. Preserve exact RAM/fingerprint/hardware evidence.
6. Mark merge-ready only after implementation + hardware + docs agree.

After merge, the next bounded milestone should classify the remaining real opcode families and add only one coherent native effect family at a time. Prefer small state/intent ownership before entities or renderer mutation.

See [`PORTING_STATUS.md`](PORTING_STATUS.md) and [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md).
