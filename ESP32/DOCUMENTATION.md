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

Current active milestone:

- [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md) — audit all 265 real MAP_INTRO opcode IDs and execute exactly one real, reversible event-state opcode through a fail-closed native executor supporting only IDs `11/19/20`; **AWAITING REAL-CYD HARDWARE PASS**.

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
 -> ESP32-native gameplay + renderer
```

Do not promote desktop `Render_t`, `DoomCanvas_t`, pointer-heavy map structs or legacy resource ownership into permanent requirements.

## Current recovery point

Latest merged hardware baseline:

```text
PR   = #48
main = 0c8a52549ebb436139f7cd5c8b4ee63bdd175907
```

Active branch:

```text
agent/esp32-map1-native-opcode-exec1
```

Hardware-proven fingerprints entering this milestone:

```text
arenaFNV      = c3882516
decodedFNV    = a426dd18
mapStateFNV   = cd99b98e
lookupFNV     = 63430151
descriptorFNV = 27115328
linkageFNV    = 5727902c
scriptFNV     = f9e3d9df
filterFNV     = a5923b21
resumeFNV     = b98452da
```

Persistent native map/world/script heap remains:

```text
15252 B
```

The current candidate adds **0 persistent bytes**. It inventories the real opcode corpus, supports only `EV_CHANGESTATE`, `EV_NEXTSTATE`, `EV_PREVSTATE`, executes one actual supported BSP command if present, verifies the exact mutation in `EspMapScriptState`, restores `scriptFNV=f9e3d9df`, and PARKs before any world/render/entity effect.

## Milestone workflow

1. Branch from exact latest hardware-validated `main`.
2. Keep one bounded objective per branch.
3. Fail closed before unimplemented or unsafe ownership.
4. Validate normal optimized firmware on the real classic CYD.
5. Preserve exact RAM/fingerprint/hardware evidence.
6. Mark merge-ready only after implementation + hardware + docs agree.

See [`PORTING_STATUS.md`](PORTING_STATUS.md) and [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md).
