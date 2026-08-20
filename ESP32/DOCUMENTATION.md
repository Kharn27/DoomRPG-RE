# ESP32 documentation map

This file defines how the ESP32 CYD port documentation is organized and which file is authoritative for each kind of information.

The goal is to make the project easy to recover after a break without copying the same hardware logs and measurements into several places.

## Source-of-truth hierarchy

### `README.md` — stable port guide

Use [`README.md`](README.md) to understand and operate the port as it exists now:

- target hardware
- build/flash commands
- PlatformIO environments
- stable rendering/resource architecture
- SD/native-pack layout
- touch model
- high-level execution path
- stable developer notes

It should remain readable and should not become a chronological hardware log.

### `PORTING_STATUS.md` — authoritative current recovery point

Use [`PORTING_STATUS.md`](PORTING_STATUS.md) when resuming development.

It owns:

- latest merged hardware baseline and active candidate
- current hardware-safe state/RAM boundary
- current engine invariants
- current native-map format/runtime plan
- selected regression hashes/timings
- next bounded milestone

If a historical archive differs because a later build moved a heap/FNV baseline, `PORTING_STATUS.md` wins for the **current** state. Historical values remain valid for their original build.

The older long-form recovery catalog that existed before the native MAP_INTRO pass is preserved unchanged at:

- [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md)

This snapshot preserves menu/touch/LRU/FNV/build-specific measurements that no longer need to dominate the live recovery file.

### Milestone archives — detailed evidence

Merged milestone archives:

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`START_GAME.md`](START_GAME.md) | real fresh Start action, OOM diagnosis and 55,416 B lifecycle cleanup | #36 | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) | first real fitted `ST_INTRO` frame | #37 | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| [`INTRO_CLOCK.md`](INTRO_CLOCK.md) | bounded 50 ms multi-frame intro clock | #38 | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| [`INTRO_INPUT.md`](INTRO_INPUT.md) | full bounded `More` / `Continue` touch progression | #39 | `7ba68955a9b0979924c5e759736fb483589be744` |
| [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md) | bounded intro resource teardown and measured RAM recovery | #41 | `897e982f4b37039d984b13265beaa68a83dce98b` |

Current merge-ready milestone document:

- [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) — legacy MAP_INTRO feasibility refusal, ESP32-native `.pak` BSP reader, exact section offsets/resource sets and the hardware-validated 14,095 B compact structural plan.

The pre-final version of that document is preserved for information-retention safety at:

- [`archive/MAP1_STRUCTURAL_LOAD_PRE_HARDWARE_PASS.md`](archive/MAP1_STRUCTURAL_LOAD_PRE_HARDWARE_PASS.md)

After the current branch is merged, `MAP1_STRUCTURAL_LOAD.md` becomes an immutable merged milestone archive; only archival metadata such as PR number/merge SHA should then change.

## Architecture documentation rule

DoomRPG-RE is the executable specification/reference, **not the permanent engine architecture**.

Documentation must clearly distinguish:

```text
behavior/data contract recovered from DoomRPG-RE
```

from:

```text
temporary compatibility/probe scaffolding used to measure that contract
```

Do not promote `Render_t`, `DoomCanvas_t`, legacy map-wide ownership, pointer-heavy `Node_t/Line_t/Sprite_t`, or linker wrappers into permanent requirements merely because they are currently useful during migration.

The final ESP32 build may completely stop compiling the desktop-derived engine once native replacements own the required behavior and formats.

Current long-term direction:

```text
Doom RPG data / recovered behavior
              -> ESP32-native parsers
              -> compact immutable native map base
              -> small mutable index-based overlays
              -> ESP32-native renderer/game
```

## Information retention rule

Before removing text from `README.md` or `PORTING_STATUS.md`, verify one of these is true:

1. the information is duplicated semantically in a milestone archive;
2. the information is retained in another current document;
3. the information is obsolete and its replacement is explicit while the old value remains in a historical archive/snapshot.

Do **not** delete unique hardware measurements merely to shorten documentation. Move/archive/index them instead.

A safe refusal or failed architectural feasibility measurement is valuable evidence when it explains why a native replacement is required.

The two `archive/*PRE_*` snapshots created during the MAP_INTRO milestone exist specifically to satisfy this rule while allowing the current recovery documents to stay focused.

## What belongs where

| Information | README | PORTING_STATUS | Milestone/archive |
| --- | :---: | :---: | :---: |
| Build / flash commands | yes | reference only | no |
| Target hardware | yes | concise | inherited snapshot |
| Stable engine architecture | yes | concise invariants | historical/detail when milestone-specific |
| Latest merged/candidate SHA | link | **authoritative** | historical base/merge SHA |
| Current safe boundary | summary | **authoritative** | historical boundary |
| Current native map plan | concise | **authoritative** | detailed proof |
| FNV/CRC regression values | selected | **authoritative current set** | detailed milestone subset |
| Heap/largest-block current values | summary | **authoritative** | exact historical build values |
| Full Serial logs | no | selected excerpts only | yes |
| Why a design decision was made | concise | recovery-relevant | detailed when milestone-specific |
| Old failed/temporary result | generally no | only if recovery-relevant | yes |
| Next development boundary | link | **authoritative** | historical/active design context |

## Documentation workflow for each milestone

1. Branch from the exact latest hardware-validated `main`.
2. Give the branch one coherent bounded objective; a measurement may legitimately reshape the implementation while staying on that branch.
3. Add a dedicated milestone document when evidence has long-term diagnostic/architectural value.
4. Build/flash/test normal firmware on the real classic CYD.
5. Fail closed before known unsafe working sets.
6. During development, use explicit statuses such as `AWAITING HARDWARE PASS`, `MEASUREMENT PASS`, `BRANCH CONTINUES`, or `MERGE-READY`.
7. A successful refusal can be a measurement PASS without being merge-ready if the replacement still belongs to the same objective.
8. Update `PORTING_STATUS.md` when the safe boundary or next implementation boundary changes.
9. Update `README.md` only when stable operating architecture/workflow changes.
10. Merge only when implementation + real hardware + documentation form a coherent boundary.
11. After merge, mark the milestone document as a merged archive with PR number and merge SHA.
12. The live roadmap remains in `PORTING_STATUS.md`, not in old archives.

## Current documentation recovery point

Latest merged baseline before the current candidate:

```text
PR   = #41 — bounded intro disposal
main = 897e982f4b37039d984b13265beaa68a83dce98b
```

Current candidate:

```text
branch = agent/esp32-map1-structural-load
hardware-tested code = 45833b68b0e185630b1e5a769e54a051196c70e8
status = MERGE-READY
```

Hardware now proves that the native reader can consume all `21823 B` of `/intro.bsp` from `DoomRPG-ESP32.pak` through a `256 B` window, reproduce the exact structure/offsets/resource sets, verify `CRC32=623f34e4` and `FNV-1a=d5cc751f`, calculate a `14095 B` compact persistent structural plan and return with zero heap/largest-block/framebuffer drift.

The live next step after merge is **native structural arena/pool allocation + direct section population**, still parked before entity spawn or `ST_PLAYING`.

See [`PORTING_STATUS.md`](PORTING_STATUS.md) for the exact current boundary and [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) for the complete hardware/architecture evidence.
