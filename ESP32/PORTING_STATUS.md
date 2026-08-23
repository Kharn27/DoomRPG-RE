# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #65 — native stats-menu intent
main = c8679133351fa00e01a67103386b7676660c4a6e
hardware-tested stats-menu firmware = 1dddbe86788389400d6e2186595174e723c72f5c
```

Merged evidence: [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md).

## Current candidate

```text
branch = agent/esp32-native-transition-preflight
base   = c8679133351fa00e01a67103386b7676660c4a6e
corrected firmware candidate = 4d78a66548fab6373c06c67f107f176fc3988b1c
status = IMPLEMENTED; CORRECTED REAL-CYD VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md).

First candidate `b674c9ad4878acdf3d026d061de94f964e2c7d6e` produced a useful real-CYD diagnostic failure: `/junction.bsp` parsed and CRC-verified completely, but its BSP header `loadMapId` is `2`, not resource map ID `9`. The model has been corrected accordingly.

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

Level-exit stats, player exit-state/result, stats-menu intent, map catalog and transition-preflight result add no persistent heap allocation. Current target remains exactly `18008 B`.

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
```

Latest stats-menu same-build witnesses:

```text
playerExitWitnessFNV = 0b2ae445
transitionFNV        = f450c49f
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

Native consumers hardware-proven through the pause point:

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
```

No legacy `Player_addLevelStats()`, `Game_changeMap()`, menu mutation or map load is called by those native stages.

## Native map catalog

Permanent API:

```text
ESP32/include/esp_map_catalog.h
ESP32/src/esp_map_catalog.c

EspMapCatalog_isValidId()
EspMapCatalog_nameForId()
EspMapCatalog_idForName()
```

Recovered exact resource-map order:

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

Static catalog audit:

```text
catalogFNV = ce322e3f
```

Corrected hardware validation remains pending.

## Transition-preflight semantics

Permanent API:

```text
ESP32/include/esp_map_transition_preflight.h
ESP32/src/esp_map_transition_preflight.c

EspMapTransitionPreflightResult = 56 B
EspMapTransitionPreflight_run(targetMapId, &result)
```

Critical semantic split discovered on real hardware:

```text
targetMapId
  = resource/lifecycle identity
  = catalog / Game.mapFiles[] / DoomCanvas_loadMap()

gameplayLoadMapId
  = BSP header Render.loadMapID byte
  = level-progression/stat semantic
```

Legacy `Player_addLevelStats()` proves that gameplay ID `2` is the hub/no-completion gate:

```text
if (showStats && render->loadMapID != 2) {
    completedLevels |= 1 << (render->loadMapID - 1)
    ...
}
```

Therefore real Junction is expected to be:

```text
resourceMapId      = 9
gameplayLoadMapId  = 2
resource/gameplay IDs intentionally distinct
```

The permanent preflight now validates `gameplayLoadMapId` only for safe progression-bit semantics (`1..32`); it does not require equality with resource ID.

## Real-CYD Junction diagnostic facts from candidate v1

First candidate:

```text
b674c9ad4878acdf3d026d061de94f964e2c7d6e
```

Hardware successfully streamed the complete target before the obsolete ID-equality check failed:

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

Structure:

```text
nodes=77
lines=207
mapSprites=48
events=66
byteCodes=319
strings=126
stringData=12235
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

Stream:

```text
bytes=21051/21051
readCalls=83
window=256 B
FNV1a=fefaf5ca
CRC32=4a2c5800
verified=yes
```

These are diagnostic hardware observations until the corrected final probe reproduces them and completes all rollback/failclosed checks.

## Corrected final transition-preflight probe

Corrected code SHA:

```text
4d78a66548fab6373c06c67f107f176fc3988b1c
```

The lifecycle now services only:

```text
Esp32TransitionPreflightFinalProbe_*
```

The original failing probe remains compiled as history but is not run.

Corrected acceptance requires:

```text
resultBytes=56
resourceMapId=9
gameplayLoadMapId=2
hubProgressionGate=1
entryOffset=1974397
sourceBytes=21051
crc32=4a2c5800
fnv1a=fefaf5ca
persistentPlanBytes=8867
nodes=77 lines=207 mapSprites=48 events=66 byteCodes=319 strings=126 stringData=12235
repeat exact
catalog count=13 roundtrip=13 catalogFNV=ce322e3f
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

RAM/integrity target:

```text
persistentHeapBytes=0
heap8 before == after
largest8 before == after
persistent native total remains 18008 B
PAK closed at PARK
all Entrance owner FNVs unchanged
framebuffer same-build equality
legacy Player/transition witnesses unchanged
sourceTeardown=no
mapLoad=no
menuMutation=no
mapSwap=no
entities=0 monsters=0
```

## Validation

Build/flash normal environment:

```text
esp32-cyd
```

Branch / corrected firmware:

```text
agent/esp32-native-transition-preflight
4d78a66548fab6373c06c67f107f176fc3988b1c
```

Capture `[TRANSITIONPREFLIGHTFINAL]`, both complete Junction `[BSPREAD]` inventories, `[TRANSITIONPREFLIGHT]` final lines, and stable `[ALIVE]` lines.

No CI status is published. No local build or corrected hardware PASS is claimed.

## Current architecture boundary

Hardware-proven through PR #65:

```text
compact immutable native Entrance map + explicit mutable owners
all 16 real MAP_INTRO opcode families
SAVEGAME durable route
CHANGEMAP pending transition intent
SHOW/HIDE compact sprite/entity topology
native level-exit stats
native player exit-state
native stats-menu semantic intent
```

Corrected candidate adds:

```text
immutable generic 13-map resource catalog
read-only target PAK/BSP preflight
explicit resourceMapId vs gameplayLoadMapId semantics
```

Still outside:

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

Do not merge until corrected firmware `4d78a66548fab6373c06c67f107f176fc3988b1c` passes on the real CYD and every later commit remains documentation-only.
