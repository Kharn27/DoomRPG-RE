# ESP32 documentation map

This file defines how the ESP32 CYD port documentation is organized and which
file is authoritative for each kind of information.

The goal is to keep the project easy to recover after a break without copying the
same hardware logs and measurements into several files.

## Source-of-truth hierarchy

### `README.md` — stable port guide

Use [`README.md`](README.md) to understand and operate the port **as it exists
now**:

- target hardware
- build and flash commands
- PlatformIO environments
- rendering/resource architecture
- SD layout
- touch model
- current high-level execution path
- stable developer notes

It should remain readable and should not become a chronological hardware log.

### `PORTING_STATUS.md` — current recovery point

Use [`PORTING_STATUS.md`](PORTING_STATUS.md) when resuming development.
It is the authoritative current snapshot for:

- latest hardware-validated `main` SHA
- current active branch/recovery boundary
- exact RAM / framebuffer / state invariants
- important regression FNVs
- current presentation timings
- milestone/PR references
- deferred work and the next bounded milestone

If a value in a historical milestone archive differs from this file because a
later build changed the baseline, `PORTING_STATUS.md` wins for the **current**
state. The historical value remains valid for the old milestone/build.

### Milestone archives — immutable evidence after merge

Recent merged archives:

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`START_GAME.md`](START_GAME.md) | real fresh Start action, OOM diagnosis and 55,416 B lifecycle cleanup | #36 | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) | first real fitted `ST_INTRO` frame | #37 | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| [`INTRO_CLOCK.md`](INTRO_CLOCK.md) | bounded 50 ms multi-frame intro clock | #38 | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| [`INTRO_INPUT.md`](INTRO_INPUT.md) | full bounded `More` / `Continue` touch progression | #39 | `7ba68955a9b0979924c5e759736fb483589be744` |
| [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md) | bounded intro resource teardown and measured RAM recovery | #41 | `897e982f4b37039d984b13265beaa68a83dce98b` |

These files intentionally keep detailed Serial excerpts, build-specific heap
baselines, FNVs, rejected approaches and exact hardware observations.

Once a milestone is merged, its archive should not be rewritten to describe the
newest architecture. Only small archival metadata fixes are appropriate. The
current architecture belongs in `README.md`; the current recovery state belongs
in `PORTING_STATUS.md`.

### Active branch evidence

The current unmerged measurement/design document is:

- [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) — first post-prologue
  `/intro.bsp` structural feasibility probe and native streaming-loader plan.

Unlike a merged archive, an active branch document is expected to evolve while
hardware measurements drive implementation on that same branch.

## Architecture documentation rule

DoomRPG-RE is the executable specification/reference, not the permanent engine
architecture.

Documentation must clearly distinguish:

```text
behavior/data contract recovered from DoomRPG-RE
```

from:

```text
temporary compatibility scaffolding used to measure that contract
```

Do not accidentally promote `Render_t`, `DoomCanvas_t`, legacy map-wide resource
ownership or linker wrappers into permanent requirements merely because they are
currently useful during migration. The final ESP32 build may completely stop
compiling the desktop-derived engine once native replacements own the required
behavior and data formats.

## Information retention rule

Before removing text from `README.md` or `PORTING_STATUS.md`, verify one of these
is true:

1. the information is duplicated verbatim or semantically in a milestone archive;
2. the information is retained in the other current document;
3. the information is obsolete and the replacement/current value is explicitly
   documented while the old value remains in a historical archive.

Do **not** delete unique hardware measurements merely to make a document shorter.
Move or index them instead.

A safe refusal or failed architectural feasibility measurement is also valuable
hardware evidence when it explains why a native replacement is required.

## What belongs where

| Information | README | PORTING_STATUS | Milestone/archive |
| --- | :---: | :---: | :---: |
| Build / flash commands | yes | reference only | no |
| Target hardware | yes | concise | inherited snapshot |
| Current engine architecture | yes | concise invariants | historical/detail when milestone-specific |
| Latest `main` SHA | link | **authoritative** | historical base/merge SHA |
| Current safe boundary | summary | **authoritative** | historical boundary |
| FNV regression catalog | selected | **authoritative** | detailed milestone subset |
| Heap/largest-block current values | summary | **authoritative** | exact old build values |
| Full Serial logs | no | selected excerpts only | yes |
| Why a design decision was made | concise | recovery-relevant | detailed when milestone-specific |
| Old failed/temporary result | generally no | only if recovery-relevant | yes |
| Next development boundary | link | **authoritative** | historical/active design context |

## Documentation workflow for each new milestone

1. Branch from the exact latest hardware-validated `main`.
2. Add a dedicated milestone document when the evidence is substantial or has
   long-term diagnostic/architectural value.
3. During implementation, the milestone document may carry `IMPLEMENTED`,
   `AWAITING HARDWARE PASS`, `MEASUREMENT PASS`, `BRANCH CONTINUES`, or
   `MERGE-READY` status as appropriate.
4. A fail-closed hardware refusal can be a successful measurement without making
   the branch merge-ready; continue on the same branch when the measured result
   directly defines the replacement implementation.
5. Update `PORTING_STATUS.md` with the live recovery contract and only the
   representative evidence needed to resume work.
6. Update `README.md` only if stable operating architecture or developer workflow
   changed.
7. After a PR is merged, mark the milestone document as a **MERGED ARCHIVE** with
   its PR number and merge SHA.
8. Its old `Next milestone` section becomes historical context; the live roadmap
   is only in `PORTING_STATUS.md`.

## Current documentation recovery point

Latest merged hardware baseline:

```text
PR   = #41 — bounded intro disposal
main = 897e982f4b37039d984b13265beaa68a83dce98b
```

That real-CYD boundary is:

```text
state                   = ST_INTRO (9)
storyPage               = 3
storyTextPage           = 0
intro clock/input       = inactive
intro images/texts      = NULL
render clip             = off
startupMap              = 1
heap8                   = 84408 on PR #41 validation build
largest8                = 36852
shapeData               = NULL
mediaTexels             = NULL
DoomCanvas_loadMap      = NOT called
```

Current development branch:

```text
agent/esp32-map1-structural-load
```

Its normal-firmware probe measured `/intro.bsp` completely, proved that the
legacy simultaneous raw-BSP + runtime allocation does not fit even with zero
safety margin, refused safely, cleaned temporary data and returned to the same
logical boundary at `heap8=84384`, `largest8=36852` on that build.

The branch continues with an ESP32-native streaming BSP reader rather than trying
to force the old `Render_beginLoadMapData()` lifecycle to fit.

The live next implementation and any newer values belong in
[`PORTING_STATUS.md`](PORTING_STATUS.md).
