# ESP32 native stats-menu intent milestone

Branch: `agent/esp32-native-stats-menu-intent`

Base merged `main`:

```text
PR   = #64 — native player exit-state
main = 3759bcd12a3f6d36a6a696457110ab27474c24b8
```

Hardware-tested firmware:

```text
1dddbe86788389400d6e2186595174e723c72f5c
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective and result

The real `EV_CHANGEMAP showStats=1` path is now natively owned through the semantic stats-menu pause point:

```text
native CHANGEMAP pending intent
 -> native level-exit stats
 -> native player exit-state application
 -> native stats-menu intent
```

Recovered `Game_changeMap()` performs these logical writes after the player level-exit writes:

```text
menu->mapNameId = targetMapId
MenuSystem_setMenu(targetMapId == MAP_END_GAME
                   ? MENU_MAP_STATS_OVERALL
                   : MENU_MAP_STATS)
game->changeMapParam = 0
```

The `showStats` branch does **not** load the target map at this point.

This milestone projects those writes into one small pointer-free semantic intent. It does not mutate legacy Menu/Game/Render state, render a menu, resolve arbitrary map names or load Junction.

## Permanent API

Files:

```text
ESP32/include/esp_stats_menu_intent.h
ESP32/src/esp_stats_menu_intent.c
```

Hardware-proven ABI:

```text
EspStatsMenuIntent = 4 B
```

Fields:

```text
targetMapId      uint8
menuKind         uint8  # NONE / LEVEL / OVERALL
active           uint8
consumePending   uint8
```

Semantic kinds deliberately do not encode legacy `MENU_*` numeric IDs:

```text
NONE    = 0
LEVEL   = 1
OVERALL = 2
```

API:

```text
EspStatsMenuIntent_reset(intent)
EspStatsMenuIntent_prepare(targetMapId, showStats, intent)
```

Rules:

```text
targetMapId 1..13
showStats must be 0 or 1
showStats=0 -> NOT_APPLICABLE + zero intent
showStats=1 + target != MAP_END_GAME -> LEVEL
showStats=1 + target == MAP_END_GAME -> OVERALL
successful intent -> active=1, consumePending=1
```

`consumePending=1` is the native semantic equivalent of legacy `game->changeMapParam = 0` after the branch is consumed. This milestone does not mutate the previously proven CHANGEMAP owner itself.

Target map-name -> map-ID resolution remains a separate future transition/catalog concern. For the real MAP_INTRO command, target ID `9 / MAP_JUNCTION` was already hardware-proven by the CHANGEMAP milestone.

Permanent code has no dependency on `Menu_t`, `MenuSystem_t`, `Game_t`, `Render_t`, `DoomCanvas_t`, PAK I/O or allocation.

## Real-CYD proof

The hardware-tested firmware is exactly:

```text
1dddbe86788389400d6e2186595174e723c72f5c
```

### Real Junction path

Hardware:

```text
intentBytes=4
targetMap=9
menuKind=1 / LEVEL
active=1
consumePending=1
sourceStatsFNV=bd41bcfa
intentFNV=96afe901
legacyMenuId=15
```

The precomputed raw-ABI fingerprint matched hardware exactly:

```text
intent bytes = 09 01 01 01
intentFNV    = 96afe901
```

### End-game variant

Hardware explicitly proved the legacy special case:

```text
endGameTarget=13
endGameKind=2 / OVERALL
endGameFNV=deea91b4
legacyOverallId=16
```

The precomputed `deea91b4` fingerprint matched hardware exactly. Legacy menu IDs are observed only by the temporary probe; permanent native code remains independent of those numeric values.

### Direct-load gate

Hardware:

```text
showStats=0
noStatsStatus=1 / NOT_APPLICABLE
noStatsZero=1
zeroFNV=4b95f515
repeatExact=1
```

The Serial line emitted `noStatsStatus=1noStatsZero=1` without a separating space; both fields are unambiguous and passed.

The actual direct-load path remains future transition work.

### Fail closed

Hardware:

```text
target0=1
target14=1
showStats2=1
nullIntent=1
reset=1
stateAtomic=yes
```

Invalid calls fail closed with no partial state mutation.

## RAM / integrity

Hardware-proven persistent native heap entering this milestone was `18008 B`.

This milestone adds no persistent allocation:

```text
heap8      65616 -> 65616 delta=0
largest8   34804 -> 34804 delta=0
persistentHeapBytes=0
```

Therefore persistent native total remains exactly:

```text
18008 B
```

Same-build native integrity witnesses:

```text
frameFNV        e8a3b4ef -> e8a3b4ef
lineStateFNV    e5e74861
spriteTopologyFNV=3f321e43
sourceStatsFNV  bd41bcfa
```

Legacy witnesses:

```text
playerExitFNV 0b2ae445 -> 0b2ae445
transitionFNV f450c49f -> f450c49f
legacyRuntimeClear=yes
menuMutation=no
Game_changeMapCalled=no
mapLoad=no
```

The `playerExitFNV=0b2ae445` witness is the same-build probe projection for this milestone; the previously hardware-proven Player-exit semantic fingerprints remain unchanged.

No call was made to `Game_changeMap()`, `MenuSystem_setMenu()` or `DoomCanvas_loadMap()`.

## Final PARK

```text
state=9 page=3
nativeStatsMenuIntent=yes
intentBytes=4
targetMap=9
menuKind=LEVEL
consumePendingSemantic=yes
persistentBytes=0
nativePlayerExitState=yes
legacyMenuMutation=no
transitionTriggered=no
entities=0
monsters=0
noGameplay=yes
```

Stable real-CYD heartbeats:

```text
95234  ms heap=131380 heap8=65616 largest8=34804
100236 ms heap=131380 heap8=65616 largest8=34804
...
270270 ms heap=131380 heap8=65616 largest8=34804
```

The long stable heartbeat run proves no delayed allocation/leak or deferred legacy transition occurred after the probe.

## Architecture boundary after PASS

Semantic native ownership of the recovered `Game_changeMap()` show-stats pause path is complete through:

```text
CHANGEMAP pending intent
 -> exit stats
 -> player exit state
 -> stats-menu intent
```

Still intentionally outside:

```text
actual stats-menu rendering/input consumer
generic target map-name -> ID/catalog resolution
transition preflight and lifecycle handoff
actual Junction map swap
full native entity/monster gameplay
native ST_PLAYING loop/rendering
sound playback
```

The next bounded milestone should establish the target transition/catalog preflight needed before replacing `/intro.bsp` with `/junction.bsp`, without yet performing the destructive map swap.

The real classic CYD Serial log is the final hardware source of truth.
