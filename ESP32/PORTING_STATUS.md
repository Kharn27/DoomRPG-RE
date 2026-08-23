# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #65 — native stats-menu intent
main = c8679133351fa00e01a67103386b7676660c4a6e
hardware-tested stats-menu firmware = 1dddbe86788389400d6e2186595174e723c72f5c
```

Merged evidence: [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-transition-preflight
base   = c8679133351fa00e01a67103386b7676660c4a6e
hardware-tested firmware = 4d78a66548fab6373c06c67f107f176fc3988b1c
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Active evidence: [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md).

Candidate v1 `b674c9ad4878acdf3d026d061de94f964e2c7d6e` exposed a useful model error: Junction resource ID is `9` while its BSP gameplay `loadMapId` is `2`. Corrected v2 separated those semantics and passed completely on the real CYD.

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
entities    = 0
monsters    = 0
ST_PLAYING  = not reached
```

## MAP_INTRO identity

```text
/intro.bsp / Entrance
bytes=21823 crc32=623f34e4 loadMapId=1
nodes=223 lines=480 mapSprites=344 events=93 byteCodes=265
strings=94 stringData=7779 maxString=313
```

All real MAP_INTRO opcode IDs have native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Hardware-proven persistent native RAM

```text
immutable arena          14112 B
mutable tile state        1040 B
mutable script state       100 B
mutable line state         136 B
mutable texture state       76 B
mutable automap state      120 B
mutable sprite topology   2424 B
-------------------------------
total                    18008 B
```

Level-exit stats, player exit-state/result, stats-menu intent, map catalog and transition-preflight result are caller-owned or immutable program data and add no persistent heap allocation. Hardware total remains exactly `18008 B`.

## Hardware-proven fingerprints

```text
arenaFNV                 = c3882516
mapStateFNV              = cd99b98e
scriptFNV                = f9e3d9df
lineStateFNV             = e5e74861
lineTextureStateFNV      = f1fc1875
automapStateFNV          = 669b1aa7
saveRouteOwnerFNV        = 06ea6ea8
saveRouteResultFNV       = c2ecb064
changeMapOwnerFNV        = f75eb7c7
changeMapResultFNV       = 2f40c9be
spriteTopologyFNV        = 3f321e43
showResultFNV            = 6029eb3c
hideResultFNV            = d24f5bae
contextAfterShowFNV      = 2de723aa
contextAfterHideFNV      = bb1d78a4
legacyEntityTopologyFNV  = f8f9b485
levelExitStatsFNV        = bd41bcfa
levelExitNoStatsFNV      = d9532169
levelExitMapId2FNV       = ceb6ad21
levelExitShowSensFNV     = 5155b517
levelExitSecretOpenFNV   = 6694b0e1
playerExitInitialFNV     = 940b0171
playerExitAppliedFNV     = 298eaaa4
playerExitResultFNV      = 5d10a566
playerExitAllMasksFNV    = c93e8128
playerExitLiveFNV        = 57fce418
statsMenuIntentFNV       = 96afe901
statsMenuEndGameFNV      = deea91b4
statsMenuZeroFNV         = 4b95f515
catalogFNV               = ce322e3f
transitionPreflightFNV   = 108e5c7b
junctionSourceFNV        = fefaf5ca
```

Latest same-build legacy witnesses:

```text
playerWitnessFNV = 0b2ae445
transitionFNV    = f450c49f
```

Framebuffer FNV is only a same-build equality witness and is not a cross-build canon.

## Hardware-proven CHANGEMAP exit chain

Real MAP_INTRO command:

```text
name=/junction.bsp
targetMap=9 / MAP_JUNCTION
spawnParam=0
showStats=1
effects=03
pending=1
```

Native consumer chain now hardware-proven through target preflight:

```text
CHANGEMAP pending intent
 -> EspMapLevelExitStats = 20 B
      loadMapId=1 showStats=1
      secrets=0/6 monsters=0/30
      completionLevelBit=00000001
      effects=1f
      FNV=bd41bcfa
 -> EspPlayerExitState = 28 B
      appliedFNV=298eaaa4
      persistentHeapBytes=0
 -> EspStatsMenuIntent = 4 B
      targetMap=9
      menuKind=LEVEL
      consumePending=1
      FNV=96afe901
 -> immutable 13-map catalog
      catalogFNV=ce322e3f
 -> EspMapTransitionPreflightResult = 56 B
      resourceMapId=9
      gameplayLoadMapId=2
      FNV=108e5c7b
      persistentHeapBytes=0
```

No legacy `Player_addLevelStats()`, `Game_changeMap()`, menu mutation, source teardown or map swap is performed by this chain.

## Native resource-map catalog

Permanent files/API:

```text
ESP32/include/esp_map_catalog.h
ESP32/src/esp_map_catalog.c

EspMapCatalog_isValidId()
EspMapCatalog_nameForId()
EspMapCatalog_idForName()
```

Recovered exact resource order:

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

Real-CYD proof:

```text
count=13
roundtrip=13
catalogFNV=ce322e3f
invalidName=1
legacyIds=1
```

## Transition-preflight semantics

Permanent API:

```text
ESP32/include/esp_map_transition_preflight.h
ESP32/src/esp_map_transition_preflight.c

EspMapTransitionPreflightResult = 56 B
EspMapTransitionPreflight_run(targetMapId, &result)
```

Critical semantic split:

```text
targetMapId
  = resource/lifecycle identity
  = catalog / Game.mapFiles[] / DoomCanvas_loadMap()

gameplayLoadMapId
  = BSP header Render.loadMapID byte
  = level-progression/stat semantic
```

Legacy `Player_addLevelStats()` establishes gameplay ID `2` as the hub/no-completion gate. Real Junction is therefore correctly:

```text
resourceMapId      = 9
gameplayLoadMapId  = 2
hubProgressionGate = 1
```

The permanent preflight validates `gameplayLoadMapId` only for the safe recovered bit-semantic range `1..32`; resource/gameplay equality is not required.

## Hardware-proven Junction target

```text
resourceName      = /junction.bsp
entryOffset       = 1974397
sourceBytes       = 21051
sourceCRC32       = 4a2c5800
sourceFNV1a       = fefaf5ca
mapName           = Junction
gameplayLoadMapId = 2
spawnIndex        = 943
spawnDirection    = 64
cameraSpawnIndex  = 0
floorTexture      = 117
ceilingTexture    = 151
```

Structure:

```text
nodes=77
lines=207
mapSprites=48
events=66
byteCodes=319
strings=126
stringData=12235
legacyStringAlloc=12361
maxString=380
trailing=0
```

Offsets:

```text
nodes=35
lines=807
sprites=2879
events=3121
byteCodes=3387
strings=6260
blockMap=18747
planes=19003
end=21051
```

Resources:

```text
lineTex=22
mapSpriteIds=16
textureReq=30
spriteReq=16
planeTex=6
changeSprite=0
spriteAsTexture=0
overflow=0/0/0
```

Compact plan:

```text
nodes=770
lines=2070
sprites=240
events=264
byteCodes=2871
stringOffsets=252
blockMap=256
planes=2048
resourceSets=96
persistentPlanBytes=8867
```

Stream and preflight:

```text
readCalls=83
window=256 B
CRC32=4a2c5800 verified=yes
FNV1a=fefaf5ca
resultFNV=108e5c7b
repeatFNV=108e5c7b
repeatExact=1
resourceGameplayDistinct=1
elapsed=147 ms
ready=1
```

## Fail closed / I/O / RAM

Hardware:

```text
target0=1
target14=1
nullResult=1
packBusy=1
busyZero=1
stateAtomic=yes
```

I/O/RAM:

```text
heap8      65608 -> 65608 delta=0
largest8   34804 -> 34804 delta=0
persistentHeapBytes=0
heapOpen=61232
transientPackCost=4376 B
largestOpen=34804
packClosed=yes
fullTargetCRC=yes
```

Persistent native total remains exactly `18008 B`.

## Entrance / legacy integrity

```text
arenaFNV     c3882516
mapStateFNV  cd99b98e
scriptFNV    f9e3d9df
lineFNV      e5e74861
textureFNV   f1fc1875
automapFNV   669b1aa7
topologyFNV  3f321e43

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
entities=0 monsters=0 noGameplay=yes
```

Stable post-PASS heartbeats:

```text
1025843 ms heap=131372 heap8=65608 largest8=34804
1030844 ms heap=131372 heap8=65608 largest8=34804
```

## Current architecture boundary

Hardware-proven ownership now includes:

```text
compact immutable native Entrance map + explicit mutable owners
all 16 real MAP_INTRO opcode families
SAVEGAME durable route
CHANGEMAP pending transition intent
SHOW/HIDE compact sprite/entity topology
native level-exit stats
native player exit-state
native stats-menu semantic intent
immutable generic 13-map resource catalog
read-only target Junction PAK/BSP preflight
explicit resourceMapId vs gameplayLoadMapId semantics
```

Still intentionally outside:

```text
actual stats-menu rendering/input consumer
source-target lifecycle handoff / source teardown ordering
Junction resident-runtime allocation/swap
Junction mutable-owner rebuild
spawn/loadType handoff
full native entity/monster gameplay
native ST_PLAYING progression/rendering
sound playback
```

The next bounded milestone should own **source-target lifecycle handoff / reversible swap staging** before any real Junction resident-runtime replacement.

## Merge recommendation

```text
MERGE agent/esp32-native-transition-preflight
```

Hardware-tested firmware is `4d78a66548fab6373c06c67f107f176fc3988b1c`. All commits after that firmware must remain documentation-only until merge.
