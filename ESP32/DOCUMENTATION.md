# ESP32 documentation map

This file defines how the ESP32 CYD port documentation is organized and which file is authoritative for each kind of information.

## Source-of-truth hierarchy

- [`README.md`](README.md): stable build/flash and platform guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: detailed implementation and real-CYD evidence.

## Merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`START_GAME.md`](START_GAME.md) | fresh Start action + lifecycle cleanup | #36 | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) | first fitted `ST_INTRO` frame | #37 | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| [`INTRO_CLOCK.md`](INTRO_CLOCK.md) | bounded intro clock | #38 | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| [`INTRO_INPUT.md`](INTRO_INPUT.md) | intro input progression | #39 | `7ba68955a9b0979924c5e759736fb483589be744` |
| [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md) | intro teardown + RAM recovery | #41 | `897e982f4b37039d984b13265beaa68a83dce98b` |
| [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) | native BSP inventory + compact plan | #42 | `c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e` |
| [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md) | compact immutable map arena | #43 | `503fdd66fae625a45446fb4ea0853abc71d7dda3` |
| [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md) | allocation-free compact accessors | #44 | `ddcf19e6166f210a6f63fec1c608234ee3e253ea` |
| [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md) | compact mutable tile state | #45 | `feec8a7fcb839dbd9f6de708f56f26b69a1e79e9` |
| [`MAP1_NATIVE_EVENTS.md`](MAP1_NATIVE_EVENTS.md) | allocation-free tile -> event lookup | #46 | `438cffabaaaaa3dc3b45486f56eacec1a047edcf` |
| [`MAP1_NATIVE_EVENT_DESCRIPTOR.md`](MAP1_NATIVE_EVENT_DESCRIPTOR.md) | event descriptor + bytecode linkage | #47 | `a3e629ba0be6b4dcc6329b17f18a0c3ca9828958` |
| [`MAP1_NATIVE_EVENT_FILTER.md`](MAP1_NATIVE_EVENT_FILTER.md) | compact script state + event filtering | #48 | `0c8a52549ebb436139f7cd5c8b4ee63bdd175907` |
| [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md) | opcode audit + first native state mutation | #49 | `6e43ef059db52783b7264e84579216cb2572a1e2` |
| [`MAP1_NATIVE_UI_INTENT.md`](MAP1_NATIVE_UI_INTENT.md) | string spans + UI intents | #50 | `9a5e8ade361180d220f2b3614a443e5efb0d27bd` |
| [`MAP1_NATIVE_STRING_READER.md`](MAP1_NATIVE_STRING_READER.md) | bounded pack-backed strings | #51 | `526640b12d978fdbe9c8a9239c37db2fca95cddd` |
| [`MAP1_NATIVE_STATUS_MESSAGE.md`](MAP1_NATIVE_STATUS_MESSAGE.md) | FORCE_MESSAGE owner | #52 | `40b61af5e2115266d4d03dddcc3175850538b0f5` |
| [`MAP1_NATIVE_DIALOG_OWNER.md`](MAP1_NATIVE_DIALOG_OWNER.md) | DIALOG/NOBACK pause owner | #53 | `395418510207bf24ac45ddbb4c4c15db3ddc8998` |
| [`MAP1_NATIVE_NOTEBOOK.md`](MAP1_NATIVE_NOTEBOOK.md) | NOTE notebook owner | #54 | `03002f79eb03bdcb4c9e430c43e4693dab47e44b` |
| [`MAP1_NATIVE_KEY_GATE.md`](MAP1_NATIVE_KEY_GATE.md) | CHECK_KEY dynamic gate | #55 | `03c4275f2abfd6671c8bf499c075435d7b61ab97` |
| [`MAP1_NATIVE_PASSWORD_OWNER.md`](MAP1_NATIVE_PASSWORD_OWNER.md) | PASSWORD pause/submission owner | #56 | `3c113cc047aeb613f2ba4ab7905e92487c796f80` |
| [`MAP1_NATIVE_LINE_DOOR_STATE.md`](MAP1_NATIVE_LINE_DOOR_STATE.md) | first mutable world owner + OPEN/CLOSE | #57 | `e4fb32f41b7074bbb433e64f4c824edb2167cf50` |
| [`MAP1_NATIVE_UNLOCK_STATE.md`](MAP1_NATIVE_UNLOCK_STATE.md) | second line-world owner + EV_UNLOCK | #58 | `7503b379185db3f05713eb34f1762173edb977d0` |

## Current merge-ready milestone

[`MAP1_NATIVE_GIVEMAP_STATE.md`](MAP1_NATIVE_GIVEMAP_STATE.md) owns `9 / EV_GIVEMAP` as a compact native automap mutation family.

```text
branch = agent/esp32-map1-native-givemap-state
base   = 7503b379185db3f05713eb34f1762173edb977d0
hardware-tested firmware = 2e0f8f5de93f806380ee254a8dab59a817c73f5d
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Hardware-proven automap owner:

```text
480 line reveal bits   = 60 B
344 sprite reveal bits = 43 B
payload                = 103 B
actual heap            = 120 B
initial line revealed  = 0
initial sprite revealed= 0
automapStateFNV        = 669b1aa7
```

Real GIVEMAP corpus:

```text
refs=1 mutated=1 noMutation=0 removable=0 stateExecRefused=1
resultBytes=20
lineTargets=430 spriteTargets=344 entranceTargets=4
lineMutTotal=430 spriteMutTotal=344 tileMutTotal=4
giveMapFNV=98c7ac59
rollback=1/1 idempotentHandled=1
```

Canonical real mutation:

```text
cmd43 event14 off1
430 line reveals + 344 sprite reveals + 4 entrance VISITED bits
handled=1 removeIfHandled=0
automapStateFNV 669b1aa7 -> 9d03ca2d -> rollback 669b1aa7
mapStateFNV     cd99b98e -> e21edbce -> rollback cd99b98e
```

## Current hardware-proven boundary

Persistent native heap:

```text
immutable arena         14112 B
mutable tile state       1040 B
mutable script state      100 B
mutable line state        136 B
mutable texture state      76 B
mutable automap state     120 B
------------------------------
total                   15584 B
```

Latest same-build GIVEMAP allocation witness:

```text
heap8 68384 -> 68264
payload=103 B actual heap=120 B overhead=17 B
largest8=34804 stable
frameFNV=453f0d5c stable
PARK heap=134028 heap8=68264 largest8=34804
```

Key fingerprints:

```text
arenaFNV              = c3882516
mapStateFNV           = cd99b98e
scriptFNV             = f9e3d9df
keyGateFNV            = 9ace79cd
passwordOwnerFNV      = 48f01689
passwordSubmitFNV     = 90e8c574
lineStateFNV          = e5e74861
lineDoorFNV           = b1c9d297
lineMutatedFNV        = 8f57d779
lineTextureStateFNV   = f1fc1875
unlockFNV             = 261d756a
unlockMutatedLineFNV  = 8d5f89d8
unlockMutatedTexFNV   = 997459ec
automapStateFNV       = 669b1aa7
giveMapFNV            = 98c7ac59
giveMapMutatedAutoFNV = 9d03ca2d
giveMapMutatedMapFNV  = e21edbce
```

Legacy witnesses and framebuffer remain unchanged across GIVEMAP; `packIO=no`, legacy runtime remains clear, `entities=0`, `monsters=0`, `noGameplay=yes`.

## Architecture rule

DoomRPG-RE desktop/J2ME is the executable behavior/format reference, not the permanent ESP32 architecture.

```text
original data/behavior
 -> native pack-backed parsers
 -> compact immutable map
 -> small explicit mutable overlays
 -> native event/script ownership
 -> native gameplay effects
 -> native renderer
```

Never reintroduce runtime ZIP access, map-wide `shapeData`, map-wide `mediaTexels`, pointer-heavy desktop map structures or legacy `Render_t` ownership as a shortcut.

## Remaining MAP_INTRO families

After GIVEMAP, still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
18 EV_HIDE
27 EV_SAVEGAME
```

SHOW/HIDE are deliberately not treated as simple sprite visibility: legacy semantics also kill/link/unlink entities. They require a later explicit compact native entity/topology boundary. CHANGEMAP and SAVEGAME remain larger ownership boundaries.

## Milestone workflow

1. Branch from exact latest hardware-validated `main`.
2. Keep one coherent bounded objective per branch.
3. Recover exact legacy semantics before native ownership design.
4. Fail closed before unsupported/unsafe effects.
5. Validate normal optimized `esp32-cyd` on the real classic CYD.
6. Preserve exact RAM/fingerprint/hardware evidence.
7. Mark merge-ready only after implementation + hardware + docs agree.
8. Keep all post-hardware commits docs-only unless another firmware is flashed.

Current recommendation: **merge `agent/esp32-map1-native-givemap-state`**.