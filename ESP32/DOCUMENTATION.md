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
| [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) | SHOW/HIDE compact topology; all real MAP_INTRO opcode families owned | #62 | `ed5cd9a09c9ae36f999661f4284f64400681b1af` |
| [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md) | pure map-derived `Player_addLevelStats()` snapshot | #63 | `533784b5483e14a12558fb08c9331d8b744caa88` |

## Current merge-ready milestone

[`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md) applies the hardware-proven exit snapshot to a small pointer-free native player state.

```text
branch = agent/esp32-native-player-exit-state
base   = 533784b5483e14a12558fb08c9331d8b744caa88
hardware-tested firmware = f8c5a1c398c0946025aef976f7a997589bae4923
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Permanent ABI:

```text
EspPlayerExitState       = 28 B
EspPlayerExitApplyResult = 28 B
persistent heap          = 0 B
```

The owner contains only the fields actually written by recovered `Player_addLevelStats()`:

```text
totalTime / totalMoves
completedLevels
killedMonstersLevels
foundSecretsLevels
berserkerTics
familiarActive
```

No familiar/Entity pointer is retained. `elapsedTimeMs` and current `levelMoves` are explicit caller inputs.

### Real-CYD deterministic proof

With the hardware-proven intro snapshot `effects=1f`:

```text
stateBytes=28 resultBytes=28
elapsed=12345 moves=37
initialFNV=940b0171
appliedFNV=298eaaa4
resultFNV=5d10a566

totalTime  10203040 -> 10206079
totalMoves 01020304 -> 01020329
completed  00000004 -> 00000005
killed     00000008 -> 00000008
secrets    00000010 -> 00000010
berserker  9 -> 0
familiar   1 -> 0
```

Mask/gate proof:

```text
repeatIdempotent=1
allMasks=1
allStateFNV=c93e8128
noStatsGate=1
mapId2Gate=1
```

Live legacy projection:

```text
elapsed=64325
moves=0
projection=1
liveStateFNV=57fce418
legacyPlayerUnchanged=yes
```

The elapsed value is run-specific; exact projection and legacy equality are the contract.

### Fail closed / pointer boundary

```text
rollbackFNV=940b0171
rollback=1
familiarSemanticOnly=yes
entityPointerStored=no
nullState=1
nullStats=1
nullResult=1
effectMismatch=1
bitMismatch=1
rangeMismatch=1
stateAtomic=yes
```

### RAM and legacy integrity

```text
persistent native heap = 18008 B
candidate addition     = 0 B
heap8     65632 -> 65632
largest8  34804 -> 34804
frameFNV  ef79123a -> ef79123a
lineFNV   e5e74861
topologyFNV=3f321e43
```

Legacy state stayed untouched:

```text
playerExitFNV f5cbf9f5 -> f5cbf9f5
transitionFNV f450c49f -> f450c49f
Player_addLevelStatsCalled=no
playerMutation=no
menuMutation=no
transitionTriggered=no
legacyRuntimeClear=yes
```

Final PARK:

```text
nativePlayerExitState=yes
nativeExitStats=yes
playerMutationProven=yes
legacyPlayerMutation=no
persistentBytes=0
entities=0
monsters=0
noGameplay=yes
```

## Hardware-proven boundary

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
playerExitAppliedFNV   = 298eaaa4

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
 -> compact immutable map + explicit mutable owners
 -> complete native MAP_INTRO event-family ownership
 -> native gameplay/effect consumers
      -> level-exit stats snapshot       [hardware-proven]
      -> player exit-state application   [hardware-proven]
      -> stats-menu intent/consumer      [next natural boundary]
      -> CHANGEMAP / Junction map swap
 -> native renderer/gameplay loop
```

Still outside current ownership:

```text
stats-menu intent/consumer
actual CHANGEMAP / Junction map swap
full native entity/monster gameplay
legacy-world-free gameplay loop
native gameplay renderer
ST_PLAYING progression
sound playback
```

## Merge recommendation

```text
MERGE agent/esp32-native-player-exit-state
```

Hardware-tested firmware is `f8c5a1c398c0946025aef976f7a997589bae4923`. All commits after it are documentation-only.
