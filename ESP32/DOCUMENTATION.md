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
| [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) | SHOW/HIDE compact sprite topology; completes all real MAP_INTRO opcode families | #62 | `ed5cd9a09c9ae36f999661f4284f64400681b1af` |

## Current merge-ready milestone

[`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md) is the first hardware-proven native consumer after complete MAP_INTRO event-family ownership.

```text
branch = agent/esp32-map1-native-level-exit-stats
base   = ed5cd9a09c9ae36f999661f4284f64400681b1af
hardware-tested firmware = f9a05933a00fab26b1c0e2b15375d074161ef2bc
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

The real intro CHANGEMAP has `showStats=1`, so legacy first computes level stats and opens the map-stats menu before a later Junction transition. This milestone now computes the map-derived stats as a pure native 20-byte value without mutating Player/Menu/Game/Render/DoomCanvas.

### Hardware-proven exit snapshot

```text
loadMapId          = 1
showStats          = 1
secrets            = 0 / 6
monsters           = 0 / 30
markCompleted      = 1
markAllSecrets     = 0
markAllMonsters    = 0
completionLevelBit = 00000001
effects            = 1f
statsFNV           = bd41bcfa
resultBytes        = 20
elapsed            = 11 ms
```

The effect byte is:

```text
1f = base exit effects 0f + mark-completed 10
```

Legacy gates are hardware-proven:

```text
showStats=0 -> base effects only
loadMapId=2 -> base effects only
noStatsFNV         = d9532169
noCompletionMapFNV = ceb6ad21
```

### Dynamic owner sensitivity

The collector is not a static MAP_INTRO lookup. Hardware proved it consumes current native mutable owners.

Real SHOW blocker proof:

```text
cmd205 event74 off2
enemyBlockersRemoved=1
topologyFNV 3f321e43 -> 723e7300 -> 3f321e43
mutated statsFNV = 5155b517
```

Real secret line proof:

```text
line39 initialOpen=0 proof=1
lineFNV e5e74861 -> 6694b0e1 -> e5e74861
```

### RAM and integrity

```text
persistent heap total = 18008 B
candidate addition     = 0 B
heap8     65640 -> 65640
largest8  34804 -> 34804
```

Native owner rollback remained exact and legacy Player/menu/transition state was unchanged:

```text
playerStatsFNV 17e22395 -> 17e22395
transitionFNV  f450c49f -> f450c49f
Player_addLevelStatsCalled=no
menuMutation=no
transitionTriggered=no
```

Final PARK:

```text
nativeExitStats=yes
persistentBytes=0
allMapIntroOpcodeFamiliesOwned=yes
playerMutation=no
menuMutation=no
worldRestored=yes
entities=0
monsters=0
noGameplay=yes
```

## Current hardware-proven boundary

```text
persistent native heap = 18008 B
arenaFNV               = c3882516
mapStateFNV            = cd99b98e
scriptFNV              = f9e3d9df
lineStateFNV           = e5e74861
lineTextureStateFNV    = f1fc1875
automapStateFNV        = 669b1aa7
spriteTopologyFNV      = 3f321e43
levelExitStatsFNV      = bd41bcfa

allMapIntroOpcodeFamiliesOwned=yes
entities=0
monsters=0
ST_PLAYING not reached
shapeData=NULL
mediaTexels=NULL
```

## Architecture direction

```text
original Doom RPG behavior/data
 -> native pack-backed parsers
 -> compact immutable map
 -> explicit mutable owners
 -> complete native MAP_INTRO event-family ownership
 -> native gameplay/effect consumers
      -> level-exit stats snapshot  [hardware-proven]
      -> native player exit-state application
      -> stats-menu intent/consumer
      -> CHANGEMAP / Junction map swap
 -> native renderer
```

Still outside the current merged baseline/candidate boundary:

```text
application of exit effects to native player state
stats-menu owner/consumer
actual CHANGEMAP map swap
full native entity/monster gameplay
legacy-world-free gameplay loop
native gameplay rendering
actual ST_PLAYING progression
sound playback
```

## Merge recommendation

```text
MERGE agent/esp32-map1-native-level-exit-stats
```

Hardware-tested firmware is `f9a05933a00fab26b1c0e2b15375d074161ef2bc`. All commits after it are documentation-only. After merge, recover the true new `main` before choosing the next consumer milestone.
