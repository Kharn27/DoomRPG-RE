# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #62 — native SHOW/HIDE sprite topology
main = ed5cd9a09c9ae36f999661f4284f64400681b1af
hardware-tested firmware = f881ccdad20d950462dd781456c340e792f59ec3
```

Merged evidence: [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md).

All 16 real MAP_INTRO opcode IDs have explicit native ownership/execution boundaries.

## Current merge-ready milestone

```text
branch = agent/esp32-map1-native-level-exit-stats
base   = ed5cd9a09c9ae36f999661f4284f64400681b1af
hardware-tested firmware = f9a05933a00fab26b1c0e2b15375d074161ef2bc
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Active evidence: [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md).

This is the first hardware-proven post-event-family consumer. It computes the map-derived portion of legacy `Player_addLevelStats()` without mutating legacy Player/Menu/Game/Render state.

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

Level-exit stats is a caller-owned 20 B value and adds no persistent allocation. Hardware total remains exactly `18008 B`.

## Hardware-proven fingerprints

Inherited canons:

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
spriteTopologyFNV       = 3f321e43
showResultFNV           = 6029eb3c
hideResultFNV           = d24f5bae
showStateFNV            = b6a45f47
hideStateFNV            = bec68187
contextAfterShowFNV     = 2de723aa
contextAfterHideFNV     = bb1d78a4
legacyEntityTopologyFNV = f8f9b485
```

New level-exit stats canons:

```text
levelExitStatsFNV       = bd41bcfa
levelExitNoStatsFNV     = d9532169
levelExitMapId2FNV      = ceb6ad21
levelExitShowSensFNV    = 5155b517
levelExitSecretOpenFNV  = 6694b0e1
```

Same-build legacy witnesses for this firmware:

```text
playerStatsFNV = 17e22395
transitionFNV  = f450c49f
```

These legacy witnesses are equality guards for this milestone; the permanent native collector does not depend on legacy objects.

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

## Level-exit stats permanent API

Files:

```text
ESP32/include/esp_map_level_exit_stats.h
ESP32/src/esp_map_level_exit_stats.c
```

API:

```text
EspMapLevelExitStats_collect(loadMapId, showStats, outStats)
```

Hardware-proven ABI:

```text
EspMapLevelExitStats = 20 B
```

Permanent dependencies are only:

```text
EspMapRuntime
EspMapLineState
EspMapSpriteTopology
```

No Player/Menu/Game/Render/DoomCanvas object is referenced by the permanent collector. It performs no PAK I/O, no ZIP I/O and no allocation.

## Real-CYD level-exit snapshot

Canonical intro result:

```text
loadMapId          = 1
showStats          = 1
secrets            = 0 / 6
monsters           = 0 / 30
markCompleted      = 1
markAllSecrets     = 0
markAllMonsters    = 0
completionLevelBit = 00000001
effects            = 1f
statsFNV           = bd41bcfa
elapsed            = 11 ms
```

Effect interpretation:

```text
01 accumulate time
02 accumulate moves
04 reset berserker
08 clear familiar
10 mark completed
20 mark all secrets
40 mark all monsters

source result 1f = 0f base effects + 10 mark completed
```

## Legacy gates

Hardware proved:

```text
showStats=0  -> base effects only
loadMapId=2  -> base effects only
baseEffects  = 0f
showStats0   = yes
loadMapId2   = yes
equalityOnZero=legacy
```

Fingerprints:

```text
noStatsFNV         = d9532169
noCompletionMapFNV = ceb6ad21
```

## Dynamic sensitivity proofs

Real SHOW / deferred blocker:

```text
cmd=205 event=74 off=2
enemyBlockersRemoved=1
topologyFNV 3f321e43 -> 723e7300 -> 3f321e43
mutated statsFNV = 5155b517
```

This proves native level-exit monster counts consume the compact ALIVE state changed by SHOW without calling legacy `Entity_died()`.

Real secret-line sensitivity:

```text
line=39
initialOpen=0
proof=1
lineFNV e5e74861 -> 6694b0e1 -> e5e74861
```

## Fail closed

```text
mapId0      = 1
mapId33     = 1
showStats2  = 1
nullResult  = 1
stateAtomic = yes
```

## RAM / integrity evidence

```text
heap8      65640 -> 65640 delta=0
largest8   34804 -> 34804 delta=0
persistentHeapBytes=0
frameFNV   bd237825 -> bd237825
arenaFNV   c3882516 -> c3882516
lineFNV    e5e74861 -> e5e74861
topologyFNV 3f321e43 -> 3f321e43
```

Legacy guards:

```text
playerStatsFNV 17e22395 -> 17e22395
transitionFNV  f450c49f -> f450c49f
legacyRuntimeClear=yes
Player_addLevelStatsCalled=no
menuMutation=no
transitionTriggered=no
```

Final PARK:

```text
state=9 page=3
nativeExitStats=yes
resultBytes=20
persistentBytes=0
allMapIntroOpcodeFamiliesOwned=yes
playerMutation=no
menuMutation=no
worldRestored=yes
entities=0 monsters=0 noGameplay=yes
```

Stable real-CYD heartbeats:

```text
35280 ms heap=131404 heap8=65640 largest8=34804
40281 ms heap=131404 heap8=65640 largest8=34804
```

## Current architecture boundary

Hardware-proven native ownership now includes:

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
pure native level-exit stats snapshot / completion plan
```

Still intentionally outside:

```text
application of exit effects to native player state
native stats-menu consumer
actual CHANGEMAP transition / Junction map swap
full native entity/monster gameplay
legacy-world-free gameplay loop
native gameplay renderer
ST_PLAYING progression
sound playback
```

`entities=0`, `monsters=0`, `ST_PLAYING` is not reached, and `shapeData/mediaTexels` remain NULL.

## Merge recommendation

```text
MERGE agent/esp32-map1-native-level-exit-stats
```

Hardware-tested firmware is `f9a05933a00fab26b1c0e2b15375d074161ef2bc`. All commits after that firmware must remain documentation-only until merge; no further flash is required if that condition holds.
