# ESP32 MAP_INTRO native level-exit stats milestone

Branch: `agent/esp32-map1-native-level-exit-stats`

Base merged `main`:

```text
PR   = #62 — native SHOW/HIDE sprite topology
main = ed5cd9a09c9ae36f999661f4284f64400681b1af
```

Firmware candidate:

```text
f9a05933a00fab26b1c0e2b15375d074161ef2bc
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Why this is the next boundary

MAP_INTRO event-family ownership is complete: all 16 real opcode IDs have native ownership/execution boundaries.

The real intro `EV_CHANGEMAP` has `showStats=1`. Legacy `Game_changeMap()` therefore does not load Junction immediately. Its first consumer is:

```text
Player_addLevelStats(player, true)
menu->mapNameId = targetMap
MenuSystem_setMenu(MENU_MAP_STATS)
```

Only later does the transition proceed.

This milestone owns the **map-derived stats snapshot/plan only**. It does not mutate Player, Menu, Game, Render or DoomCanvas and does not trigger the map transition.

## Exact legacy source

`Player_addLevelStats(player, z)` always:

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

it additionally marks the level completed and evaluates two map-derived predicates.

Secrets:

```text
for every Render line:
    if line.flags & 8:
        totalSecrets++
        if line.flags & 64:
            foundSecrets++

if foundSecrets == totalSecrets:
    foundSecretsLevels |= 1 << (loadMapID - 1)
```

Monsters:

```text
for every legacy map entity with entity->monster:
    totalMonsters++
    if sprite.info & 0x01000000 OR entity.info & 0x00020000:
        deadMonsters++

if deadMonsters == totalMonsters:
    killedMonstersLevels |= 1 << (loadMapID - 1)
```

The BSP-native pass already hardware-proved `/intro.bsp` header `loadMapId=1`, so the completion bit for the intro is `0x00000001`.

## Permanent native API

Files:

```text
ESP32/include/esp_map_level_exit_stats.h
ESP32/src/esp_map_level_exit_stats.c
```

API:

```text
EspMapLevelExitStats_collect(loadMapId, showStats, outStats)
```

The implementation has no dependency on legacy `Player`, `Menu`, `Game`, `Render`, `DoomCanvas`, entities or sound.

It reads only:

```text
EspMapRuntime
EspMapLineState
EspMapSpriteTopology
```

Secret predicate:

```text
immutable line.flags & 0x8
+
mutable EspMapLineState OPEN bit
```

Monster predicate:

```text
native entity type == enemy
+
ESP_MAP_SPRITE_TOPOLOGY_EXISTS
+
dead iff ESP_MAP_SPRITE_TOPOLOGY_ALIVE is clear
```

The topology ALIVE bit is the native death predicate already mutated by deterministic SHOW blocker projection and intended for later native gameplay consumers.

## Result ABI

Expected classic ESP32 ABI:

```text
EspMapLevelExitStats = 20 B
```

Fields:

```text
completionLevelBit  uint32
secretsFound        uint16
secretsTotal        uint16
monstersDead        uint16
monstersTotal       uint16
loadMapId           uint8
showStats           uint8
markCompleted       uint8
markAllSecrets      uint8
markAllMonsters     uint8
effectFlags         uint8
```

The struct is caller-owned and the collector allocates nothing.

## Effect flags

The collector does not write player state. It reports the later native player consumer obligations:

```text
01 ACCUMULATE_TIME
02 ACCUMULATE_MOVES
04 RESET_BERSERKER
08 CLEAR_FAMILIAR
10 MARK_COMPLETED
20 MARK_ALL_SECRETS
40 MARK_ALL_MONSTERS
```

Base effects `0x0f` apply regardless of `showStats`, matching legacy `Player_addLevelStats()`.

Completion flags apply only when:

```text
showStats != 0 && loadMapId != 2
```

Equality semantics intentionally match legacy, including `0 == 0` totals.

## Temporary real-CYD probe

Files:

```text
ESP32/include/native_map1_level_exit_stats_probe.h
ESP32/src/native_map1_level_exit_stats_probe.c
```

The probe runs only after the hardware-proven final SHOW/HIDE probe.

### Source snapshot

It collects intro stats with:

```text
loadMapId=1
showStats=1
```

and independently recomputes the same counts from raw native line/open bitsets and topology type/link-state storage.

Hardware establishes:

```text
secretsFound / secretsTotal
monstersDead / monstersTotal
markCompleted
markAllSecrets
markAllMonsters
completionLevelBit
effectFlags
statsFNV
```

### Legacy gates

The probe also verifies:

```text
showStats=0
 -> base effects only
 -> no completion bit writes proposed

loadMapId=2 + showStats=1
 -> base effects only
 -> no completion bit writes proposed
```

### SHOW/death sensitivity

The probe scans the real 93-event corpus, resets the topology before each SHOW and locates the first real SHOW that reports `DEFER_BLOCKER_GAMEPLAY` with a deterministic blocker removal.

It then collects stats from the mutated native topology and verifies:

```text
secret counts unchanged
monster total unchanged
monster dead delta == number of removed enemy blockers
```

Then it resets topology and requires exact restoration to:

```text
3f321e43
```

The selected command/event and real `enemyBlockersRemoved` are hardware values.

### Secret sensitivity

If MAP_INTRO has at least one secret line, the probe toggles only that line's native OPEN bit, collects again, requires an exact +/-1 `secretsFound` delta, then restores the line state exactly to:

```text
e5e74861
```

If there are no secret lines, the probe reports the sensitivity path as not applicable instead of inventing one.

### Fail closed

```text
loadMapId=0  -> INVALID + zero result
loadMapId=33 -> INVALID + zero result
showStats=2  -> INVALID + zero result
NULL result  -> INVALID
```

## RAM / integrity target

Hardware-proven persistent native heap entering this milestone:

```text
18008 B
```

This milestone target:

```text
persistentHeapBytes = 0
heap8 delta          = 0
largest8 delta       = 0
```

Final integrity requires exact preservation of:

```text
arenaFNV            c3882516
mapStateFNV         cd99b98e
scriptFNV           f9e3d9df
lineStateFNV        e5e74861
lineTextureStateFNV f1fc1875
automapStateFNV     669b1aa7
spriteTopologyFNV   3f321e43
framebuffer
legacy Player exit-stat fields
legacy transition/menu state
```

Still required:

```text
PAK closed
legacy Render runtime clear
entities=0
monsters=0
ST_PLAYING not reached
```

The probe never calls `Player_addLevelStats()`, `Game_changeMap()`, `MenuSystem_setMenu()` or any legacy entity routine.

## Expected Serial family

```text
[MAPEXITSTATS] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO level-exit stats snapshot ===
[MAPEXITSTATS] CONTRACT ...
[MAPEXITSTATS] READY resultBytes=20 loadMapId=1 showStats=1 secrets=.../... monsters=.../... markCompleted=1 markAllSecrets=... markAllMonsters=... completionBit=00000001 effects=... statsFNV=... elapsed=...ms
[MAPEXITSTATS] GATES ... showStats0=yes loadMapId2=yes ...
[MAPEXITSTATS] SHOWSENS cmd=... event=... off=... enemyBlockersRemoved=... topologyFNV=3f321e43->... statsFNV=... rollback=3f321e43
[MAPEXITSTATS] SECRETSENS line=... initialOpen=... proof=... lineFNV=e5e74861->...->e5e74861
[MAPEXITSTATS] FAILCLOSED mapId0=1 mapId33=1 showStats2=1 nullResult=1 stateAtomic=yes
[MAPEXITSTATS] RAM ... persistentHeapBytes=0 ...
[MAPEXITSTATS] LEGACY ... Player_addLevelStatsCalled=no menuMutation=no transitionTriggered=no
[MAPEXITSTATS] PARK ... nativeExitStats=yes resultBytes=20 persistentBytes=0 ... entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

Use normal PlatformIO environment `esp32-cyd`.

No CI status is published for the candidate. No local build or hardware PASS is claimed.

## Boundary after PASS

A PASS establishes the first native consumer after complete MAP_INTRO event-family ownership.

The next transition work can then split cleanly into:

```text
native player exit-state application
native stats-menu intent/owner
native CHANGEMAP target preflight / map swap
```

without falling back to legacy entities or Render map state.
