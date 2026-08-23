# ESP32 native Junction transition preflight milestone

Branch: `agent/esp32-native-transition-preflight`

Base merged `main`:

```text
PR   = #65 — native stats-menu intent
main = c8679133351fa00e01a67103386b7676660c4a6e
```

Firmware candidate:

```text
b674c9ad4878acdf3d026d061de94f964e2c7d6e
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

The real MAP_INTRO exit chain is hardware-proven through the stats-menu pause intent. Before any destructive `/intro.bsp` -> `/junction.bsp` handoff, the port now needs a permanent way to answer two questions without touching the current map:

```text
1. Which original BSP resource belongs to a native map ID?
2. Does that target exist in DoomRPG-ESP32.pak and parse/CRC cleanly with the native BSP reader?
```

This milestone owns those questions only. It does **not** reset Entrance, free any native owner, mutate legacy `Game/Menu/Render`, load Junction into the resident runtime, enter `ST_PLAYING`, or render gameplay.

## Recovered map catalog

Legacy `Game_init()` and `Game_getResourceMapID()` define the exact 13-map resource order:

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

Permanent files:

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

The table is immutable program data. Returned names are static literals; there is no heap-owned string or pointer-heavy runtime object.

Temporary probe computes a deterministic catalog fingerprint over `(id, NUL-terminated resource name)` pairs. Static prediction:

```text
catalogFNV = ce322e3f
```

Hardware remains authoritative.

## Permanent target preflight

Files:

```text
ESP32/include/esp_map_transition_preflight.h
ESP32/src/esp_map_transition_preflight.c
```

Caller-owned ABI:

```text
EspMapTransitionPreflightResult = 56 B
```

Fields summarize only immutable target facts:

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
headerLoadMapId
ready
```

API:

```text
EspMapTransitionPreflight_run(targetMapId, &result)
```

Status values:

```text
INVALID
PACK_BUSY
PACK_OPEN_FAILED
ENTRY_NOT_FOUND
BSP_INVALID
ID_MISMATCH
OK
```

### I/O contract

The pack must be closed on entry. The preflight:

```text
map ID
 -> immutable native catalog name
 -> open /DoomRPG-ESP32.pak
 -> hash-sorted SD index lookup
 -> existing EspBspReader full streaming inventory
      window = 256 B
      complete BSP CRC32 verification
      structural counts/offset traversal
      compact persistent-plan estimate
 -> verify BSP header loadMapId matches catalog target
 -> close PAK
 -> return 56 B summary
```

`EspBspReader_inventoryPackEntry()` retains no map-wide payload; its cursor uses the existing 256 B bounded window. The preflight retains no allocation after return.

If the pack is already open, the function returns `PACK_BUSY` and does not steal or close the caller's session.

## Real Junction proof target

The real MAP_INTRO CHANGEMAP already proved:

```text
targetMap = 9 / MAP_JUNCTION
name      = /junction.bsp
spawnParam=0
showStats =1
```

The new probe runs after the hardware-proven stats-menu intent and performs two identical preflights for target 9. It requires:

```text
status=OK
resultBytes=56
targetMap=9
name=/junction.bsp
headerLoadMapId=9
ready=1
repeat exact
PAK closed after each run
```

The real CYD will establish new Junction canons for:

```text
nameHash
PAK entry offset
source bytes
CRC32
source FNV1a
compact persistent-plan bytes
nodes
lines
mapSprites
events
byteCodes
strings
stringDataBytes
preflight result FNV
elapsed time
```

No values are guessed in advance.

## Catalog / fail-closed probe

The temporary probe also requires:

```text
catalog count=13
roundtrip=13/13
catalogFNV=ce322e3f      # static prediction
unknown name -> failure + mapId=0
id 0 -> no resource
id 14 -> no resource
legacy MAP_JUNCTION == native 9
legacy MAP_END_GAME == native 13
```

Preflight fail-closed requirements:

```text
target0=1
target14=1
nullResult=1
packBusy=1
busyZero=1
stateAtomic=yes
```

## Source-map integrity boundary

The probe snapshots Entrance before any target I/O and requires exact equality afterward:

```text
arenaFNV          c3882516
mapStateFNV       cd99b98e
scriptFNV         f9e3d9df
lineFNV           e5e74861
textureFNV        f1fc1875
automapFNV        669b1aa7
topologyFNV       3f321e43
framebuffer same-build equality
legacy Player witness equality
legacy transition/menu witness equality
```

Also required:

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
```

## RAM target

Hardware-proven persistent native heap entering the milestone:

```text
18008 B
```

The catalog is immutable program data and the preflight result is caller-owned. Pack/File activity is transient only.

Acceptance:

```text
persistentHeapBytes=0
heap8 before == after
largest8 before == after
persistent native total remains 18008 B
```

The probe separately records the transient heap cost while a PAK session is open; that number is diagnostic and not persistent ownership.

## Expected Serial family

```text
[TRANSITIONPREFLIGHT] ARMED ...

=== Doom RPG ESP32-native Junction transition preflight ===
[TRANSITIONPREFLIGHT] CONTRACT ...
[BSPREAD] ENTRY /junction.bsp ...
[BSPREAD] HEADER ...
[BSPREAD] INVENTORY ...
[BSPREAD] PLAN ...
[BSPREAD] STREAM ... verified=yes
# repeated once for determinism

[TRANSITIONPREFLIGHT] READY resultBytes=56 targetMap=9 name="/junction.bsp" headerLoadMapId=9 nameHash=... entryOffset=... bytes=... crc32=... fnv=... plan=... nodes=... lines=... sprites=... events=... byteCodes=... strings=... stringData=... resultFNV=... elapsed=...ms
[TRANSITIONPREFLIGHT] CATALOG count=13 roundtrip=13 catalogFNV=ce322e3f invalidName=1 legacyIds=1
[TRANSITIONPREFLIGHT] REPEAT resultFNV=... repeatFNV=... exact=1
[TRANSITIONPREFLIGHT] FAILCLOSED target0=1 target14=1 nullResult=1 packBusy=1 busyZero=1 stateAtomic=yes
[TRANSITIONPREFLIGHT] IO packIO=yes inventoryWindow=256 ... packClosed=yes repeatInventory=yes
[TRANSITIONPREFLIGHT] RAM ... persistentHeapBytes=0 ... all source FNVs unchanged
[TRANSITIONPREFLIGHT] LEGACY ... sourceTeardown=no mapLoad=no menuMutation=no
[TRANSITIONPREFLIGHT] PARK ... nativeCatalog=yes nativeTargetPreflight=yes targetMap=9 targetReady=yes sourceMapPreserved=yes packClosed=yes persistentBytes=0 mapSwap=no entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

Use normal PlatformIO environment `esp32-cyd`.

No CI status is published for the candidate. No local build or hardware PASS is claimed.

## Boundary after PASS

A PASS means Junction is proven readable and structurally valid **while Entrance remains resident**:

```text
CHANGEMAP pending intent       [hardware-proven]
 -> exit stats                [hardware-proven]
 -> player exit-state         [hardware-proven]
 -> stats-menu intent         [hardware-proven]
 -> map catalog               [candidate]
 -> target PAK/BSP preflight  [candidate]
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

The next bounded milestone after hardware PASS should design/prove the **source-target lifecycle handoff / reversible swap staging**. It should not jump directly to a legacy `DoomCanvas_loadMap()` call.

The real classic CYD Serial log remains the final hardware source of truth.
