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
firmware candidate = b674c9ad4878acdf3d026d061de94f964e2c7d6e
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md).

The candidate adds an immutable native 13-map catalog and a read-only Junction PAK/BSP preflight. It performs no Entrance teardown and no map swap.

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

Level-exit stats, player exit-state/result and stats-menu intent are caller-owned values and add no persistent allocation. Current transition-preflight candidate also targets `0 B` persistent addition, keeping the total at `18008 B`.

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
frameFNV             = e8a3b4ef
```

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

Native consumers now hardware-proven through the pause point:

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

No legacy `Player_addLevelStats()`, `Game_changeMap()`, menu mutation or map load is called by these native stages.

## Current native map catalog

Permanent files:

```text
ESP32/include/esp_map_catalog.h
ESP32/src/esp_map_catalog.c
```

Recovered exact legacy map-resource order:

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

API:

```text
EspMapCatalog_isValidId()
EspMapCatalog_nameForId()
EspMapCatalog_idForName()
```

Static catalog-audit prediction:

```text
catalogFNV = ce322e3f
```

Hardware remains authoritative.

## Current transition-preflight API

Permanent files:

```text
ESP32/include/esp_map_transition_preflight.h
ESP32/src/esp_map_transition_preflight.c
```

ABI target:

```text
EspMapTransitionPreflightResult = 56 B
```

`EspMapTransitionPreflight_run(targetMapId, &result)`:

```text
requires PAK closed on entry
resolves ID through native catalog
opens /DoomRPG-ESP32.pak
finds hash-sorted entry on SD
streams complete target BSP through existing 256 B EspBspReader window
verifies full CRC32 and structure
requires BSP header loadMapId == catalog target
returns compact target summary
closes PAK before return
retains no allocation
```

Fail-closed statuses include `INVALID`, `PACK_BUSY`, `PACK_OPEN_FAILED`, `ENTRY_NOT_FOUND`, `BSP_INVALID`, `ID_MISMATCH`, `OK`.

If the pack is already open, preflight returns `PACK_BUSY` without stealing/closing the caller session.

## Candidate Junction proof target

Expected semantic target only:

```text
targetMap=9
name=/junction.bsp
headerLoadMapId=9
resultBytes=56
ready=1
repeat exact
pack closed after each preflight
```

The following Junction values are intentionally **not guessed** before hardware:

```text
nameHash
entry offset
source bytes
CRC32
source FNV1a
compact persistent-plan bytes
nodes/lines/mapSprites/events/byteCodes/strings/stringData
result FNV
elapsed
```

Catalog/failclosed target:

```text
count=13
roundtrip=13/13
catalogFNV=ce322e3f  # static prediction
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
PAK closed at park
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

Branch / firmware:

```text
agent/esp32-native-transition-preflight
b674c9ad4878acdf3d026d061de94f964e2c7d6e
```

Capture `[TRANSITIONPREFLIGHT]`, the Junction `[BSPREAD]` inventory lines, and stable `[ALIVE]` lines.

No CI status is published for the candidate. No local build or hardware PASS is claimed.

## Current architecture boundary

Hardware-proven ownership through PR #65:

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

Candidate adds:

```text
immutable generic 13-map catalog
read-only target PAK/BSP preflight for Junction
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

Do not merge the candidate until the exact firmware above passes on the real CYD and every later commit remains documentation-only.
