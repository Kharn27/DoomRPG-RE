# ESP32 native player exit-state milestone

Branch: `agent/esp32-native-player-exit-state`

Base merged `main`:

```text
PR   = #63 — native MAP_INTRO level-exit stats
main = 533784b5483e14a12558fb08c9331d8b744caa88
```

Firmware candidate:

```text
f8c5a1c398c0946025aef976f7a997589bae4923
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

The previous hardware milestone produced the exact map-derived input for recovered `Player_addLevelStats()`:

```text
loadMapId=1 showStats=1
secrets=0/6
monsters=0/30
completionLevelBit=00000001
effects=1f
statsFNV=bd41bcfa
```

This milestone consumes that value into a small pointer-free native player exit owner. It still does not mutate legacy `Player_t`, open the stats menu or trigger CHANGEMAP.

## Exact legacy writes

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

The native owner deliberately does not retain `player.time`, `player.moves`, `dogFamiliar` or any Entity pointer. The future gameplay/player core supplies elapsed time and current level moves explicitly.

## Permanent API

Files:

```text
ESP32/include/esp_player_exit_state.h
ESP32/src/esp_player_exit_state.c
```

State:

```text
EspPlayerExitState = 28 B expected

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

Result:

```text
EspPlayerExitApplyResult = 28 B expected
```

API:

```text
EspPlayerExitState_reset(state)
EspPlayerExitState_apply(state, stats, elapsedTimeMs, levelMoves, result)
```

Permanent dependencies are only `stdint`, `string` and `esp_map_level_exit_stats.h`. There is no dependency on legacy Player/Game/Menu/Render/DoomCanvas/Entity or clocks.

## Validation rules

The consumer does not blindly trust caller input. Before mutation it verifies the full `EspMapLevelExitStats` contract:

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

Any inconsistent value fails closed with state unchanged and a zero result.

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

`uint32_t` arithmetic preserves the 32-bit bit-pattern/wrap behavior expected on the target.

## Temporary real-CYD probe

Files:

```text
ESP32/include/native_player_exit_state_probe.h
ESP32/src/native_player_exit_state_probe.c
```

The probe runs after the hardware-proven level-exit stats probe.

### Deterministic source-apply proof

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

With the real intro snapshot (`effects=1f`), hardware must prove:

```text
time/moves accumulate exactly
completedLevels ORs bit 00000001
killed/found masks unchanged
berserker -> 0
familiar -> 0
source result flags exact
```

Static little-endian ABI calculations predict these FNVs; hardware remains authoritative:

```text
initialFNV = 940b0171
appliedFNV = 298eaaa4
resultFNV  = 5d10a566
```

The probe then reapplies the same completion with zero elapsed/moves and requires mask/reset idempotence.

### Gate proof

It consumes the already-proven collector variants:

```text
showStats=0
loadMapId=2
```

Both must still accumulate time/moves and clear berserker/familiar, while leaving all progression masks unchanged.

### All-mask proof

A structurally valid derived snapshot sets both found/dead counts equal to totals, therefore:

```text
markCompleted=1
markAllSecrets=1
markAllMonsters=1
effects=7f
```

All three progression masks must OR the same level bit. Static predicted state FNV is `c93e8128`; hardware is final truth.

### Live legacy projection

Without modifying legacy Player, the probe seeds native state from the live legacy exit-related fields, captures once:

```text
elapsed = uptime - player.time
moves   = player.moves
```

and verifies the native post-state formula exactly. Legacy Player remains an equality witness only.

### Fail closed

Hardware must prove:

```text
nullState=1
nullStats=1
nullResult=1
effectMismatch=1
bitMismatch=1
rangeMismatch=1
stateAtomic=yes
```

## RAM / integrity target

Hardware-proven persistent native heap entering this milestone:

```text
18008 B
```

Both state and result are caller-owned values. Target:

```text
persistentHeapBytes=0
heap8 delta=0
largest8 delta=0
hardware persistent total remains 18008 B
```

Integrity requires exact preservation of:

```text
lineStateFNV      e5e74861
spriteTopologyFNV 3f321e43
framebuffer
legacy Player exit-related fields
legacy menu/transition state
legacy Render runtime clear
PAK closed
entities=0 monsters=0
```

## Expected Serial family

```text
[PLAYEREXITPROBE] ARMED ...

=== Doom RPG ESP32-native player exit-state application ===
[PLAYEREXITPROBE] CONTRACT ...
[PLAYEREXIT] READY stateBytes=28 resultBytes=28 elapsed=12345 moves=37 initialFNV=... appliedFNV=... resultFNV=... effects=1f ...
[PLAYEREXIT] MASKS sourceCompleted=1 sourceSecrets=0 sourceMonsters=0 repeatIdempotent=1 allMasks=1 allStateFNV=... noStatsGate=1 mapId2Gate=1
[PLAYEREXIT] LIVE elapsed=... moves=... projection=1 liveStateFNV=... legacyPlayerUnchanged=yes
[PLAYEREXIT] STATE rollbackFNV=... rollback=1 familiarSemanticOnly=yes entityPointerStored=no
[PLAYEREXIT] FAILCLOSED ... stateAtomic=yes
[PLAYEREXIT] RAM ... persistentHeapBytes=0 ...
[PLAYEREXIT] LEGACY ... playerMutation=no menuMutation=no transitionTriggered=no
[PLAYEREXIT] PARK ... nativePlayerExitState=yes ... entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

Use normal PlatformIO environment `esp32-cyd`.

No CI status is published for the candidate. No local build or real-CYD PASS is claimed.

## Boundary after PASS

A PASS will complete native ownership/application of the `Player_addLevelStats()` exit-state writes while keeping legacy Player untouched.

The next bounded consumer should then be the stats-menu transition intent/owner for the already-proven `CHANGEMAP showStats=1` path. Actual map swap remains later.
