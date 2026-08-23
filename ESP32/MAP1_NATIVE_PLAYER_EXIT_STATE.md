# ESP32 native player exit-state milestone

Branch: `agent/esp32-native-player-exit-state`

Base merged `main`:

```text
PR   = #63 — native MAP_INTRO level-exit stats
main = 533784b5483e14a12558fb08c9331d8b744caa88
```

Hardware-tested firmware:

```text
f8c5a1c398c0946025aef976f7a997589bae4923
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective and result

The previous hardware milestone produced the exact map-derived input for recovered `Player_addLevelStats()`:

```text
loadMapId=1 showStats=1
secrets=0/6
monsters=0/30
completionLevelBit=00000001
effects=1f
statsFNV=bd41bcfa
```

This milestone consumes that value into a small pointer-free native player exit owner. It does not mutate legacy `Player_t`, open the stats menu or trigger CHANGEMAP.

## Exact legacy writes represented

Recovered `Player_addLevelStats(player, z)` writes only:

```text
totalTime += now - player.time
totalMoves += player.moves
completedLevels |= current-level bit          [gated]
foundSecretsLevels |= current-level bit       [gated + all secrets]
killedMonstersLevels |= current-level bit     [gated + all monsters]
berserkerTics = 0
dogFamiliar = NULL
```

The native owner deliberately does not retain `player.time`, `player.moves`, `dogFamiliar` or any Entity pointer. Elapsed time and current level moves are explicit caller inputs.

## Permanent API

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

State:

```text
totalTime               uint32
totalMoves              uint32
completedLevels         uint32
killedMonstersLevels    uint32
foundSecretsLevels      uint32
berserkerTics           uint32
familiarActive          uint8
reserved                3 B
```

`familiarActive` is semantic presence only. No pointer is stored.

API:

```text
EspPlayerExitState_reset(state)
EspPlayerExitState_apply(state, stats, elapsedTimeMs, levelMoves, result)
```

Permanent dependencies are only `stdint`, `string` and `esp_map_level_exit_stats.h`. There is no dependency on legacy Player/Game/Menu/Render/DoomCanvas/Entity, no clock access, no PAK/ZIP I/O and no allocation.

## Validation rules

Before mutation the consumer validates the full `EspMapLevelExitStats` contract:

```text
loadMapId 1..32
showStats / mark booleans in 0..1
found <= total
dead <= total
no unknown effect bits
base effects exactly 0f
completion bit exactly 1 << (loadMapId-1) when gated
completion bit zero when not gated
markAllSecrets exactly mirrors found==total when gated
markAllMonsters exactly mirrors dead==total when gated
effect byte exactly matches the derived marks
```

Invalid input fails closed with state unchanged and a zero result.

## Application semantics

For a valid snapshot:

```text
state.totalTime  += elapsedTimeMs
state.totalMoves += levelMoves
state.berserkerTics = 0
state.familiarActive = 0

if markCompleted:
    completedLevels |= completionLevelBit
if markAllSecrets:
    foundSecretsLevels |= completionLevelBit
if markAllMonsters:
    killedMonstersLevels |= completionLevelBit
```

`uint32_t` arithmetic preserves target 32-bit bit-pattern/wrap behavior.

## Real-CYD deterministic apply proof

Seed:

```text
totalTime            = 10203040
totalMoves           = 01020304
completedLevels      = 00000004
killedMonstersLevels = 00000008
foundSecretsLevels   = 00000010
berserkerTics        = 9
familiarActive       = 1
elapsed              = 12345 ms
levelMoves           = 37
```

Hardware result with real intro `effects=1f`:

```text
stateBytes=28
resultBytes=28
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

The precomputed little-endian fingerprints matched the real CYD exactly.

## Mask / gate proof

Hardware:

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

Therefore:

```text
same completion with 0 elapsed/moves -> progression/reset idempotent
showStats=0 -> no progression-mask mutation
loadMapId=2 -> no progression-mask mutation
valid all-complete stats -> all three progression masks OR the level bit
```

## Live legacy projection

The probe seeded native state from the live legacy exit-related fields without modifying legacy Player and captured once:

```text
elapsed=64325
moves=0
projection=1
liveStateFNV=57fce418
legacyPlayerUnchanged=yes
```

The live elapsed value is run-timing-specific; the contract is exact formula agreement and legacy equality.

## Rollback / pointer boundary

```text
rollbackFNV=940b0171
rollback=1
familiarSemanticOnly=yes
entityPointerStored=no
```

No Entity/familiar pointer crosses into permanent native ownership.

## Fail closed

Hardware proof:

```text
nullState=1
nullStats=1
nullResult=1
effectMismatch=1
bitMismatch=1
rangeMismatch=1
stateAtomic=yes
```

## RAM / integrity

Hardware-proven persistent native heap entering this milestone was `18008 B`.

This milestone adds no persistent allocation:

```text
heap8      65632 -> 65632 delta=0
largest8   34804 -> 34804 delta=0
persistentHeapBytes=0
```

Therefore persistent native total remains exactly:

```text
18008 B
```

Same-build integrity witnesses:

```text
frameFNV        ef79123a -> ef79123a
lineStateFNV    e5e74861
spriteTopologyFNV=3f321e43
```

Legacy witnesses:

```text
playerExitFNV = f5cbf9f5 -> f5cbf9f5
transitionFNV = f450c49f -> f450c49f
legacyRuntimeClear=yes
Player_addLevelStatsCalled=no
playerMutation=no
menuMutation=no
transitionTriggered=no
```

## Final PARK

```text
state=9 page=3
nativePlayerExitState=yes
stateBytes=28
resultBytes=28
persistentBytes=0
nativeExitStats=yes
playerMutationProven=yes
legacyPlayerMutation=no
entities=0
monsters=0
noGameplay=yes
```

Stable real-CYD heartbeats:

```text
70245 ms heap=131396 heap8=65632 largest8=34804
75246 ms heap=131396 heap8=65632 largest8=34804
80247 ms heap=131396 heap8=65632 largest8=34804
```

## Architecture boundary after PASS

Native ownership now includes both halves of recovered `Player_addLevelStats()`:

```text
map-derived level-exit snapshot
+
pointer-free player exit-state application
```

Legacy Player remains untouched.

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

The next bounded milestone should own the stats-menu transition intent for the already-proven `CHANGEMAP showStats=1` path. Actual map swap remains later.

The real classic CYD Serial log is the final hardware source of truth.
