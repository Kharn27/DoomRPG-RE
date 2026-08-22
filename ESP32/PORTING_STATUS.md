# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #60 — native EV_SAVEGAME route owner
main = 50ed329801fe99917ef2f848ee13e742ae7734ab
hardware-tested firmware = 42497b80c6158300ec3fa7b8eb8af6cee643f59e
```

Merged evidence: [`MAP1_NATIVE_SAVE_ROUTE.md`](MAP1_NATIVE_SAVE_ROUTE.md).

## Current candidate

```text
branch = agent/esp32-map1-native-change-map-intent
base   = 50ed329801fe99917ef2f848ee13e742ae7734ab
firmware candidate = 93e0be24558ebffcbc9f60ef0ced54f29274ab28
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md).

The candidate owns only `2 / EV_CHANGEMAP` as a caller-owned pending transition intent. It does **not** trigger the later texture-7 transition consumer: no sound, level stats, menu transition or map load occurs in this milestone.

## Permanent invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
runtime ZIP = forbidden
backing     = /DoomRPG-ESP32.pak
```

## MAP_INTRO identity

```text
/intro.bsp / Entrance
bytes=21823 crc32=623f34e4
nodes=223 lines=480 mapSprites=344 events=93 byteCodes=265
strings=94 stringData=7779 maxString=313
```

Real opcode IDs:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Hardware-proven fingerprints through PR #60

```text
arenaFNV               = c3882516
mapStateFNV            = cd99b98e
scriptFNV              = f9e3d9df
lineStateFNV           = e5e74861
lineTextureStateFNV    = f1fc1875
automapStateFNV        = 669b1aa7
lineDoorFNV            = b1c9d297
unlockFNV              = 261d756a
giveMapFNV             = 98c7ac59
giveMapMutatedAutoFNV  = 9d03ca2d
giveMapMutatedMapFNV   = e21edbce
saveRouteOwnerFNV      = 06ea6ea8
saveRouteResultFNV     = c2ecb064
saveRouteContentFNV    = 725845aa
saveRouteInitialFNV    = 9a00a0bd
saveRouteSampleFNV     = 7e69bd59
legacySaveRouteFNV     = 9bcfe135
```

Current CHANGEMAP candidate will establish:

```text
changeMapOwnerFNV   = pending
changeMapResultFNV  = pending
changeMapContentFNV = pending
initialOwnerFNV     = pending
sampleOwnerFNV      = pending
legacyTransitionFNV = pending witness
playerStatsFNV      = pending witness
```

## Persistent native RAM ownership

Hardware-proven heap entering this candidate:

```text
immutable arena        14112 B
mutable tile state      1040 B
mutable script state     100 B
mutable line state       136 B
mutable texture state     76 B
mutable automap state    120 B
-----------------------------
total                  15584 B
```

Current candidate value types:

```text
EspMapChangeMapState  = 16 B expected
EspMapChangeMapResult = 20 B expected
persistent heap       = 0 B expected
```

The probe temporarily opens the native PAK only to verify real destination names. The permanent CHANGEMAP executor performs no PAK I/O.

## Hardware-proven recent families

```text
OPEN/CLOSE:
  refs=71 mutated=29 locked=18 alreadyTarget=24 rollback=29/29
  lineStateFNV=e5e74861 lineDoorFNV=b1c9d297

UNLOCK:
  refs=6 mutated=6 lockMutated=6 textureMutated=6 rollback=6/6
  lineTextureStateFNV=f1fc1875 unlockFNV=261d756a

GIVEMAP:
  refs=1 lines=430 sprites=344 entranceTiles=4 rollback=1/1
  automapStateFNV=669b1aa7 giveMapFNV=98c7ac59

SAVEGAME route:
  refs=1 ownerBytes=46 resultBytes=16 persistentHeapBytes=0
  mapName="/junction.bsp" tile=15,29 destination=992,1888 angle=64
  ownerFNV=06ea6ea8 resultFNV=c2ecb064 contentFNV=725845aa
  rollback=1/1 reapplyExact=1 ownerSurvivesPackClose=1
```

## EV_CHANGEMAP recovered contract

Legacy bytecode execution itself is only:

```text
Game.changeMapParam = arg1
return handled=true
```

The parameter is consumed later by `Game_changeMap()` only when an opened transition door with texture `7` is processed. That later consumer:

```text
plays sound 5068
spawnParam = (rawParam << 1) >> 9
resolves mapStrings[rawParam & 0xff]
updates level stats
bit31 SHOWSTATS -> stats menu
otherwise       -> map load
clears changeMapParam
```

The candidate therefore owns only pending state:

```text
rawParam
map-local EspMapStringRef
source event/command provenance
active flag
```

For rawParam=0, the native owner clears and the command still reports handled=true, matching legacy assignment semantics.

Deferred result metadata:

```text
pending + showStats=1 -> ADD_LEVEL_STATS | SHOW_STATS_MENU = 0x03
pending + showStats=0 -> ADD_LEVEL_STATS | LOAD_MAP        = 0x05
pending=0             -> no deferred effects
```

Sound 5068 is intentionally outside this opcode owner because it belongs to the later texture-7 door transition trigger.

## Real-CYD validation target

The probe must discover rather than guess:

```text
CHANGEMAP refs
pending / zero-param refs
showStats / direct-load refs
removable refs
fallback-map refs
map-name total bytes / max length
first real target map + raw param + spawn param
ownerFNV / resultFNV / contentFNV
initial/sample owner FNV
transient pack-open heap cost
legacy transition witness FNV
player stats witness FNV
```

Acceptance:

```text
refs > 0
pending > 0
pending + zeroParam = refs
showStats + directLoad = pending
stateExecRefused = refs
rollback = refs/refs
ownerBytes = 16
resultBytes = 20
reapplyExact = 1
closedPackApply = 1
activeAtPark = 0
persistentHeapBytes = 0
executorPackIO = no
transitionTriggered = no
statsMutation = no
menuMutation = no
mapLoad = no
```

Fail closed:

```text
unsupported=1
badOffset=1
badDescriptor=1
nullDescriptor=1
nullState=1
nullResult=1
reset=1
stateAtomic=yes
```

Protected inherited state:

```text
arenaFNV=c3882516
mapStateFNV=cd99b98e
scriptFNV=f9e3d9df
lineStateFNV=e5e74861
lineTextureStateFNV=f1fc1875
automapStateFNV=669b1aa7
legacy SAVE route unchanged
legacy transition fields unchanged
player level-stat fields unchanged
legacy Render runtime clear
entities=0 monsters=0 ST_PLAYING not reached
```

## Remaining MAP_INTRO families after candidate PASS

If CHANGEMAP passes, only these remain unowned:

```text
7  EV_SHOW
18 EV_HIDE
```

They remain intentionally deferred because their exact legacy semantics include entity death/link/unlink and tile entity topology, not just sprite visibility.

## Validation target

Build/flash normal optimized `esp32-cyd` from:

```text
agent/esp32-map1-native-change-map-intent
```

Firmware candidate:

```text
93e0be24558ebffcbc9f60ef0ced54f29274ab28
```

Capture `[MAPCHANGEMAP]`, `[MAPCHANGEMAPPROBE]` and a stable `[ALIVE]` heartbeat.

No CI status is published for this firmware candidate. No local build or hardware PASS is claimed.
