# ESP32 native Junction transition preflight milestone

Branch: `agent/esp32-native-transition-preflight`

Base merged `main`:

```text
PR   = #65 — native stats-menu intent
main = c8679133351fa00e01a67103386b7676660c4a6e
```

Hardware-tested corrected firmware:

```text
4d78a66548fab6373c06c67f107f176fc3988b1c
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective and result

Before any destructive `/intro.bsp` -> `/junction.bsp` handoff, this milestone establishes two permanent read-only facilities:

```text
native resource-map catalog
 -> target PAK lookup
 -> complete bounded BSP inventory + CRC
 -> compact caller-owned transition preflight summary
```

It does not reset Entrance, free current native owners, mutate legacy `Game/Menu/Render`, load Junction into the resident runtime, enter `ST_PLAYING`, or render gameplay.

The corrected v2 probe passed on the real classic CYD while preserving Entrance exactly.

## Permanent native map catalog

Recovered original resource order:

```text
1  /intro.bsp
2  /level01.bsp
3  /level02.bsp
4  /level03.bsp
5  /level04.bsp
6  /level05.bsp
7  /level06.bsp
8  /level07.bsp
9  /junction.bsp
10 /junction_destroyed.bsp
11 /items.bsp
12 /reactor.bsp
13 /endgame.bsp
```

Files/API:

```text
ESP32/include/esp_map_catalog.h
ESP32/src/esp_map_catalog.c

EspMapCatalog_isValidId()
EspMapCatalog_nameForId()
EspMapCatalog_idForName()
```

Real-CYD audit:

```text
count       = 13
roundtrip   = 13
catalogFNV  = ce322e3f
invalidName = 1
legacyIds   = 1
junctionName=/junction.bsp
```

## Permanent transition preflight

Files:

```text
ESP32/include/esp_map_transition_preflight.h
ESP32/src/esp_map_transition_preflight.c
```

Hardware-proven ABI:

```text
EspMapTransitionPreflightResult = 56 B
```

Summary fields:

```text
nameHash
entryOffset
sourceBytes
sourceCrc32
sourceFNV1a
persistentPlanBytes
nodes
lines
mapSprites
events
byteCodes
strings
stringDataBytes
targetMapId
gameplayLoadMapId
ready
```

Critical semantic split discovered and then hardware-proven:

```text
targetMapId
  = resource/catalog/lifecycle identity
  = Game.mapFiles[] / DoomCanvas_loadMap() identity

gameplayLoadMapId
  = byte stored in BSP header
  = legacy Render.loadMapID
  = level-progression/stat bookkeeping semantic
```

These IDs are intentionally independent.

Legacy `Player_addLevelStats()` proves that gameplay ID `2` is the hub/no-completion gate:

```text
if (showStats && render->loadMapID != 2) {
    completedLevels |= 1 << (render->loadMapID - 1)
    ...
}
```

Real Junction therefore correctly has:

```text
resourceMapId      = 9
gameplayLoadMapId  = 2
hubProgressionGate = 1
```

The permanent preflight validates only that `gameplayLoadMapId` is safe for the recovered bit semantics (`1..32`). It does not require equality with the resource ID.

For source compatibility with the first diagnostic probe, `headerLoadMapId` remains an ABI-neutral alias of the same one-byte field; it has no separate storage.

## I/O contract

`EspMapTransitionPreflight_run(targetMapId, &result)` requires the PAK closed on entry:

```text
resource map ID
 -> immutable catalog name
 -> open /DoomRPG-ESP32.pak
 -> bounded hash-sorted index lookup on SD
 -> EspBspReader complete streaming inventory
      window = 256 B
      full BSP CRC32
      complete structural traversal
      compact persistent-plan estimate
 -> validate gameplayLoadMapId range
 -> close PAK
 -> return 56 B pointer-free summary
```

If the PAK is already open, status is `PACK_BUSY` and the caller retains ownership of its session.

Statuses:

```text
INVALID
PACK_BUSY
PACK_OPEN_FAILED
ENTRY_NOT_FOUND
BSP_INVALID
GAMEPLAY_ID_INVALID
OK
```

## Diagnostic v1 and corrected v2

First candidate:

```text
b674c9ad4878acdf3d026d061de94f964e2c7d6e
```

It successfully found, streamed and CRC-verified `/junction.bsp`, then failed only because the first model incorrectly required:

```text
BSP loadMapId == resource targetMapId
```

Hardware showed `9 != 2`. This was a model/probe assumption failure, not an I/O or BSP parse failure.

Corrected hardware-tested firmware:

```text
4d78a66548fab6373c06c67f107f176fc3988b1c
```

The corrected final probe explicitly requires the real `resourceMapId=9 / gameplayLoadMapId=2` semantics and ran two complete Junction inventories identically.

## Hardware-proven Junction identity

```text
resourceName      = /junction.bsp
entryOffset       = 1974397
sourceBytes       = 21051
sourceCRC32       = 4a2c5800
sourceFNV1a       = fefaf5ca
BSP mapName       = Junction
gameplayLoadMapId = 2
spawnIndex        = 943
spawnDirection    = 64
cameraSpawnIndex  = 0
floorTexture      = 117
ceilingTexture    = 151
```

Structural inventory:

```text
nodes       = 77
lines       = 207
mapSprites  = 48
events      = 66
byteCodes   = 319
strings     = 126
stringData  = 12235
legacyStringAlloc = 12361
maxString   = 380
structuralEnd = 21051
trailing    = 0
```

Section offsets:

```text
nodes      = 35
lines      = 807
sprites    = 2879
events     = 3121
byteCodes  = 3387
strings    = 6260
blockMap   = 18747
planes     = 19003
end        = 21051
```

Resources:

```text
lineTex         = 22
mapSpriteIds    = 16
textureReq      = 30
spriteReq       = 16
planeTex        = 6
changeSprite    = 0
spriteAsTexture = 0
overflow        = 0/0/0
```

Compact persistent plan:

```text
nodes         = 770 B
lines         = 2070 B
sprites       = 240 B
events        = 264 B
byteCodes     = 2871 B
stringOffsets = 252 B
blockMap      = 256 B
planes        = 2048 B
resourceSets  = 96 B
-------------------
persistent    = 8867 B
```

Streaming proof, reproduced twice:

```text
bytes     = 21051/21051
readCalls = 83
window    = 256 B
FNV1a     = fefaf5ca
CRC32     = 4a2c5800
verified  = yes
repeatExact = 1
```

Preflight result:

```text
resultBytes      = 56
resourceMapId    = 9
gameplayLoadMapId= 2
hubProgressionGate=1
resultFNV        = 108e5c7b
repeatFNV        = 108e5c7b
resourceGameplayDistinct=1
elapsed          = 147 ms
ready            = 1
```

## Fail-closed proof

Hardware:

```text
target0    = 1
target14   = 1
nullResult = 1
packBusy   = 1
busyZero   = 1
stateAtomic=yes
```

The PACK_BUSY test opened the PAK deliberately, verified that preflight refused it without stealing the session, then closed it explicitly.

## RAM and I/O

Hardware:

```text
heap8      65608 -> 65608 delta=0
largest8   34804 -> 34804 delta=0
persistentHeapBytes=0

heapOpen          = 61232
transientPackCost = 4376 B
largestOpen       = 34804
packIO            = yes
fullTargetCRC     = yes
window            = 256 B
packClosed        = yes
```

Hardware-proven persistent native heap remains exactly:

```text
18008 B
```

## Entrance integrity / legacy isolation

Native owners after the target I/O remained exactly:

```text
arenaFNV     = c3882516
mapStateFNV  = cd99b98e
scriptFNV    = f9e3d9df
lineFNV      = e5e74861
textureFNV   = f1fc1875
automapFNV   = 669b1aa7
topologyFNV  = 3f321e43
```

Same-build framebuffer witness:

```text
frameFNV db8a11f6 -> db8a11f6
```

Legacy witnesses:

```text
playerFNV     0b2ae445 -> 0b2ae445
transitionFNV f450c49f -> f450c49f
legacyRuntimeClear=yes
sourceTeardown=no
mapLoad=no
menuMutation=no
mapSwap=no
```

Final PARK:

```text
state=9 page=3
nativeCatalog=yes
nativeTargetPreflight=yes
resourceMapId=9
gameplayLoadMapId=2
targetReady=yes
sourceMapPreserved=yes
packClosed=yes
persistentBytes=0
mapSwap=no
entities=0
monsters=0
noGameplay=yes
```

Stable heartbeats observed after PASS:

```text
1025843 ms heap=131372 heap8=65608 largest8=34804
1030844 ms heap=131372 heap8=65608 largest8=34804
```

## Architecture boundary after PASS

Hardware-proven chain now includes:

```text
CHANGEMAP pending intent
 -> exit stats
 -> player exit-state
 -> stats-menu intent
 -> generic resource-map catalog
 -> target Junction PAK/BSP preflight
```

Still intentionally outside:

```text
actual stats-menu rendering/input consumer
source-map teardown ordering / lifecycle handoff
Junction resident-runtime allocation/swap
Junction mutable-owner rebuild
spawn/loadType handoff
full native entity/monster gameplay
native ST_PLAYING loop/rendering
sound playback
```

The next bounded milestone should own the **source-target lifecycle handoff / reversible swap staging**. It must not call legacy `DoomCanvas_loadMap()` as a shortcut.

## Merge recommendation

```text
MERGE agent/esp32-native-transition-preflight
```

Hardware-tested firmware is `4d78a66548fab6373c06c67f107f176fc3988b1c`. Every commit after that firmware must remain documentation-only until merge.

The real classic CYD Serial log is the final hardware source of truth.
