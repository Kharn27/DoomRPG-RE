# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #59 — native EV_GIVEMAP automap state
main = 9891a25d700f9ffe1be044ac4a7629c3487604ec
hardware-tested firmware = 2e0f8f5de93f806380ee254a8dab59a817c73f5d
```

Merged evidence: [`MAP1_NATIVE_GIVEMAP_STATE.md`](MAP1_NATIVE_GIVEMAP_STATE.md).

## Current merge-ready milestone

```text
branch = agent/esp32-map1-native-save-route
base   = 9891a25d700f9ffe1be044ac4a7629c3487604ec
hardware-tested firmware = 42497b80c6158300ec3fa7b8eb8af6cee643f59e
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Detailed evidence: [`MAP1_NATIVE_SAVE_ROUTE.md`](MAP1_NATIVE_SAVE_ROUTE.md).

The milestone owns only `27 / EV_SAVEGAME` as a durable future-save route capture. Opcode 27 itself performs no save-file write: it captures `/junction.bsp`-style route data into a caller-owned state that survives source-map teardown.

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

## Hardware-proven fingerprints through SAVEGAME

```text
arenaFNV              = c3882516
mapStateFNV           = cd99b98e
scriptFNV             = f9e3d9df
lineStateFNV          = e5e74861
lineTextureStateFNV   = f1fc1875
automapStateFNV       = 669b1aa7
lineDoorFNV           = b1c9d297
unlockFNV             = 261d756a
giveMapFNV            = 98c7ac59
giveMapMutatedAutoFNV = 9d03ca2d
giveMapMutatedMapFNV  = e21edbce
saveRouteOwnerFNV     = 06ea6ea8
saveRouteResultFNV    = c2ecb064
saveRouteContentFNV   = 725845aa
saveRouteInitialFNV   = 9a00a0bd
saveRouteSampleFNV    = 7e69bd59
legacySaveRouteFNV    = 9bcfe135
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

Hardware-proven value types now include:

```text
EspMapSaveRouteState  = 46 B
EspMapSaveRouteResult = 16 B
persistent heap       = 0 B
```

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
  refs=1 removable=0 stateExecRefused=1
  ownerBytes=46 resultBytes=16 persistentHeapBytes=0
  mapName="/junction.bsp" nameLen=13
  tile=15,29 destination=992,1888 angle=64
  rollback=1/1 reapplyExact=1 ownerSurvivesPackClose=1
```

Canonical SAVEGAME command:

```text
cmd1 event1 off0
arg1=401d0f00 arg2=00000100
mapString=0
name="/junction.bsp"
raw tile=15,29
destination=992,1888
angle=64
handled=1 removeIfHandled=0
```

## EV_SAVEGAME permanent contract

Legacy opcode 27 only captures route data:

```text
mapStringId = arg1 & 0xff
rawX        = (arg1 >> 8) & 0xff
rawY        = (arg1 >> 16) & 0xff
angle       = (arg1 >> 24) & 0xff
destinationX= 32 + (rawX << 6)
destinationY= 32 + (rawY << 6)
handled     = true
```

The map name must survive map teardown. The first zero-copy-ref implementation was corrected before hardware; tested firmware `42497b80...` stores the bounded destination name inline.

Permanent behavior:

```text
one bounded native-PAK string read
no allocation
no ZIP
no save-file write
no map transition
no legacy Game mutation
```

## SAVEGAME hardware proof

Real-CYD corpus:

```text
refs=1 removable=0 ownerBytes=46 resultBytes=16 stateExecRefused=1
mapNameBytes=13 maxMapName=13
ownerFNV=06ea6ea8 resultFNV=c2ecb064 contentFNV=725845aa
initialOwnerFNV=9a00a0bd sampleOwnerFNV=7e69bd59
rollback=1/1 reapplyExact=1
ownerSurvivesPackClose=1 activeAtPark=0
```

Fail closed:

```text
unsupported=1 badOffset=1 badDescriptor=1 nullDescriptor=1
nullEntry=1 nullState=1 nullResult=1 closedPack=1 reset=1
stateAtomic=yes
```

PAK / RAM witness:

```text
heapOpen=63832 transientHeapCost=4376 largestOpen=34804
packIO=yes boundedNameRead=yes persistentHeapBytes=0 saveFileWrite=no
heap8=68208->68208
largest8=34804->34804
frameFNV=99102464->99102464
```

Inherited state stayed exact:

```text
arenaFNV=c3882516
mapStateFNV=cd99b98e
scriptFNV=f9e3d9df
automapStateFNV=669b1aa7
legacy notebook/keys/Hud/password/continuation unchanged
legacy saveRouteFNV=9bcfe135->9bcfe135
legacy runtime clear
entities=0 monsters=0 ST_PLAYING not reached
```

Stable PARK heartbeats:

```text
135324 ms: heap=133972 heap8=68208 largest8=34804 all reported subsystems ready
140327 ms: heap=133972 heap8=68208 largest8=34804 all reported subsystems ready
```

## Remaining MAP_INTRO families

Still unowned:

```text
2  EV_CHANGEMAP
7  EV_SHOW
18 EV_HIDE
```

SHOW/HIDE remain entity-topology coupled. CHANGEMAP is the remaining transition family and must be recovered from the new true main after this branch is merged.

## Merge recommendation

**MERGE `agent/esp32-map1-native-save-route`.**

Hardware-tested firmware content:

```text
42497b80c6158300ec3fa7b8eb8af6cee643f59e
```

All later commits must remain documentation-only unless another firmware is flashed.

After merge, recover the true new `main`, reread this file, `DOCUMENTATION.md`, the merged SAVE route archive and exact legacy behavior before selecting the next family.