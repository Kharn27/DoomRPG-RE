# ESP32 documentation map

This file defines the current ESP32 CYD documentation map.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: detailed implementation and hardware evidence.

## Merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`START_GAME.md`](START_GAME.md) | start/lifecycle cleanup | #36 | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) | first fitted intro frame | #37 | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| [`INTRO_CLOCK.md`](INTRO_CLOCK.md) | bounded intro clock | #38 | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| [`INTRO_INPUT.md`](INTRO_INPUT.md) | intro input progression | #39 | `7ba68955a9b0979924c5e759736fb483589be744` |
| [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md) | intro teardown | #41 | `897e982f4b37039d984b13265beaa68a83dce98b` |
| [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) | BSP inventory | #42 | `c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e` |
| [`MAP1_NATIVE_RUNTIME.md`](MAP1_NATIVE_RUNTIME.md) | compact immutable runtime | #43 | `503fdd66fae625a45446fb4ea0853abc71d7dda3` |
| [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md) | allocation-free access | #44 | `ddcf19e6166f210a6f63fec1c608234ee3e253ea` |
| [`MAP1_NATIVE_STATE.md`](MAP1_NATIVE_STATE.md) | mutable tile state | #45 | `feec8a7fcb839dbd9f6de708f56f26b69a1e79e9` |
| [`MAP1_NATIVE_EVENTS.md`](MAP1_NATIVE_EVENTS.md) | tile-event lookup | #46 | `438cffabaaaaa3dc3b45486f56eacec1a047edcf` |
| [`MAP1_NATIVE_EVENT_DESCRIPTOR.md`](MAP1_NATIVE_EVENT_DESCRIPTOR.md) | event descriptor/linkage | #47 | `a3e629ba0be6b4dcc6329b17f18a0c3ca9828958` |
| [`MAP1_NATIVE_EVENT_FILTER.md`](MAP1_NATIVE_EVENT_FILTER.md) | script state/filtering | #48 | `0c8a52549ebb436139f7cd5c8b4ee63bdd175907` |
| [`MAP1_NATIVE_OPCODE_EXEC1.md`](MAP1_NATIVE_OPCODE_EXEC1.md) | first state opcode execution | #49 | `6e43ef059db52783b7264e84579216cb2572a1e2` |
| [`MAP1_NATIVE_UI_INTENT.md`](MAP1_NATIVE_UI_INTENT.md) | UI/string intents | #50 | `9a5e8ade361180d220f2b3614a443e5efb0d27bd` |
| [`MAP1_NATIVE_STRING_READER.md`](MAP1_NATIVE_STRING_READER.md) | bounded pack strings | #51 | `526640b12d978fdbe9c8a9239c37db2fca95cddd` |
| [`MAP1_NATIVE_STATUS_MESSAGE.md`](MAP1_NATIVE_STATUS_MESSAGE.md) | status-message owner | #52 | `40b61af5e2115266d4d03dddcc3175850538b0f5` |
| [`MAP1_NATIVE_DIALOG_OWNER.md`](MAP1_NATIVE_DIALOG_OWNER.md) | dialog owner | #53 | `395418510207bf24ac45ddbb4c4c15db3ddc8998` |
| [`MAP1_NATIVE_NOTEBOOK.md`](MAP1_NATIVE_NOTEBOOK.md) | notebook owner | #54 | `03002f79eb03bdcb4c9e430c43e4693dab47e44b` |
| [`MAP1_NATIVE_KEY_GATE.md`](MAP1_NATIVE_KEY_GATE.md) | key gate | #55 | `03c4275f2abfd6671c8bf499c075435d7b61ab97` |
| [`MAP1_NATIVE_PASSWORD_OWNER.md`](MAP1_NATIVE_PASSWORD_OWNER.md) | password pause/submit owner | #56 | `3c113cc047aeb613f2ba4ab7905e92487c796f80` |
| [`MAP1_NATIVE_LINE_DOOR_STATE.md`](MAP1_NATIVE_LINE_DOOR_STATE.md) | OPEN/CLOSE world state | #57 | `e4fb32f41b7074bbb433e64f4c824edb2167cf50` |
| [`MAP1_NATIVE_UNLOCK_STATE.md`](MAP1_NATIVE_UNLOCK_STATE.md) | UNLOCK world state | #58 | `7503b379185db3f05713eb34f1762173edb977d0` |
| [`MAP1_NATIVE_GIVEMAP_STATE.md`](MAP1_NATIVE_GIVEMAP_STATE.md) | GIVEMAP automap state | #59 | `9891a25d700f9ffe1be044ac4a7629c3487604ec` |
| [`MAP1_NATIVE_SAVE_ROUTE.md`](MAP1_NATIVE_SAVE_ROUTE.md) | SAVEGAME future-save route | #60 | `50ed329801fe99917ef2f848ee13e742ae7734ab` |
| [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md) | CHANGEMAP pending transition intent | #61 | `fc39ac60757e0d992e3729a5044a9d83e9994971` |

## Current candidate

[`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) owns the final two real MAP_INTRO opcode families, `7 / EV_SHOW` and `18 / EV_HIDE`, through one compact native map-sprite/entity topology.

```text
branch = agent/esp32-map1-native-show-hide-topology
base   = fc39ac60757e0d992e3729a5044a9d83e9994971
firmware candidate = 93d26e171e8a98f3824b3071e01b9234c8ebe6c3
status = IMPLEMENTED; CORRECTED REAL-CYD VALIDATION PENDING
```

Permanent topology storage:

```text
344 map sprites
7 B / sprite
payload = 2408 B

entity type       1 B
entity subtype    1 B
visual state      1 B
link state/tile   2 B
link order        2 B
```

Expected result ABIs:

```text
EspMapShowResult = 26 B
EspMapHideResult = 18 B
```

SHOW owns the directly deterministic visual/link projection and reports deferred blocker gameplay instead of invoking legacy `Entity_died()`. HIDE traverses compact native tile links and hides/unlinks eligible non-enemy map-sprite entities. Runtime ZIP access remains forbidden; `/entities.db` is read only during owner build from `/DoomRPG-ESP32.pak`.

## First SHOW/HIDE hardware attempt and correction

Initial firmware `1e9760de2269f57ec24dcea0fc16774a119ae65a` built the compact owner but failed the temporary `topology/corpus audit`.

Diagnostic firmware `3a7dc83b14e8de47827b51bee12b0c907635ffc3` proved the permanent owner and all 12 real opcode applications were individually valid:

```text
sprites=344 storageBytes=2408
actual heap delta=2424 B
stateFNV=3f321e43
entities=220 hasDef=213 fallback=7 linked=209
hiddenEntities=11 enemies=30 destructibles=13 nextOrder=209
initial audit=1

refs=12 show=11 hide=1
showOk=11 hideOk=1
showAlreadyLinked=0 showRandomBlocker=0
showOtherFailure=0 hideOtherFailure=0
finalFNV=3f321e43
```

The sole HIDE is a legitimate source-state no-op:

```text
cmd173 event60 off9
tile=2,22 / index706
status=OK hidden=0 effects=00 handled=1 remove=1
```

The failing probe had incorrectly required an isolated HIDE mutation. This was a test-harness assumption, not a permanent owner failure.

The same event has an earlier real SHOW on the same tile:

```text
cmd165 event60 off1
sprite0 tile706
FNV 3f321e43 -> 2de723aa
```

The corrected final probe therefore validates both:

```text
isolated real corpus:
  HIDE may be handled/no-op from source state

contextual mutation proof:
  same-event/same-tile SHOW
  -> real HIDE must hide/unlink
  -> second HIDE must be handled/idempotent
  -> reset exact
```

No permanent `esp_map_sprite_topology.c` code was changed by this correction.

## Current hardware-proven boundary through PR #61

```text
persistent native heap = 15584 B
arenaFNV               = c3882516
mapStateFNV            = cd99b98e
scriptFNV              = f9e3d9df
lineStateFNV           = e5e74861
lineTextureStateFNV    = f1fc1875
automapStateFNV        = 669b1aa7
giveMapFNV             = 98c7ac59
saveRouteOwnerFNV      = 06ea6ea8
saveRouteResultFNV     = c2ecb064
saveRouteContentFNV    = 725845aa
changeMapOwnerFNV      = f75eb7c7
changeMapResultFNV     = 2f40c9be
changeMapContentFNV    = f7a79d99
legacyTransitionFNV    = 79ab740c
playerStatsFNV         = 0b2ae445
```

The SHOW/HIDE topology values above are diagnostic observations, not yet promoted to final hardware canons until firmware `93d26e17...` passes its corrected probe.

## Corrected validation target

The final firmware must establish:

```text
initial topology audit PASS
refs = SHOW + HIDE
stateExecRefused = refs
rollback = refs/refs
SHOW mutation coverage > 0
isolated HIDE handled (mutation or no-op accepted)
same-event/same-tile context SHOW found
context HIDE hides/unlinks >= 1 entity
second context HIDE handled/no-op with unchanged FNV
showRepeatGuard=1
hideContext=1
hideIdempotent=1
reset=1
showResultBytes=26
hideResultBytes=18
```

RAM target:

```text
topology payload = 2408 B
2408 <= actual heap cost <= 2536 B
largest8 >= 32768 B
new persistent total = 15584 B + hardware-measured topology allocation
```

Final integrity remains:

```text
legacy Render runtime clear
legacy entity topology unchanged
entities=0 monsters=0
framebuffer unchanged
shapeData=NULL
mediaTexels=NULL
ST_PLAYING not reached
```

## Architecture rule

DoomRPG-RE desktop/J2ME remains the executable behavior/format reference, not the final ESP32 architecture.

```text
original behavior/data
 -> native pack-backed parsers
 -> compact immutable map
 -> explicit small mutable owners
 -> native event/script ownership
 -> native gameplay/effect consumers
 -> native renderer
```

Never reintroduce runtime ZIP access, map-wide `shapeData`, map-wide `mediaTexels`, pointer-heavy desktop map structures or legacy `Render_t` ownership as a shortcut.

## MAP_INTRO event-family frontier

If firmware `93d26e171e8a98f3824b3071e01b9234c8ebe6c3` passes, there will be **no remaining real MAP_INTRO opcode ID without an explicit native ownership/execution boundary**.

That closes event-family ownership only. Native entity/monster gameplay, deferred effect consumers, transition consumption, rendering and progression to `ST_PLAYING` remain later work.

Do not merge this candidate until corrected real-CYD hardware validation passes and all later commits are verified documentation-only.
