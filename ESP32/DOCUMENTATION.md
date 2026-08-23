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
| [`MAP1_NATIVE_ACCESS.md`](MAP1_NATIVE_ACCESS.md) | allocation-free access | #44 | `ddcf19e6166f63fec1c608234ee3e253ea` |
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
| [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) | SHOW/HIDE compact topology; all MAP_INTRO opcode families owned | #62 | `ed5cd9a09c9ae36f999661f4284f64400681b1af` |
| [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md) | map-derived level-exit stats | #63 | `533784b5483e14a12558fb08c9331d8b744caa88` |
| [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md) | pointer-free player exit writes | #64 | `3759bcd12a3f6d36a6a696457110ab27474c24b8` |
| [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md) | LEVEL/OVERALL stats-menu intent | #65 | `c8679133351fa00e01a67103386b7676660c4a6e` |
| [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md) | map catalog + Junction PAK/BSP preflight | #66 | `9f981f490282200f216aef66d22608d2244beb00` |

## Current candidate

[`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md) introduces a generic explicit resident-map lifecycle and proves the intended source/target ownership order with a reversible hardware transaction.

```text
branch = agent/esp32-native-resident-handoff
base   = 9f981f490282200f216aef66d22608d2244beb00
firmware candidate = f71520281254ff9d0b2d5e4be1b3611e29ca87c4
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

### Permanent lifecycle

```text
EspMapResidentLifecycle_resetAll
EspMapResidentLifecycle_isEmpty
EspMapResidentLifecycle_isReady
EspMapResidentLifecycle_capture
EspMapResidentLifecycle_loadFromEmpty
```

The key invariant is explicit ownership of destruction:

```text
loadFromEmpty() never tears down a live source
live owners -> NOT_EMPTY before PAK I/O
resetAll()  -> only explicit destructive primitive
```

Teardown order:

```text
topology -> automap -> texture -> line -> script -> map state -> runtime
```

Build order from EMPTY:

```text
runtime -> map state -> script -> line -> texture -> automap -> topology
```

The lifecycle owns one temporary PAK session, uses `/entities.db` for topology, closes PAK before return and returns EMPTY on partial-build failure.

### Resident snapshot

```text
EspMapResidentSnapshot = 96 B
```

Entrance logical payload target:

```text
runtime=14112
state=1024
script=81
line=120
texture=60
automap=103
topology=2408
payload=17908 B
actual hardware owner heap=18008 B
expected allocator overhead=100 B
snapshotFNV static target=97090c81
```

### Temporary Junction residency target

Already-proven Junction source/plan:

```text
resource=/junction.bsp
resourceMapId=9
gameplayLoadMapId=2
bytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
nodes=77 lines=207 sprites=48 events=66 byteCodes=319 strings=126
runtime plan=8867 B
```

Current owner formulas predict:

```text
runtime=8867
state=1024
script=73
line=52
texture=26
automap=32
topology=336
payload=10410 B
```

The CYD must establish the actual Junction heap cost, allocator overhead, seven resident FNVs, whole snapshot FNV and topology counts.

### Reversible proof sequence

```text
SOURCE Entrance
 -> reject hidden load with NOT_EMPTY
 -> inventory Entrance + Junction
 -> resetAll
 -> EMPTY1
 -> prove PACK_BUSY ownership while empty
 -> build full Junction resident set
 -> capture Junction twice exactly
 -> resetAll
 -> EMPTY2 == EMPTY1 heap/largest
 -> rebuild Entrance
 -> RESTORED == SOURCE byte-for-byte
```

If anything fails after source release, the probe attempts immediate `/intro.bsp` recovery before PARK.

Strict acceptance:

```text
EMPTY1 - SOURCE heap = 18008 B
EMPTY2 heap/largest == EMPTY1
RESTORED heap/largest == SOURCE
final heap delta=0
PAK closed
framebuffer unchanged
legacy Player/transition unchanged
legacy Render runtime clear
DoomCanvas_loadMapCalled=no
menuMutation=no
legacyPlayerMutation=no
mapSwapCommitted=no
targetLeftResident=no
entities=0 monsters=0
ST_INTRO page=3
```

### Expected Serial focus

```text
[RESIDENTHANDOFFPROBE] ARMED ...
=== Doom RPG ESP32-native reversible resident handoff ===
[BSPREAD] ... /intro.bsp ...
[BSPREAD] ... /junction.bsp ...
[MAPRT] ...
[MAPSTATE] ...
[MAPLINESTATE] ...
[MAPLINETEX] ...
[MAPAUTOMAP] ...
[RESIDENTHANDOFF] SOURCE ...
[RESIDENTHANDOFF] EMPTY1 ...
[RESIDENTHANDOFF] GATES ...
[RESIDENTHANDOFF] JUNCTION ...
[RESIDENTHANDOFF] JUNCTIONFNV ...
[RESIDENTHANDOFF] JUNCTIONTOPO ...
[RESIDENTHANDOFF] EMPTY2 ...
[RESIDENTHANDOFF] RESTORE ...
[RESIDENTHANDOFF] RAM ...
[RESIDENTHANDOFF] LEGACY ...
[RESIDENTHANDOFF] PARK ...
[ALIVE] ...
```

Build/flash with normal `esp32-cyd`. No CI status is published and no local build/hardware PASS is claimed.

## Hardware-proven boundary through PR #66

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
 -> complete MAP_INTRO event-family ownership
 -> exit/transition consumers
      -> level-exit stats          [hardware-proven]
      -> player exit-state         [hardware-proven]
      -> stats-menu intent         [hardware-proven]
      -> generic map catalog       [hardware-proven]
      -> target PAK/BSP preflight  [hardware-proven]
      -> explicit resident lifecycle + reversible handoff [candidate]
      -> committed transition state machine
      -> spawn/loadType handoff
 -> native gameplay/render loop
```

Still outside current candidate:

```text
committed Junction residency
native transition point-of-no-return state machine
spawn/loadType handoff
actual stats-menu rendering/input
full native entity/monster gameplay
ST_PLAYING progression
native gameplay renderer
sound playback
```
