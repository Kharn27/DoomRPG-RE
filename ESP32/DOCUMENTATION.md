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
| [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md) | pointer-free application of player level-exit writes | #64 | `3759bcd12a3f6d36a6a696457110ab27474c24b8` |
| [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md) | pointer-free LEVEL/OVERALL stats-menu intent | #65 | `c8679133351fa00e01a67103386b7676660c4a6e` |

## Current merge-ready milestone

[`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md) establishes the immutable 13-map resource catalog and read-only target PAK/BSP preflight for Junction.

```text
branch = agent/esp32-native-transition-preflight
base   = c8679133351fa00e01a67103386b7676660c4a6e
hardware-tested firmware = 4d78a66548fab6373c06c67f107f176fc3988b1c
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Resource identity vs gameplay identity

The first diagnostic candidate exposed an important format distinction. Corrected v2 then hardware-proved it:

```text
resourceMapId      = 9 / MAP_JUNCTION / /junction.bsp
gameplayLoadMapId  = 2 / BSP Render.loadMapID
hubProgressionGate = 1
```

The resource ID selects the map asset/lifecycle target. The BSP gameplay ID drives progression/stat bookkeeping. Legacy `Player_addLevelStats()` explicitly treats gameplay ID `2` as the hub/no-completion gate, so the IDs are intentionally distinct.

### Hardware-proven Junction

```text
entryOffset=1974397
bytes=21051
crc32=4a2c5800
fnv1a=fefaf5ca
spawn=943
dir=64
camera=0
floorTex=117
ceilingTex=151

nodes=77
lines=207
mapSprites=48
events=66
byteCodes=319
strings=126
stringData=12235
maxString=380
trailing=0

persistentPlanBytes=8867
readCalls=83
window=256 B
```

The complete target BSP was inventoried twice with identical CRC/FNV and structure.

### Catalog / preflight proof

```text
EspMapTransitionPreflightResult = 56 B
catalog count=13
roundtrip=13
catalogFNV=ce322e3f
preflight resultFNV=108e5c7b
repeatFNV=108e5c7b
repeatExact=1
resourceGameplayDistinct=1
ready=1
```

Fail closed:

```text
target0=1
target14=1
nullResult=1
packBusy=1
busyZero=1
stateAtomic=yes
```

RAM/I/O:

```text
persistent native heap = 18008 B
milestone addition     = 0 B
heap8      65608 -> 65608
largest8   34804 -> 34804
heapOpen   61232
transientPackCost=4376 B
packClosed=yes
```

Entrance and legacy remain untouched:

```text
arenaFNV     c3882516
mapStateFNV  cd99b98e
scriptFNV    f9e3d9df
lineStateFNV e5e74861
lineTextureFNV=f1fc1875
automapFNV   669b1aa7
spriteTopologyFNV=3f321e43

playerFNV     0b2ae445 -> 0b2ae445
transitionFNV f450c49f -> f450c49f
sourceTeardown=no
mapLoad=no
menuMutation=no
mapSwap=no
entities=0
monsters=0
```

Final PARK:

```text
nativeCatalog=yes
nativeTargetPreflight=yes
resourceMapId=9
gameplayLoadMapId=2
targetReady=yes
sourceMapPreserved=yes
packClosed=yes
persistentBytes=0
mapSwap=no
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
statsMenuIntentFNV     = 96afe901
catalogFNV             = ce322e3f
transitionPreflightFNV = 108e5c7b
junctionSourceFNV      = fefaf5ca

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
 -> native exit/transition consumers
      -> level-exit stats             [hardware-proven]
      -> player exit-state            [hardware-proven]
      -> stats-menu semantic intent   [hardware-proven]
      -> generic resource map catalog [hardware-proven]
      -> target PAK/BSP preflight     [hardware-proven]
      -> source-target lifecycle handoff [next]
      -> Junction resident-runtime swap
 -> native gameplay/render loop
```

Still outside:

```text
actual stats-menu rendering/input
source-map teardown ordering / lifecycle handoff
Junction resident-runtime allocation and mutable-owner rebuild
spawn/loadType handoff
full native entity/monster gameplay
native ST_PLAYING progression/rendering
sound playback
```

## Merge recommendation

```text
MERGE agent/esp32-native-transition-preflight
```

Hardware-tested firmware is `4d78a66548fab6373c06c67f107f176fc3988b1c`. All later commits are documentation-only.
