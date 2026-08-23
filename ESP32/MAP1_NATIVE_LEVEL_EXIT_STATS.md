# ESP32 MAP_INTRO native level-exit stats milestone

Branch: `agent/esp32-map1-native-level-exit-stats`

Base merged `main`:

```text
PR   = #62 — native SHOW/HIDE sprite topology
main = ed5cd9a09c9ae36f999661f4284f64400681b1af
```

Hardware-tested firmware:

```text
f9a05933a00fab26b1c0e2b15375d074161ef2bc
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective and result

MAP_INTRO event-family ownership was already complete. This milestone is the first native consumer after that boundary.

The real intro `EV_CHANGEMAP` has `showStats=1`. Legacy `Game_changeMap()` therefore first performs:

```text
Player_addLevelStats(player, true)
menu->mapNameId = targetMap
MenuSystem_setMenu(MENU_MAP_STATS)
```

before a later map transition.

This milestone now hardware-proves the **map-derived level-exit stats snapshot/plan** without mutating Player, Menu, Game, Render or DoomCanvas and without triggering CHANGEMAP.

## Exact legacy semantics represented

`Player_addLevelStats(player, z)` always performs the future player-state effects:

```text
totalTime  += now - player.time
totalMoves += player.moves
berserkerTics = 0
dogFamiliar = NULL
```

When:

```text
z && render->loadMapID != 2
```

it also marks the current level completed and evaluates map-derived secret and monster predicates.

Secrets:

```text
for each line where line.flags & 8:
    totalSecrets++
    if line.flags & 64:
        foundSecrets++
```

Native equivalent:

```text
immutable line.flags & 0x8
+
mutable EspMapLineState OPEN bit
```

Monsters:

```text
for each legacy entity with entity->monster:
    totalMonsters++
    dead if sprite death bit or entity inactive/dead bit is set
```

Native equivalent:

```text
entity type == enemy
+
ESP_MAP_SPRITE_TOPOLOGY_EXISTS
+
dead iff ESP_MAP_SPRITE_TOPOLOGY_ALIVE is clear
```

The BSP-native header already proves `/intro.bsp` has `loadMapId=1`, so the completion bit is `0x00000001`.

## Permanent API

Files:

```text
ESP32/include/esp_map_level_exit_stats.h
ESP32/src/esp_map_level_exit_stats.c
```

API:

```text
EspMapLevelExitStats_collect(loadMapId, showStats, outStats)
```

Permanent dependencies are only native map owners. There is no Player/Menu/Game/Render/DoomCanvas dependency, no PAK I/O, no ZIP access and no allocation.

Hardware-proven ABI:

```text
EspMapLevelExitStats = 20 B
```

Fields include:

```text
completionLevelBit
secretsFound / secretsTotal
monstersDead / monstersTotal
loadMapId / showStats
markCompleted / markAllSecrets / markAllMonsters
effectFlags
```

## Effect flags

The collector reports future player-state obligations without applying them:

```text
01 ACCUMULATE_TIME
02 ACCUMULATE_MOVES
04 RESET_BERSERKER
08 CLEAR_FAMILIAR
10 MARK_COMPLETED
20 MARK_ALL_SECRETS
40 MARK_ALL_MONSTERS
```

Base effects `0x0f` always apply. Completion effects apply only when `showStats != 0 && loadMapId != 2`, matching legacy, including equality semantics when a total is zero.

## Real-CYD source snapshot

Hardware output:

```text
resultBytes       = 20
loadMapId         = 1
showStats         = 1
secretsFound      = 0
secretsTotal      = 6
monstersDead      = 0
monstersTotal     = 30
markCompleted     = 1
markAllSecrets    = 0
markAllMonsters   = 0
completionLevelBit= 00000001
effectFlags       = 1f
statsFNV          = bd41bcfa
elapsed           = 11 ms
```

Interpretation:

```text
0x1f = base exit effects 0x0f + MARK_COMPLETED 0x10
```

No all-secrets or all-monsters completion is proposed because source MAP_INTRO begins at `0/6` secrets and `0/30` monsters dead.

## Legacy gate proof

Hardware established both legacy gates:

```text
showStats=0  -> base effects only
loadMapId=2  -> base effects only
```

Fingerprints:

```text
noStatsFNV         = d9532169
noCompletionMapFNV = ceb6ad21
baseEffects        = 0f
showStats0         = yes
loadMapId2         = yes
equalityOnZero     = legacy
```

## Real SHOW / deferred-death sensitivity

The probe scanned the real corpus and selected a real SHOW with deterministic blocker removal:

```text
cmd=205
event=74
off=2
enemyBlockersRemoved=1
```

Topology and stats witnesses:

```text
topologyFNV 3f321e43 -> 723e7300
mutated statsFNV      = 5155b517
rollback topologyFNV  = 3f321e43
```

This proves that the level-exit monster snapshot consumes the same compact `ALIVE` state changed by native SHOW blocker projection. The selected real command removes one enemy blocker; the collector observes the corresponding monster-death sensitivity without invoking legacy `Entity_died()`.

## Secret-line sensitivity

A real secret line was used:

```text
lineIndex   = 39
initialOpen = 0
proof       = 1
```

Native line-state witness:

```text
lineFNV e5e74861 -> 6694b0e1 -> e5e74861
```

The temporary OPEN mutation changed the secret-found count exactly as required, then rollback restored the canonical line state.

## Fail closed

Hardware proof:

```text
mapId0      = 1
mapId33     = 1
showStats2  = 1
nullResult  = 1
stateAtomic = yes
```

Invalid requests do not mutate any native owner.

## RAM / integrity

Hardware-proven persistent heap entering this milestone was `18008 B`.

This milestone adds no persistent allocation:

```text
heap8      65640 -> 65640  delta=0
largest8   34804 -> 34804  delta=0
persistentHeapBytes=0
```

Therefore the native persistent total remains:

```text
18008 B
```

Same-build integrity witnesses:

```text
frameFNV    bd237825 -> bd237825
arenaFNV    c3882516 -> c3882516
lineFNV     e5e74861 -> e5e74861
topologyFNV 3f321e43 -> 3f321e43
```

Legacy witnesses:

```text
playerStatsFNV = 17e22395 -> 17e22395
transitionFNV  = f450c49f -> f450c49f
legacyRuntimeClear=yes
Player_addLevelStatsCalled=no
menuMutation=no
transitionTriggered=no
```

The milestone never calls `Player_addLevelStats()`, `Game_changeMap()`, `MenuSystem_setMenu()` or a legacy entity routine.

## Final PARK

```text
state=9 page=3
nativeExitStats=yes
resultBytes=20
persistentBytes=0
allMapIntroOpcodeFamiliesOwned=yes
playerMutation=no
menuMutation=no
worldRestored=yes
entities=0
monsters=0
noGameplay=yes
```

Stable heartbeat evidence:

```text
35280 ms heap=131404 heap8=65640 largest8=34804
40281 ms heap=131404 heap8=65640 largest8=34804
```

## Architecture boundary after PASS

This establishes the first hardware-proven native consumer after complete MAP_INTRO event-family ownership.

Native ownership now includes enough information to compute the real intro exit statistics without legacy Render/entities.

Still outside this milestone:

```text
application of exit effects to native player state
native stats-menu intent/consumer
actual CHANGEMAP transition / Junction map swap
full native entity/monster gameplay
legacy-world-free gameplay loop
native gameplay renderer
ST_PLAYING progression
sound playback
```

The next milestone should recover from merged `main` and choose one bounded next consumer, with a natural direction of applying the already-proven exit effects to a small explicitly owned native player exit state before opening menu/transition ownership.

The real classic CYD Serial log is the final hardware source of truth.
