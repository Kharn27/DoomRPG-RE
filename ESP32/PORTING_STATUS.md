# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #63 — native MAP_INTRO level-exit stats
main = 533784b5483e14a12558fb08c9331d8b744caa88
hardware-tested firmware = f9a05933a00fab26b1c0e2b15375d074161ef2bc
```

Merged evidence: [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md).

All 16 real MAP_INTRO opcode IDs have explicit native ownership/execution boundaries. The first post-opcode consumer, native level-exit stats, is also hardware-proven.

## Current candidate

```text
branch = agent/esp32-native-player-exit-state
base   = 533784b5483e14a12558fb08c9331d8b744caa88
firmware candidate = f8c5a1c398c0946025aef976f7a997589bae4923
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md).

This milestone consumes the already-proven 20 B native level-exit snapshot into a 28 B pointer-free native player exit state without mutating legacy `Player_t`.

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

`entities=0`, `monsters=0` and `ST_PLAYING` is still not reached.

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

Level-exit stats is caller-owned 20 B and added 0 B persistent heap. Current candidate state/result are also caller-owned; target total remains `18008 B`.

## Hardware-proven fingerprints

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

Effects:

```text
01 accumulate time
02 accumulate moves
04 reset berserker
08 clear familiar
10 mark completed
20 mark all secrets
40 mark all monsters
```

Source intro result is `1f = 0f base + 10 completed`.

## Current permanent player-exit API

Files:

```text
ESP32/include/esp_player_exit_state.h
ESP32/src/esp_player_exit_state.c
```

Expected ABI:

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
familiarActive   # semantic bool only, no Entity pointer
```

API:

```text
EspPlayerExitState_reset(state)
EspPlayerExitState_apply(state, stats, elapsedTimeMs, levelMoves, result)
```

`elapsedTimeMs` and `levelMoves` are explicit caller inputs. The permanent owner has no dependency on legacy Player/Game/Menu/Render/DoomCanvas/Entity, no clock access, no PAK/ZIP I/O and no allocation.

The consumer validates the complete stats contract before mutation and fails closed on inconsistent effect flags, completion bits, ranges or mark predicates.

## Candidate validation target

Deterministic seed:

```text
totalTime=10203040 totalMoves=01020304
completed=00000004 killed=00000008 secrets=00000010
berserker=9 familiar=1
elapsed=12345 moves=37
```

With real intro stats `effects=1f`:

```text
time/moves accumulate
completed |= 00000001
killed/found masks unchanged
berserker -> 0
familiar -> 0
```

Static ABI/FNV prediction only; real CYD is authoritative:

```text
initialFNV = 940b0171
appliedFNV = 298eaaa4
resultFNV  = 5d10a566
allMasksFNV= c93e8128
```

Probe also requires:

```text
repeat completion with 0 elapsed/moves -> mask/reset idempotent
showStats=0 gate -> no progression-mask writes
loadMapId=2 gate -> no progression-mask writes
synthetic valid all-complete stats -> all three masks OR level bit
live legacy Player projection -> exact formula, legacy unchanged
null/inconsistent inputs -> fail closed, state atomic
heap8/largest8/framebuffer unchanged
lineStateFNV=e5e74861 unchanged
spriteTopologyFNV=3f321e43 unchanged
PAK closed
entities=0 monsters=0
```

RAM target:

```text
persistentHeapBytes = 0
persistent native total remains 18008 B
```

## Current architecture boundary

Hardware-proven ownership:

```text
compact immutable native map
explicit tile/script/line/texture/automap/sprite mutable owners
all 16 real MAP_INTRO opcode families
SAVEGAME durable route
CHANGEMAP pending transition intent
SHOW/HIDE compact sprite/entity topology
native level-exit stats snapshot
```

Candidate adds:

```text
native application of Player_addLevelStats exit writes to a pointer-free player state
```

Still outside:

```text
native stats-menu intent/consumer
actual CHANGEMAP / Junction map swap
full native entity/monster gameplay
legacy-world-free gameplay loop
native gameplay renderer
ST_PLAYING progression
sound playback
```

## Validation

Build/flash normal environment:

```text
esp32-cyd
```

Branch / firmware:

```text
agent/esp32-native-player-exit-state
f8c5a1c398c0946025aef976f7a997589bae4923
```

Capture `[PLAYEREXITPROBE]`, `[PLAYEREXIT]` and stable `[ALIVE]` lines.

No CI status is published for the candidate. No local build or hardware PASS is claimed.
