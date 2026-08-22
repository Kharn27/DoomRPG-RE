# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #59 — native EV_GIVEMAP automap state
main = 9891a25d700f9ffe1be044ac4a7629c3487604ec
hardware-tested firmware = 2e0f8f5de93f806380ee254a8dab59a817c73f5d
```

Merged evidence: [`MAP1_NATIVE_GIVEMAP_STATE.md`](MAP1_NATIVE_GIVEMAP_STATE.md).

## Current candidate

```text
branch = agent/esp32-map1-native-save-route
base   = 9891a25d700f9ffe1be044ac4a7629c3487604ec
firmware candidate = 42497b80c6158300ec3fa7b8eb8af6cee643f59e
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_SAVE_ROUTE.md`](MAP1_NATIVE_SAVE_ROUTE.md).

The candidate owns only `27 / EV_SAVEGAME` as a durable future-save route capture. Opcode 27 itself does not serialize a file: it captures one map name plus destination x/y and angle. Native code copies only that <=31-byte map name from `/DoomRPG-ESP32.pak` into a caller-owned route state so it survives source-map teardown.

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

## Hardware-proven fingerprints through PR #59

```text
arenaFNV             = c3882516
mapStateFNV          = cd99b98e
scriptFNV            = f9e3d9df
lineStateFNV         = e5e74861
lineTextureStateFNV  = f1fc1875
automapStateFNV      = 669b1aa7
lineDoorFNV          = b1c9d297
unlockFNV            = 261d756a
giveMapFNV           = 98c7ac59
giveMapMutatedAutoFNV= 9d03ca2d
giveMapMutatedMapFNV = e21edbce
```

Current candidate will establish:

```text
saveRouteOwnerFNV   = pending
saveRouteResultFNV  = pending
saveRouteContentFNV = pending
initialOwnerFNV     = pending
sampleOwnerFNV      = pending
legacySaveRouteFNV  = pending witness
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

Candidate value types:

```text
EspMapSaveRouteState  = 46 B expected
EspMapSaveRouteResult = 16 B expected
persistent heap       = 0 B expected
```

The bounded PAK open/read is transient; heap and largest free block must return exactly after close.

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
```

## EV_SAVEGAME recovered contract

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

The map name must survive map teardown. The first zero-copy-ref implementation was corrected before hardware: candidate `42497b80...` stores the bounded destination name inline, matching the legacy lifetime without retaining the desktop engine.

Permanent behavior:

```text
one bounded native-PAK string read
no allocation
no ZIP
no save-file write
no map transition
no legacy Game mutation
```

## Real-CYD validation target

Hardware must discover:

```text
SAVEGAME refs / removable refs
map-name total bytes / max length
first real route name + x/y + angle
ownerFNV / resultFNV / contentFNV
initial/sample owner FNV
transient pack-open heap cost
legacy save-route witness FNV
```

Acceptance:

```text
refs > 0
stateExecRefused = refs
rollback = refs/refs
ownerBytes=46 resultBytes=16
reapplyExact=1
ownerSurvivesPackClose=1
closedPack=1
activeAtPark=0
persistentHeapBytes=0
saveFileWrite=no
```

Fail closed:

```text
unsupported=1 badOffset=1 badDescriptor=1
nullDescriptor=1 nullEntry=1 nullState=1 nullResult=1
closedPack=1 reset=1 stateAtomic=yes
```

Protected inherited state:

```text
arenaFNV=c3882516
mapStateFNV=cd99b98e
scriptFNV=f9e3d9df
lineStateFNV=e5e74861
lineTextureStateFNV=f1fc1875
automapStateFNV=669b1aa7
legacy runtime clear
entities=0 monsters=0 ST_PLAYING not reached
```

The probe also hashes the legacy fields that opcode 27 would otherwise touch: `Game.newMapName[32]`, `newDestX`, `newDestY`, `newAngle`; that witness must remain unchanged.

## Remaining MAP_INTRO families after candidate PASS

```text
2  EV_CHANGEMAP
7  EV_SHOW
18 EV_HIDE
```

SHOW/HIDE remain entity-topology coupled. Do not authorize the next family before hardware PASS + merge recovery.

## Validation target

Build/flash normal optimized `esp32-cyd` from `agent/esp32-map1-native-save-route` and capture `[MAPSAVEROUTE]`, `[MAPSAVEROUTEPROBE]` plus a stable `[ALIVE]`.

No CI status is published for candidate `42497b80c6158300ec3fa7b8eb8af6cee643f59e`. No local build or hardware PASS is claimed.
