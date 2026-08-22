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

## Current merge-ready milestone

[`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md) owns `2 / EV_CHANGEMAP` as a pending native transition intent.

```text
branch = agent/esp32-map1-native-change-map-intent
base   = 50ed329801fe99917ef2f848ee13e742ae7734ab
hardware-tested firmware = 93e0be24558ebffcbc9f60ef0ced54f29274ab28
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Real-CYD CHANGEMAP corpus:

```text
refs=1 pending=1 zeroParam=0 showStats=1 directLoad=0
removable=0 fallbackMap=0 stateExecRefused=1
ownerBytes=16 resultBytes=20 persistentHeapBytes=0
mapNameBytes=13 maxMapName=13
ownerFNV=f75eb7c7 resultFNV=2f40c9be contentFNV=f7a79d99
rollback=1/1 reapplyExact=1 closedPackApply=1
```

Canonical command:

```text
cmd2 event1 off1
arg1=80000000 arg2=00000100
mapString=0
name="/junction.bsp"
targetMap=9 / MAP_JUNCTION
spawnParam=0
showStats=1
effects=03
pending=1 handled=1 removeIfHandled=0
```

The milestone mirrors only the bytecode-time assignment of legacy `Game.changeMapParam`. It does not invoke the later texture-7 transition consumer.

Deferred effects remain metadata only:

```text
showStats -> level stats + stats menu
no stats  -> level stats + map load
```

Transition sound `5068`, actual target-map load and pending-state consumption remain deferred.

The permanent executor uses only the resident native runtime/string span and performs no PAK I/O. Hardware proved the same sample can be armed identically after the verification PAK is closed.

## Current hardware-proven boundary

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

Latest same-build CHANGEMAP witness:

```text
heap8=68176->68176
largest8=34804->34804
frameFNV=e36ac6fd->e36ac6fd
transient PAK cost=4376 B
persistent CHANGEMAP heap=0 B
transitionFNV=79ab740c->79ab740c
statsFNV=0b2ae445->0b2ae445
```

Final PARK boundary:

```text
nativeChangeMapIntent=yes
transitionArmedProven=yes
transitionTriggered=no
statsMutation=no
menuMutation=no
mapLoad=no
framebufferMutation=no
entities=0 monsters=0 noGameplay=yes
```

Stable heartbeats:

```text
35181 ms: heap=133940 heap8=68176 largest8=34804
40182 ms: heap=133940 heap8=68176 largest8=34804
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

## Remaining MAP_INTRO families

After CHANGEMAP, only these remain:

```text
7  EV_SHOW
18 EV_HIDE
```

They are entity-topology operations, not simple visibility bits: legacy behavior includes entity death, linking/unlinking and tile-chain traversal. They require the final explicit compact native sprite/entity-topology boundary.

Current recommendation: **merge `agent/esp32-map1-native-change-map-intent`**.

After merge, recover the exact new `main` before selecting or implementing the final SHOW/HIDE milestone.
