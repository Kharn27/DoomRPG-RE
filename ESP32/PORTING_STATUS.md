# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #63 — native MAP_INTRO level-exit stats
main = 533784b5483e14a12558fb08c9331d8b744caa88
hardware-tested firmware = f9a05933a00fab26b1c0e2b15375d074161ef2bc
```

Merged evidence: [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md).

All 16 real MAP_INTRO opcode IDs have explicit native ownership/execution boundaries. Native level-exit stats is hardware-proven.

## Current merge-ready milestone

```text
branch = agent/esp32-native-player-exit-state
base   = 533784b5483e14a12558fb08c9331d8b744caa88
hardware-tested firmware = f8c5a1c398c0946025aef976f7a997589bae4923
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Active evidence: [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md).

This milestone consumes the hardware-proven 20 B level-exit snapshot into a 28 B pointer-free native player exit state without mutating legacy `Player_t`.

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

`entities=0`, `monsters=0`; `ST_PLAYING` is not reached.

## MAP_INTRO identity

```text
/intro.bsp / Entrance
bytes=21823 crc32=623f34e4 loadMapId=1
nodes=223 lines=480 mapSprites=344 events=93 byteCodes=265
strings=94 stringData=7779 maxString=313
```

Owned real opcode IDs:

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

Level-exit stats and player exit-state/result are caller-owned values and add no persistent allocation. Hardware total remains exactly `18008 B`.

## Hardware-proven fingerprints

Inherited native canons:

```text
arenaFNV                 = c3882516
mapStateFNV              = cd99b98e
scriptFNV                = f9e3d9df
lineStateFNV             = e5e74861
lineTextureStateFNV      = f1fc1875
automapStateFNV          = 669b1aa7
lineDoorFNV              = b1c9d297
unlockFNV                = 261d756a
giveMapFNV               = 98c7ac59
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
```

New player exit-state canons:

```text
playerExitInitialFNV     = 940b0171
playerExitAppliedFNV     = 298eaaa4
playerExitResultFNV      = 5d10a566
playerExitAllMasksFNV    = c93e8128
playerExitLiveFNV        = 57fce418   # same-build live projection
```

Same-build legacy witnesses:

```text
playerExitLegacyFNV = f5cbf9f5
transitionFNV       = f450c49f
```

## Hardware-proven level-exit snapshot

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
resultBytes        = 20
persistentHeapBytes= 0
```

## Permanent player exit-state API

Files:

```text
ESP32/include/esp_player_exit_state.h
ESP32/src/esp_player_exit_state.c
```

Hardware-proven ABI:

```text
EspPlayerExitState       = 28 B
EspPlayerExitApplyResult = 28 B
```

State owns only:

```text
totalTime
totalMoves
completedLevels
killedMonstersLevels
foundSecretsLevels
berserkerTics
familiarActive   # semantic bool only; no Entity pointer
```

API:

```text
EspPlayerExitState_reset(state)
EspPlayerExitState_apply(state, stats, elapsedTimeMs, levelMoves, result)
```

`elapsedTimeMs` and `levelMoves` are explicit caller inputs. The permanent owner has no dependency on legacy Player/Game/Menu/Render/DoomCanvas/Entity, no clock access, no PAK/ZIP I/O and no allocation.

## Real-CYD deterministic application

```text
stateBytes=28 resultBytes=28
elapsed=12345 moves=37
effects=1f

initialFNV=940b0171
appliedFNV=298eaaa4
resultFNV=5d10a566

totalTime  10203040 -> 10206079
totalMoves 01020304 -> 01020329
completed  00000004 -> 00000005
killed     00000008 -> 00000008
secrets    00000010 -> 00000010
berserker  9 -> 0
familiar   1 -> 0
```

The static predicted FNVs matched hardware exactly.

## Gates / mask proof

```text
sourceCompleted=1
sourceSecrets=0
sourceMonsters=0
repeatIdempotent=1
allMasks=1
allStateFNV=c93e8128
noStatsGate=1
mapId2Gate=1
```

A valid all-complete snapshot applies all three progression masks. Source-state completion is idempotent when reapplied with zero elapsed/moves.

## Live Player projection

```text
elapsed=64325
moves=0
projection=1
liveStateFNV=57fce418
legacyPlayerUnchanged=yes
```

`elapsed` is run-timing-specific; hardware proves exact formula projection without legacy mutation.

## Rollback / pointer boundary

```text
rollbackFNV=940b0171
rollback=1
familiarSemanticOnly=yes
entityPointerStored=no
```

## Fail closed

```text
nullState=1
nullStats=1
nullResult=1
effectMismatch=1
bitMismatch=1
rangeMismatch=1
stateAtomic=yes
```

## RAM / integrity evidence

```text
heap8      65632 -> 65632 delta=0
largest8   34804 -> 34804 delta=0
persistentHeapBytes=0
frameFNV   ef79123a -> ef79123a
lineFNV    e5e74861
topologyFNV=3f321e43
```

Legacy guards:

```text
playerExitFNV f5cbf9f5 -> f5cbf9f5
transitionFNV f450c49f -> f450c49f
legacyRuntimeClear=yes
Player_addLevelStatsCalled=no
playerMutation=no
menuMutation=no
transitionTriggered=no
```

Final PARK:

```text
state=9 page=3
nativePlayerExitState=yes
stateBytes=28 resultBytes=28
persistentBytes=0
nativeExitStats=yes
playerMutationProven=yes
legacyPlayerMutation=no
entities=0 monsters=0 noGameplay=yes
```

Stable heartbeats:

```text
70245 ms heap=131396 heap8=65632 largest8=34804
75246 ms heap=131396 heap8=65632 largest8=34804
80247 ms heap=131396 heap8=65632 largest8=34804
```

## Current architecture boundary

Hardware-proven ownership now includes:

```text
compact immutable native map
explicit tile/script/line/texture/automap/sprite mutable owners
all 16 real MAP_INTRO opcode families
SAVEGAME durable route
CHANGEMAP pending transition intent
SHOW/HIDE compact sprite/entity topology
native level-exit stats snapshot
native player exit-state application
```

Still intentionally outside:

```text
native stats-menu intent/consumer
actual CHANGEMAP / Junction map swap
full native entity/monster gameplay
legacy-world-free gameplay loop
native gameplay renderer
ST_PLAYING progression
sound playback
```

`shapeData` and `mediaTexels` remain NULL.

## Merge recommendation

```text
MERGE agent/esp32-native-player-exit-state
```

Hardware-tested firmware is `f8c5a1c398c0946025aef976f7a997589bae4923`. All commits after that firmware must remain documentation-only until merge.
