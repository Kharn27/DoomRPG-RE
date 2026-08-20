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
- current safe execution boundary
- exact RAM / framebuffer / state invariants
- important regression FNVs
- current presentation timings
- milestone/PR references
- deferred work and the next bounded milestone

If a value in a historical milestone archive differs from this file because a
later build changed the baseline, `PORTING_STATUS.md` wins for the **current**
state. The historical value remains valid for the old milestone/build.

### Milestone archives — immutable evidence

The following files preserve the detailed evidence that established the recent
fresh-Start and intro boundaries:

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`START_GAME.md`](START_GAME.md) | real fresh Start action, OOM diagnosis and 55,416 B lifecycle cleanup | #36 | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) | first real fitted `ST_INTRO` frame | #37 | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| [`INTRO_CLOCK.md`](INTRO_CLOCK.md) | bounded 50 ms multi-frame intro clock | #38 | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| [`INTRO_INPUT.md`](INTRO_INPUT.md) | full bounded `More` / `Continue` touch progression | #39 | `7ba68955a9b0979924c5e759736fb483589be744` |

These files intentionally keep detailed Serial excerpts, build-specific heap
baselines, FNVs, rejected approaches and exact hardware observations.

Once a milestone is merged, its archive should not be rewritten to describe the
newest architecture. Only small archival metadata fixes are appropriate. The
current architecture belongs in `README.md`; the current recovery state belongs
in `PORTING_STATUS.md`.

## Information retention rule

Before removing text from `README.md` or `PORTING_STATUS.md`, verify one of these
is true:

1. the information is duplicated verbatim or semantically in a milestone archive;
2. the information is retained in the other current document;
3. the information is obsolete and the replacement/current value is explicitly
   documented while the old value remains in a historical archive.

Do **not** delete unique hardware measurements merely to make a document shorter.
Move or index them instead.

## What belongs where

| Information | README | PORTING_STATUS | Milestone archive |
| --- | :---: | :---: | :---: |
| Build / flash commands | yes | reference only | no |
| Target hardware | yes | concise | inherited snapshot |
| Current engine architecture | yes | concise invariants | historical only |
| Latest `main` SHA | link | **authoritative** | historical base/merge SHA |
| Current safe boundary | summary | **authoritative** | historical boundary |
| FNV regression catalog | selected | **authoritative** | detailed milestone subset |
| Heap/largest-block current values | summary | **authoritative** | exact old build values |
| Full Serial logs | no | selected excerpts only | yes |
| Why a design decision was made | concise | recovery-relevant | detailed when milestone-specific |
| Old failed/temporary result | generally no | only if useful as reference | yes |
| Next development boundary | link | **authoritative** | historical next boundary only |

## Documentation workflow for each new milestone

1. Branch from the exact latest hardware-validated `main`.
2. Add a dedicated milestone document only when the evidence is substantial or
   has long-term diagnostic value.
3. During implementation, the milestone document may carry `IMPLEMENTED`,
   `AWAITING HARDWARE PASS`, or `MERGE-READY` status.
4. After real-CYD validation, update `PORTING_STATUS.md` with the new current
   recovery contract and only the representative evidence needed to resume work.
5. Update `README.md` only if the stable operating architecture or user/developer
   workflow changed.
6. After the PR is merged, mark the milestone document as a **MERGED ARCHIVE**
   with its PR number and merge SHA.
7. Its old `Next milestone` section becomes historical context; the live roadmap
   is only in `PORTING_STATUS.md`.

## Current documentation recovery point

Documentation architecture branch base:

```text
main = 7ba68955a9b0979924c5e759736fb483589be744
PR   = #39 (bounded intro input merged)
```

At that point the port is safely parked after the final intro `Continue` with:

```text
state                   = ST_INTRO (9)
storyPage               = 2
storyTextPage           = 0
intro clock             = inactive
intro input             = inactive
intro images/texts      = resident
heap8                   = 50656
largest8                = 13300
DoomCanvas_run          = NOT called
DoomCanvas_disposeIntro = NOT called
DoomCanvas_loadMap      = NOT called
shapeData               = NULL
mediaTexels             = NULL
```

The live next milestone and any later values belong in
[`PORTING_STATUS.md`](PORTING_STATUS.md).
