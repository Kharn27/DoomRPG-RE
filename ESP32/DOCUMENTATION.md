# ESP32 documentation map

This file defines how the ESP32 CYD port documentation is organized and which file is authoritative for each kind of information.

## Source-of-truth hierarchy

### `README.md` — stable port guide

Use [`README.md`](README.md) for build/flash instructions, target hardware, stable architecture, SD/native-pack layout, touch model and high-level execution flow.

### `PORTING_STATUS.md` — authoritative current recovery point

Use [`PORTING_STATUS.md`](PORTING_STATUS.md) when resuming development. It owns the latest merged baseline, active branch, exact current RAM/state boundary, current native-map plan/runtime/access/state contracts and next bounded milestone.

The older full recovery catalog remains preserved unchanged at [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

### Milestone archives — detailed evidence

Merged milestones:

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`START_GAME.md`](START_GAME.md) | fresh Start action + lifecycle cleanup | #36 | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) | first fitted `ST_INTRO` frame | #37 | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| [`INTRO_CLOCK.md`](INTRO_CLOCK.md) | bounded 50 ms intro clock | #38 | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| [`INTRO_INPUT.md`](INTRO_INPUT.md) | bounded More/Continue progression | #39 | `7ba68955a9b0979924c5e759736fb483589be744` |
| [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md) | bounded intro teardown + RAM recovery | #41 | `897e982f4b37039d984b13265beaa68a83dce98b` |
| [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) | MAP_INTRO legacy refusal + native BSP reader/offset/resource/14,095 B plan | #42 | `c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e` |
| [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md) | persistent 14,095-byte native arena; 14,112 B actual heap cost, 17 B overhead, `arenaFNV=c3882516` | #43 | `503fdd66fae625a45446fb4ea0853abc71d7dda3` |
| [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md) | allocation-free native indexed access; full real-CYD semantic sweep, `decodedFNV=a426dd18`, 3–4 ms, zero drift | #44 | `ddcf19e6166f210a6f63fec1c608234ee3e253ea` |

The pre-final MAP_INTRO structural document remains preserved at [`archive/MAP1_STRUCTURAL_LOAD_PRE_HARDWARE_PASS.md`](archive/MAP1_STRUCTURAL_LOAD_PRE_HARDWARE_PASS.md).

Current merge-ready milestone:

- [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md) — first separate mutable native world state: hardware-proven 1,024-byte `tileFlags[1024]` payload, 1,040 B actual heap cost, `stateFNV=cd99b98e`, exact entrance/event topology, immutable `arenaFNV=c3882516`, and stable largest block; **MERGE-READY**.

After merge, add its PR/merge SHA to the merged-milestone table and treat the document as historical evidence except for archival metadata corrections.

## Architecture documentation rule

DoomRPG-RE is the executable specification/reference, **not the permanent engine architecture**.

Keep these concepts separate:

```text
recovered behavior/data contract
```

and:

```text
temporary desktop-derived compatibility/probe scaffolding
```

Do not promote `Render_t`, `DoomCanvas_t`, pointer-heavy `Node_t/Line_t/Sprite_t`, legacy map-wide resource ownership or linker wrappers into permanent requirements merely because they help migration.

Long-term direction:

```text
Doom RPG data / recovered behavior
        -> ESP32-native parsers
        -> compact immutable native map base
        -> allocation-free native accessors
        -> small explicit mutable native state/overlays
        -> ESP32-native renderer/game
```

The final ESP32 build may stop compiling the desktop-derived engine entirely once native components own the required contracts.

## Information retention rule

Before removing recovery information, ensure it is either duplicated semantically elsewhere, retained in another current document, or preserved in a historical archive/snapshot.

Do not delete unique hardware measurements merely to shorten documentation. Failed/safely refused feasibility experiments are valuable evidence when they explain architecture decisions.

## What belongs where

| Information | README | PORTING_STATUS | Milestone/archive |
| --- | :---: | :---: | :---: |
| Build/flash commands | yes | reference only | no |
| Stable architecture | yes | concise invariants | milestone detail |
| Latest merged/candidate SHA | link | **authoritative** | historical base/merge SHA |
| Current safe RAM/state boundary | summary | **authoritative** | historical boundary |
| Current native map/runtime/access/state contract | concise | **authoritative** | detailed proof |
| Full Serial evidence | no | selected values | yes |
| Rejected approaches | concise if recovery-relevant | concise | detailed |
| Next bounded milestone | link | **authoritative** | historical context only |

## Milestone workflow

1. Branch from the exact latest hardware-validated `main`.
2. Give the branch one coherent bounded objective.
3. Add/update a milestone document when the evidence has long-term value.
4. Build/flash/test normal firmware on the real classic CYD.
5. Fail closed before known unsafe working sets.
6. Use explicit states such as `AWAITING HARDWARE PASS`, `MEASUREMENT PASS`, `BRANCH CONTINUES`, or `MERGE-READY`.
7. Update `PORTING_STATUS.md` whenever the safe boundary changes.
8. Merge only when implementation + real hardware + documentation form a coherent boundary.
9. After merge, record the PR number and merge SHA in this archive index.

## Current recovery point

Latest merged hardware baseline:

```text
PR   = #44 — native compact MAP_INTRO access contract
main = ddcf19e6166f210a6f63fec1c608234ee3e253ea
```

Current merge-ready branch:

```text
branch = agent/esp32-map1-native-state
hardware-tested code = 9a17654b56a190932615bba4894e90debd0e3773
status = REAL-CYD HARDWARE PASS; MERGE-READY
```

The real CYD now proves three successive native layers:

```text
immutable arena
  payload            = 14095 B
  actual heap        = 14112 B
  arenaFNV           = c3882516

allocation-free access
  decodedFNV         = a426dd18
  full sweep         = 4 ms on current build
  heap drift         = 0 B

mutable tile state
  payload            = 1024 B
  actual heap        = 1040 B
  allocator overhead = 16 B
  stateFNV           = cd99b98e
  build/verify       = 9 ms
```

Combined actual native persistent map/world heap:

```text
15152 B
```

Compared with measured legacy structural allocation:

```text
55341 B -> 15152 B
saved = 40189 B
reduction ~= 72.6%
```

Spatial topology established on real hardware:

```text
block base       = 298 / 697 / 27 / 2
entrance refs    = 4
entrance cells   = 4
event refs       = 93
event cells      = 93
first entrance   = tile 68
first event      = tile 68
visited cells    = 0
largest8         = 36852 preserved
heap8 after state = 69016
legacy runtime   = absent
entities/monsters = 0
ST_PLAYING       = no
```

Tile 68 carrying both entrance and event semantics confirms the composable bit-field model for `EspMapState`.

The preferred next bounded milestone after merge is a compact native **tile -> event lookup/index contract**, derived from actual event-query behavior rather than desktop `tileEvents` ownership.

See [`PORTING_STATUS.md`](PORTING_STATUS.md), [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md), and merged [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md).
