# ESP32 native Junction transition preflight milestone

Branch: `agent/esp32-native-transition-preflight`

Base merged `main`:

```text
PR   = #65 — native stats-menu intent
main = c8679133351fa00e01a67103386b7676660c4a6e
```

Corrected firmware candidate:

```text
4d78a66548fab6373c06c67f107f176fc3988b1c
```

Status: **IMPLEMENTED; CORRECTED REAL-CYD VALIDATION PENDING**.

## Objective

Before any destructive `/intro.bsp` -> `/junction.bsp` handoff, the port needs a permanent way to answer:

```text
1. Which BSP resource belongs to a native resource-map ID?
2. Does that target exist in DoomRPG-ESP32.pak?
3. Does the complete target BSP parse and CRC correctly with bounded I/O?
4. What compact runtime budget/structure would the target require?
```

This milestone owns those questions only. It does **not** reset Entrance, free native owners, mutate legacy `Game/Menu/Render`, load Junction into the resident runtime, enter `ST_PLAYING`, or render gameplay.

## Permanent native map catalog

Recovered legacy `Game_init()` / `Game_getResourceMapID()` resource order:

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

Files:

```text
ESP32/include/esp_map_catalog.h
ESP32/src/esp_map_catalog.c
```

API:

```text
EspMapCatalog_isValidId()
EspMapCatalog_nameForId()
EspMapCatalog_idForName()
```

The table is immutable program data. Static audit prediction:

```text
catalogFNV = ce322e3f
```

Hardware remains authoritative.

## Permanent transition preflight

Files:

```text
ESP32/include/esp_map_transition_preflight.h
ESP32/src/esp_map_transition_preflight.c
```

Caller-owned ABI remains:

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

`targetMapId` and `gameplayLoadMapId` are deliberately different concepts:

```text
targetMapId
  = resource/lifecycle ID
  = Game.mapFiles[] / DoomCanvas_loadMap() identity

gameplayLoadMapId
  = byte stored in the BSP header
  = legacy Render.loadMapID
  = progression/stat bookkeeping semantic
```

The permanent API **does not require them to match**.

The legacy proof is explicit in `Player_addLevelStats()`:

```text
if (showStats && render->loadMapID != 2) {
    completedLevels |= 1 << (render->loadMapID - 1)
    ... secret/monster completion bits ...
}
```

Therefore BSP value `2` is the hub / no-completion progression gate, not resource map ID 2.

For source compatibility with the first temporary diagnostic probe, `headerLoadMapId` remains an ABI-neutral alias of the same one-byte `gameplayLoadMapId` field. It has no separate storage.

### I/O contract

`EspMapTransitionPreflight_run(targetMapId, &result)` requires the PAK closed on entry and performs:

```text
resource map ID
 -> immutable native catalog name
 -> open /DoomRPG-ESP32.pak
 -> bounded hash-sorted index lookup on SD
 -> EspBspReader complete streaming inventory
      256 B window
      full BSP CRC32
      complete structural traversal
      compact persistent-plan estimate
 -> validate gameplayLoadMapId is safely representable for legacy bit semantics
      1..32
 -> close PAK
 -> return 56 B pointer-free summary
```

If the PAK is already open, status is `PACK_BUSY`; the caller's session is neither stolen nor closed.

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

## First real-CYD diagnostic — candidate v1

First firmware candidate:

```text
b674c9ad4878acdf3d026d061de94f964e2c7d6e
```

It successfully found and fully inventoried `/junction.bsp`, then intentionally failed at the original incorrect assertion:

```text
inventory.loadMapId == targetMapId
```

Hardware showed:

```text
resource target map = 9 / MAP_JUNCTION
BSP header loadMapId = 2
```

The first candidate therefore printed:

```text
[TRANSITIONPREFLIGHT] FAILED Junction preflight
```

This was a **model/probe assumption failure**, not a PAK/BSP parse failure.

### Junction facts discovered by that run

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
maxString   = 380
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

Compact plan:

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

Streaming proof:

```text
bytes     = 21051/21051
readCalls = 83
window    = 256 B
FNV1a     = fefaf5ca
CRC32     = 4a2c5800
verified  = yes
```

These are real-CYD observations from candidate v1. They become final canons only when the corrected candidate reproduces them and completes the full acceptance proof.

## Corrected final probe v2

Files:

```text
ESP32/include/native_transition_preflight_final_probe.h
ESP32/src/native_transition_preflight_final_probe.c
```

The old first diagnostic probe remains compiled as history but is no longer serviced by the lifecycle bridge.

The corrected probe requires:

```text
resourceMapId      = 9
gameplayLoadMapId  = 2
hubProgressionGate = 1
```

and reproduces exactly the discovered Junction payload/structure above twice.

It also requires:

```text
catalog count=13
roundtrip=13/13
catalogFNV=ce322e3f
unknown name fail-closed

target0=1
target14=1
nullResult=1
packBusy=1
busyZero=1
stateAtomic=yes
```

## Source-map integrity boundary

Before/after target I/O, Entrance must remain exact:

```text
arenaFNV     = c3882516
mapStateFNV  = cd99b98e
scriptFNV    = f9e3d9df
lineFNV      = e5e74861
textureFNV   = f1fc1875
automapFNV   = 669b1aa7
topologyFNV  = 3f321e43
```

Plus same-build equality for framebuffer, legacy Player witness and legacy transition/menu witness.

Required final boundary:

```text
shapeData=NULL
mediaTexels=NULL
legacy Render runtime clear
entities=0
monsters=0
sourceTeardown=no
mapLoad=no
menuMutation=no
mapSwap=no
PAK closed at PARK
```

## RAM target

Hardware-proven persistent native heap entering the milestone:

```text
18008 B
```

Catalog is immutable program data and result is caller-owned:

```text
persistentHeapBytes = 0
persistent total remains 18008 B
heap8 before == after
largest8 before == after
```

PAK/File allocation is transient and printed separately.

## Corrected expected Serial family

```text
[TRANSITIONPREFLIGHTFINAL] ARMED ...

=== Doom RPG ESP32-native Junction transition preflight v2 ===
[TRANSITIONPREFLIGHTFINAL] CONTRACT ...

[BSPREAD] ... /junction.bsp ...   # first complete inventory
[BSPREAD] ... /junction.bsp ...   # repeat complete inventory

[TRANSITIONPREFLIGHT] READY resultBytes=56 resourceMapId=9 gameplayLoadMapId=2 hubProgressionGate=1 entryOffset=1974397 size=21051 crc32=4a2c5800 fnv1a=fefaf5ca planBytes=8867 resultFNV=... elapsed=... ready=1
[TRANSITIONPREFLIGHT] STRUCT nodes=77 lines=207 mapSprites=48 events=66 byteCodes=319 strings=126 stringData=12235
[TRANSITIONPREFLIGHT] CATALOG count=13 roundtrip=13 catalogFNV=ce322e3f invalidName=1 legacyIds=1 junctionName=/junction.bsp
[TRANSITIONPREFLIGHT] REPEAT exact=1 firstFNV=... repeatFNV=... resourceGameplayDistinct=1
[TRANSITIONPREFLIGHT] FAILCLOSED target0=1 target14=1 nullResult=1 packBusy=1 busyZero=1 stateAtomic=yes
[TRANSITIONPREFLIGHT] IO ... packClosed=yes ...
[TRANSITIONPREFLIGHT] RAM ... persistentHeapBytes=0 ...
[TRANSITIONPREFLIGHT] LEGACY ... sourceTeardown=no mapLoad=no menuMutation=no mapSwap=no
[TRANSITIONPREFLIGHT] PARK ... resourceMapId=9 gameplayLoadMapId=2 targetReady=yes sourceMapPreserved=yes packClosed=yes persistentBytes=0 mapSwap=no entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

Use normal PlatformIO environment `esp32-cyd`.

No CI status is published for corrected candidate `4d78a66548fab6373c06c67f107f176fc3988b1c`. No local build or corrected hardware PASS is claimed.

## Boundary after PASS

A corrected PASS will prove Junction readable and structurally valid while Entrance remains resident:

```text
CHANGEMAP pending intent       [hardware-proven]
 -> exit stats                [hardware-proven]
 -> player exit-state         [hardware-proven]
 -> stats-menu intent         [hardware-proven]
 -> map catalog               [corrected candidate]
 -> target PAK/BSP preflight  [corrected candidate]
```

Still outside:

```text
actual stats-menu rendering/input consumer
source-map teardown ordering
allocation/swap of Junction resident runtime
rebuild of Junction mutable owners
spawn placement / loadType handoff
full native entity/monster gameplay
native ST_PLAYING loop/rendering
sound playback
```

The next bounded milestone after PASS should design/prove the source-target lifecycle handoff / reversible swap staging, not call legacy `DoomCanvas_loadMap()`.

The real classic CYD Serial log remains the final hardware source of truth.
