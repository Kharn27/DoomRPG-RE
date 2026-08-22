# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #62 — native SHOW/HIDE sprite topology
main = ed5cd9a09c9ae36f999661f4284f64400681b1af
hardware-tested firmware = f881ccdad20d950462dd781456c340e792f59ec3
```

Merged evidence: [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md).

All 16 real MAP_INTRO opcode IDs now have explicit native ownership/execution boundaries.

## Current candidate

```text
branch = agent/esp32-map1-native-level-exit-stats
base   = ed5cd9a09c9ae36f999661f4284f64400681b1af
firmware candidate = f9a05933a00fab26b1c0e2b15375d074161ef2bc
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md).

This is the first post-event-family consumer milestone. It computes the map-derived part of legacy `Player_addLevelStats()` without mutating legacy Player/Menu/Game/Render state.

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
loadMapId=1
nodes=223 lines=480 mapSprites=344 events=93 byteCodes=265
strings=94 stringData=7779 maxString=313
```

All owned real opcode IDs:

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

Current candidate adds no persistent allocation. Target total remains `18008 B`.

## Hardware-proven fingerprints

```text
arenaFNV                = c3882516
mapStateFNV             = cd99b98e
scriptFNV               = f9e3d9df
lineStateFNV            = e5e74861
lineTextureStateFNV     = f1fc1875
automapStateFNV         = 669b1aa7
lineDoorFNV             = b1c9d297
unlockFNV               = 261d756a
giveMapFNV              = 98c7ac59
saveRouteOwnerFNV       = 06ea6ea8
saveRouteResultFNV      = c2ecb064
changeMapOwnerFNV       = f75eb7c7
changeMapResultFNV      = 2f40c9be
legacyTransitionFNV     = 79ab740c
playerStatsFNV          = 0b2ae445
spriteTopologyFNV       = 3f321e43
showResultFNV           = 6029eb3c
hideResultFNV           = d24f5bae
showStateFNV            = b6a45f47
hideStateFNV            = bec68187
contextAfterShowFNV     = 2de723aa
contextAfterHideFNV     = bb1d78a4
legacyEntityTopologyFNV = f8f9b485
```

## SHOW/HIDE merged canon

```text
sprites=344 storageBytes=2408 actualHeap=2424 overhead=16
entityDefCount=115 entities=220 hasDef=213 fallback=7
linked=209 hiddenSprites=11 hiddenEntities=11
enemies=30 destructibles=13 nextOrder=209

refs=12 show=11 hide=1 removable=12
showMutated=11 hideMutatedIsolated=0 hideNoMutation=1
blockersFound=2 blockersRemoved=2 blockerNoops=0 deferredDeaths=2
rollback=12/12 showRepeatGuard=1 hideContext=1 hideIdempotent=1 reset=1
```

Final PARK proved:

```text
allMapIntroOpcodeFamiliesOwned=yes
worldRestored=yes
legacyEntityMutation=no
framebufferMutation=no
entities=0 monsters=0 noGameplay=yes
```

## Why level-exit stats are next

The real intro CHANGEMAP is:

```text
name=/junction.bsp
targetMap=9
spawnParam=0
showStats=1
effects=03
```

Legacy `Game_changeMap()` therefore calls `Player_addLevelStats(true)` and opens the map-stats menu before any real Junction load.

The native engine now has exactly the owners needed for the map-derived counters:

```text
secret totals/found -> immutable line flag 0x8 + mutable OPEN bit
monster totals/dead -> native enemy entities + topology ALIVE bit
```

## Current permanent API

Files:

```text
ESP32/include/esp_map_level_exit_stats.h
ESP32/src/esp_map_level_exit_stats.c
```

API:

```text
EspMapLevelExitStats_collect(loadMapId, showStats, outStats)
```

Result ABI target:

```text
EspMapLevelExitStats = 20 B
```

The result contains:

```text
completionLevelBit
secretsFound / secretsTotal
monstersDead / monstersTotal
loadMapId / showStats
markCompleted / markAllSecrets / markAllMonsters
effectFlags
```

Base effect flags reproduce the non-map writes that a future native player-state consumer must perform:

```text
01 accumulate time
02 accumulate moves
04 reset berserker
08 clear familiar
```

Completion flags:

```text
10 mark completed
20 mark all secrets
40 mark all monsters
```

They apply only when `showStats && loadMapId != 2`, exactly matching legacy. The permanent collector references no legacy Player/Menu/Game/Render/DoomCanvas type and allocates nothing.

## Candidate real-CYD proof

The probe runs after hardware-proven SHOW/HIDE and requires all previous FNVs unchanged.

It will establish hardware canons for:

```text
secretsFound / secretsTotal
monstersDead / monstersTotal
markCompleted / markAllSecrets / markAllMonsters
completionLevelBit
effectFlags
statsFNV
```

It independently recomputes source counts from raw native storage.

It also proves dynamic sensitivity:

```text
real deferred-blocker SHOW
 -> collect again
 -> monster-dead delta == removed enemy blockers
 -> topology rollback exact to 3f321e43

one real secret line if present
 -> toggle native OPEN bit
 -> secretsFound +/-1
 -> line rollback exact to e5e74861
```

Legacy gates:

```text
showStats=0 -> base effects only, no completion proposal
loadMapId=2 -> base effects only, no completion proposal
```

Fail closed:

```text
mapId0=1
mapId33=1
showStats2=1
nullResult=1
stateAtomic=yes
```

RAM/integrity target:

```text
resultBytes=20
persistentHeapBytes=0
heap8 delta=0
largest8 delta=0
framebuffer unchanged
all native owner FNVs restored
legacy Player exit-stat fields unchanged
legacy menu/transition state unchanged
PAK closed
legacy runtime clear
entities=0 monsters=0
```

## Validation target

Build/flash normal optimized environment:

```text
esp32-cyd
```

Branch / firmware:

```text
agent/esp32-map1-native-level-exit-stats
f9a05933a00fab26b1c0e2b15375d074161ef2bc
```

Capture `[MAPEXITSTATS]` and stable `[ALIVE]` lines.

No CI status is published for this firmware candidate. No local build or real-CYD PASS is claimed.

## Current architecture boundary

Hardware-proven ownership through PR #62:

```text
compact immutable map runtime
allocation-free semantic access
tile/script/event native state
all 16 real MAP_INTRO opcode families
UI/string/status/dialog/notebook/key/password owners
line/open/lock/texture world overlays
GIVEMAP automap state
SAVEGAME durable route
CHANGEMAP pending transition intent
SHOW/HIDE compact sprite/entity topology
```

Candidate adds:

```text
pure native level-exit stats snapshot / completion plan
```

Still outside:

```text
native player-state application of exit effects
native stats-menu consumer
actual CHANGEMAP transition/map swap
full native entity/monster gameplay
legacy-world-free gameplay loop
native gameplay renderer
ST_PLAYING progression
actual sound playback
```

Do not merge the current candidate until the exact firmware above passes on the real classic CYD and all post-test changes are documentation-only.
