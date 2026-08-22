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

## Current merge-ready milestone

[`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) owns the final two real MAP_INTRO opcode families, `7 / EV_SHOW` and `18 / EV_HIDE`, through one compact native map-sprite/entity topology.

```text
branch = agent/esp32-map1-native-show-hide-topology
base   = fc39ac60757e0d992e3729a5044a9d83e9994971
hardware-tested firmware = f881ccdad20d950462dd781456c340e792f59ec3
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Final MAP_INTRO event-family result

All 16 real opcode IDs are now explicitly owned natively:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

There is **no remaining real MAP_INTRO opcode family without a native ownership/execution boundary**.

### SHOW/HIDE hardware canons

Compact topology:

```text
sprites=344
7 B/sprite
payload=2408 B
actual heap=2424 B
allocator overhead=16 B
stateFNV=3f321e43

entityDefCount=115
entities=220
hasDef=213
fallback=7
linked=209
hiddenSprites=11
hiddenEntities=11
enemies=30
destructibles=13
nextOrder=209
```

Corpus:

```text
refs=12
show=11
hide=1
removable=12
stateExecRefused=12
showMutated=11
hideMutatedIsolated=0
hideNoMutation=1
blockersFound=2
blockersRemoved=2
deferredDeaths=2
rollback=12/12
```

Fingerprints:

```text
showResultFNV       = 6029eb3c
hideResultFNV       = d24f5bae
showStateFNV        = b6a45f47
hideStateFNV        = bec68187
contextAfterShowFNV = 2de723aa
contextAfterHideFNV = bb1d78a4
entityTopologyFNV   = f8f9b485
```

The sole real HIDE is a valid source-state no-op. The same event contains an earlier real SHOW on the same tile, and hardware proved the contextual sequence hides/unlinks exactly one entity, then becomes idempotent on the second HIDE.

```text
SHOW cmd165 event60 off1 sprite0 tile706
HIDE cmd173 event60 off9 tile706
hidden=1
secondHidden=0
contextProven=1
idempotent=1
```

Result ABI:

```text
EspMapShowResult = 26 B
EspMapHideResult = 18 B
```

### Persistent native RAM

Hardware-proven persistent total is now:

```text
immutable arena          14112 B
mutable tile state        1040 B
mutable script state       100 B
mutable line state         136 B
mutable texture state       76 B
mutable automap state      120 B
mutable sprite topology   2424 B
-------------------------------
total                    18008 B
```

`largest8=34804` remains unchanged.

### Integrity

Real-CYD final PARK proved:

```text
allMapIntroOpcodeFamiliesOwned=yes
worldMutationProven=yes
worldRestored=yes
legacyEntityMutation=no
framebufferMutation=no
entities=0
monsters=0
noGameplay=yes
```

Inherited native fingerprints remain exact, framebuffer is unchanged, and the legacy entity topology witness remains `f8f9b485 -> f8f9b485`.

## Architecture boundary after MAP_INTRO event-family ownership

This milestone is a major boundary, not the end of the port.

Still intentionally outside current native ownership:

```text
full native entity/monster gameplay
consumption of deferred blocker death/gameplay effects
actual CHANGEMAP transition consumer
legacy-world-free gameplay loop
native gameplay renderer integration
actual ST_PLAYING progression
actual sound playback
```

The permanent invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP forbidden
/DoomRPG-ESP32.pak is native backing store
entities=0
monsters=0
ST_PLAYING not reached
```

## Merge recommendation

```text
MERGE agent/esp32-map1-native-show-hide-topology
```

All post-hardware commits after `f881ccdad20d950462dd781456c340e792f59ec3` must be documentation-only. After merge, recover from the true new `main` before selecting the next gameplay/effect-consumer milestone.
