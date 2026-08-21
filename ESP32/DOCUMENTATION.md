# ESP32 documentation map

This file defines how the ESP32 CYD port documentation is organized and which file is authoritative for each kind of information.

## Source-of-truth hierarchy

### `README.md` — stable port guide

Use [`README.md`](README.md) for build/flash instructions, target hardware, stable architecture, SD/native-pack layout, touch model and high-level execution flow.

### `PORTING_STATUS.md` — authoritative current recovery point

Use [`PORTING_STATUS.md`](PORTING_STATUS.md) when resuming development. It owns the latest merged baseline, active branch, exact current RAM/state boundary, current native runtime/access/state/event/descriptor/script/filter contracts and next bounded milestone.

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
| [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md) | persistent 14,095-byte native arena; 14,112 B actual heap cost, `arenaFNV=c3882516` | #43 | `503fdd66fae625a45446fb4ea0853abc71d7dda3` |
| [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md) | allocation-free native indexed access; full real-CYD semantic sweep, `decodedFNV=a426dd18` | #44 | `ddcf19e6166f210a6f63fec1c608234ee3e253ea` |
| [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md) | first mutable native world state; 1,024-byte tile payload, 1,040 B actual heap, `stateFNV=cd99b98e` | #45 | `feec8a7fcb839dbd9f6de708f56f26b69a1e79e9` |
| [`MAP1_NATIVE_EVENTS.md`](MAP1_NATIVE_EVENTS.md) | allocation-free tile -> event lookup; 93/931 hit/miss proof, `lookupFNV=63430151` | #46 | `438cffabaaaaa3dc3b45486f56eacec1a047edcf` |
| [`MAP1_NATIVE_EVENT_DESCRIPTOR.md`](MAP1_NATIVE_EVENT_DESCRIPTOR.md) | read-only event descriptor + exact bytecode linkage; `descriptorFNV=27115328`, `linkageFNV=5727902c`, all 265 commands partitioned once | #47 | `a3e629ba0be6b4dcc6329b17f18a0c3ca9828958` |

The pre-final MAP_INTRO structural document remains preserved at [`archive/MAP1_STRUCTURAL_LOAD_PRE_HARDWARE_PASS.md`](archive/MAP1_STRUCTURAL_LOAD_PRE_HARDWARE_PASS.md).

Current merge-ready milestone:

- [`MAP1_NATIVE_EVENT_FILTER.md`](MAP1_NATIVE_EVENT_FILTER.md) — compact 81-byte mutable script-state overlay plus exact side-effect-free `Game_runEvent()` eligibility filtering; real-CYD 142,848 contexts / 407,040 command evaluations, `filterFNV=a5923b21`, `resumeFNV=b98452da`, reversible `scriptFNV=f9e3d9df -> 99003167 -> f9e3d9df`; actual script heap 100 B; filter persistent cost 0 B; zero largest/frame/arena/map-state drift; **MERGE-READY**.

After merge, add its PR/merge SHA to the merged-milestone table and treat the document as historical evidence except for archival metadata corrections.

## Architecture documentation rule

DoomRPG-RE is the executable specification/reference, **not the permanent engine architecture**.

Keep recovered behavior/data contracts separate from temporary desktop-derived compatibility/probe scaffolding.

Do not promote `Render_t`, `DoomCanvas_t`, pointer-heavy `Node_t/Line_t/Sprite_t`, legacy map-wide resource ownership or linker wrappers into permanent requirements merely because they help migration.

Long-term direction:

```text
Doom RPG data / recovered behavior
        -> ESP32-native parsers
        -> compact immutable native map base
        -> allocation-free native accessors
        -> small explicit mutable native spatial state
        -> allocation-free native event lookup
        -> read-only event descriptor / bytecode linkage
        -> compact mutable native script state
        -> side-effect-free event eligibility filter
        -> bounded fail-closed opcode execution
        -> ESP32-native gameplay + renderer
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
| Current native runtime/access/state/event/script/filter contracts | concise | **authoritative** | detailed proof |
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
PR   = #47 — native MAP_INTRO event descriptor / bytecode linkage
main = a3e629ba0be6b4dcc6329b17f18a0c3ca9828958
```

Current merge-ready branch:

```text
branch = agent/esp32-map1-native-event-filter
status = REAL-CYD HARDWARE PASS; MERGE-READY
```

The real CYD now proves seven successive native layers:

```text
immutable arena
  payload            = 14095 B
  actual heap        = 14112 B
  arenaFNV           = c3882516

allocation-free decode
  decodedFNV         = a426dd18

mutable tile state
  payload            = 1024 B
  actual heap        = 1040 B
  stateFNV           = cd99b98e

tile -> event lookup
  persistent bytes   = 0 B
  lookupFNV          = 63430151

event descriptor / bytecode linkage
  persistent bytes   = 0 B
  descriptorFNV      = 27115328
  linkageFNV         = 5727902c
  265 command refs   = 265 unique / 0 overlaps / 0 gaps

mutable script state
  payload            = 81 B
  actual heap        = 100 B
  allocator overhead = 19 B
  scriptFNV          = f9e3d9df
  reversible test    = f9e3d9df -> 99003167 -> f9e3d9df

side-effect-free event filter
  persistent bytes   = 0 B
  contexts           = 142848
  evaluations        = 407040
  eligible           = 4784
  blocked            = 2048
  state mismatch     = 379392
  key mismatch       = 0
  flags mismatch     = 20816
  block-input events = 8
  filterFNV          = a5923b21
  resumeFNV          = b98452da
  exhaustive sweep   = 1411 ms
```

Combined actual native persistent map/world/script heap:

```text
14112 + 1040 + 100 = 15252 B
```

Compared with measured legacy structural allocation:

```text
55341 B -> 15252 B
saved = 40089 B
reduction ~= 72.4%
```

Current integrity boundary:

```text
heap8 after script state = 68844 on current build
largest8                 = 36852
framebuffer drift        = none
arenaFNV                 = c3882516
map stateFNV             = cd99b98e
scriptFNV                = f9e3d9df
filter persistent heap   = 0 B
legacy runtime           = absent
script execution         = no
entities/monsters        = 0 / 0
ST_PLAYING               = no
```

The next bounded milestone after merge is a **native opcode inventory + fail-closed executor-dispatch audit**, followed by one deliberately harmless real execution proof. Do not enable all opcode side effects or enter gameplay in one jump.

See [`PORTING_STATUS.md`](PORTING_STATUS.md), [`MAP1_NATIVE_EVENT_FILTER.md`](MAP1_NATIVE_EVENT_FILTER.md), and merged [`MAP1_NATIVE_EVENT_DESCRIPTOR.md`](MAP1_NATIVE_EVENT_DESCRIPTOR.md).
