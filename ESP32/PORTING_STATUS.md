# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #60 — native EV_SAVEGAME route owner
main = 50ed329801fe99917ef2f848ee13e742ae7734ab
hardware-tested firmware = 42497b80c6158300ec3fa7b8eb8af6cee643f59e
```

Merged evidence: [`MAP1_NATIVE_SAVE_ROUTE.md`](MAP1_NATIVE_SAVE_ROUTE.md).

## Current merge-ready milestone

```text
branch = agent/esp32-map1-native-change-map-intent
base   = 50ed329801fe99917ef2f848ee13e742ae7734ab
hardware-tested firmware = 93e0be24558ebffcbc9f60ef0ced54f29274ab28
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Detailed evidence: [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md).

This milestone owns only `2 / EV_CHANGEMAP` as a compact caller-owned pending transition intent. It does not trigger the later texture-7 transition consumer: no sound, level stats, menu transition or map load occurs.

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

## Hardware-proven fingerprints through CHANGEMAP

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
changeMapOwnerFNV      = f75eb7c7
changeMapResultFNV     = 2f40c9be
changeMapContentFNV    = f7a79d99
changeMapInitialFNV    = 69691905
changeMapSampleFNV     = 4e4ebeac
legacyTransitionFNV    = 79ab740c
playerStatsFNV         = 0b2ae445
```

## Persistent native RAM ownership

Hardware-proven heap remains:

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

Hardware-proven CHANGEMAP value types:

```text
EspMapChangeMapState  = 16 B
EspMapChangeMapResult = 20 B
persistent heap       = 0 B
```

The probe temporarily opens the native PAK only to verify the real destination string. The permanent CHANGEMAP executor performs no PAK I/O.

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

CHANGEMAP intent:
  refs=1 pending=1 zeroParam=0 showStats=1 directLoad=0 removable=0 fallbackMap=0
  ownerBytes=16 resultBytes=20 persistentHeapBytes=0
  ownerFNV=f75eb7c7 resultFNV=2f40c9be contentFNV=f7a79d99
  rollback=1/1 reapplyExact=1 closedPackApply=1
```

## EV_CHANGEMAP permanent contract

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

The native milestone owns only pending state:

```text
rawParam
map-local EspMapStringRef
source event/command provenance
active flag
```

Deferred result metadata:

```text
pending + showStats=1 -> ADD_LEVEL_STATS | SHOW_STATS_MENU = 0x03
pending + showStats=0 -> ADD_LEVEL_STATS | LOAD_MAP        = 0x05
pending=0             -> no deferred effects
```

Sound 5068 remains outside this opcode owner because it belongs to the later texture-7 transition-door trigger.

## CHANGEMAP real-CYD proof

Complete MAP_INTRO corpus:

```text
refs=1
pending=1
zeroParam=0
showStats=1
directLoad=0
removable=0
fallbackMap=0
stateExecRefused=1
mapNameBytes=13
maxMapName=13
```

Canonical real command:

```text
cmd2 event1 off1
arg1=80000000 arg2=00000100
mapString=0
name="/junction.bsp"
targetMap=9 / MAP_JUNCTION
spawnParam=0
showStats=1
effects=03
pending=1
handled=1
removeIfHandled=0
```

Owner proof:

```text
initialOwnerFNV=69691905
sampleOwnerFNV=4e4ebeac
rollback=1/1
reapplyExact=1
closedPackApply=1
activeAtPark=0
```

Fail closed:

```text
unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1
nullState=1 nullResult=1 reset=1 stateAtomic=yes
```

PAK / RAM witness:

```text
heapOpen=63800 transientHeapCost=4376 largestOpen=34804
packIO=yes verificationOnly=yes executorPackIO=no persistentHeapBytes=0
heap8=68176->68176
largest8=34804->34804
frameFNV=e36ac6fd->e36ac6fd
```

Protected inherited/native state stayed exact:

```text
arenaFNV=c3882516
mapStateFNV=cd99b98e
scriptFNV=f9e3d9df
automapStateFNV=669b1aa7
```

Legacy witnesses stayed exact:

```text
notebookFNV       = 4d7705c5 -> 4d7705c5
keys              = 00000000 -> 00000000
hudFNV            = 505b1255 -> 505b1255
passwordCanvasFNV = 214171cf -> 214171cf
continuationFNV   = e2ba14a5 -> e2ba14a5
saveRouteFNV      = 9bcfe135 -> 9bcfe135
transitionFNV     = 79ab740c -> 79ab740c
statsFNV          = 0b2ae445 -> 0b2ae445
legacyRuntimeClear= yes
```

Final boundary:

```text
transitionArmedProven=yes
transitionTriggered=no
statsMutation=no
menuMutation=no
mapLoad=no
framebufferMutation=no
entities=0 monsters=0
ST_PLAYING not reached
```

Stable PARK heartbeats:

```text
35181 ms: heap=133940 heap8=68176 largest8=34804 all reported subsystems ready
40182 ms: heap=133940 heap8=68176 largest8=34804 all reported subsystems ready
```

Absolute heap/frame values can differ across builds; acceptance uses same-build stability plus canonical fingerprints.

## Remaining MAP_INTRO families

Only these remain unowned:

```text
7  EV_SHOW
18 EV_HIDE
```

They remain intentionally deferred because their exact legacy semantics include entity death/link/unlink and tile entity topology, not merely sprite visibility.

## Merge recommendation

**MERGE `agent/esp32-map1-native-change-map-intent`.**

Hardware-tested firmware content:

```text
93e0be24558ebffcbc9f60ef0ced54f29274ab28
```

All later commits must remain documentation-only unless another firmware is flashed.

After merge, recover the true new `main`, reread this file, `DOCUMENTATION.md`, the merged CHANGEMAP archive and exact SHOW/HIDE legacy behavior before selecting the final topology milestone.
