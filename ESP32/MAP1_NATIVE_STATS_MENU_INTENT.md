# ESP32 native stats-menu intent milestone

Branch: `agent/esp32-native-stats-menu-intent`

Base merged `main`:

```text
PR   = #64 — native player exit-state
main = 3759bcd12a3f6d36a6a696457110ab27474c24b8
```

Firmware candidate:

```text
1dddbe86788389400d6e2186595174e723c72f5c
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

The show-stats branch of the real MAP_INTRO `EV_CHANGEMAP` is now owned through:

```text
native CHANGEMAP pending intent
 -> native level-exit stats
 -> native player exit-state application
```

Recovered `Game_changeMap()` then performs only these logical writes before waiting in the stats UI:

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

ABI:

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

## Temporary real-CYD probe

Files:

```text
ESP32/include/native_stats_menu_intent_probe.h
ESP32/src/native_stats_menu_intent_probe.c
```

The probe runs only after the hardware-proven native Player exit-state probe.

### Real Junction path

It first recollects the already-proven intro exit snapshot:

```text
loadMapId=1
showStats=1
secrets=0/6
monsters=0/30
effects=1f
statsFNV=bd41bcfa
```

It then prepares the real target:

```text
targetMap=9 / MAP_JUNCTION
menuKind=LEVEL
active=1
consumePending=1
```

Static raw-ABI prediction:

```text
intent bytes = 09 01 01 01
intentFNV    = 96afe901
```

Hardware remains authoritative.

### End-game variant

The legacy special case is explicitly covered:

```text
targetMap=13 / MAP_END_GAME
menuKind=OVERALL
active=1
consumePending=1
```

Static prediction:

```text
endGameFNV = deea91b4
```

The probe also verifies that legacy `MENU_MAP_STATS` and `MENU_MAP_STATS_OVERALL` are distinct, while permanent native code remains independent of their numeric values.

### Direct-load gate

For:

```text
showStats=0
```

this owner is not applicable:

```text
status=NOT_APPLICABLE
intent=all zero
zeroFNV=4b95f515
```

The actual direct-load path remains future transition work.

### Fail closed

The probe requires:

```text
target0=1
target14=1
showStats2=1
nullIntent=1
reset=1
stateAtomic=yes
```

Invalid calls zero any supplied output before returning INVALID.

## RAM / integrity target

Hardware-proven persistent native heap entering this milestone:

```text
18008 B
```

The intent is caller-owned and permanent code allocates nothing:

```text
persistentHeapBytes=0
heap8 delta=0
largest8 delta=0
persistent total remains 18008 B
```

Required unchanged hardware witnesses:

```text
levelExitStatsFNV = bd41bcfa
lineStateFNV      = e5e74861
spriteTopologyFNV = 3f321e43
framebuffer equality
legacy Player exit fields equality
legacy menu/transition equality
PAK closed
legacy Render runtime clear
entities=0
monsters=0
```

No call is made to `Game_changeMap()`, `MenuSystem_setMenu()` or `DoomCanvas_loadMap()`.

## Expected Serial family

```text
[STATSMENUPROBE] ARMED ...

=== Doom RPG ESP32-native stats-menu intent ===
[STATSMENUPROBE] CONTRACT ...
[STATSMENU] READY intentBytes=4 targetMap=9 menuKind=1 active=1 consumePending=1 sourceStatsFNV=bd41bcfa intentFNV=96afe901 ...
[STATSMENU] VARIANTS endGameTarget=13 endGameKind=2 endGameFNV=deea91b4 ... noStatsStatus=1 noStatsZero=1 zeroFNV=4b95f515 repeatExact=1
[STATSMENU] FAILCLOSED target0=1 target14=1 showStats2=1 nullIntent=1 reset=1 stateAtomic=yes
[STATSMENU] RAM ... persistentHeapBytes=0 ... lineFNV=e5e74861 topologyFNV=3f321e43
[STATSMENU] LEGACY ... menuMutation=no Game_changeMapCalled=no mapLoad=no
[STATSMENU] PARK ... nativeStatsMenuIntent=yes intentBytes=4 targetMap=9 menuKind=LEVEL ... entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

Use normal PlatformIO environment `esp32-cyd`.

No CI status is published for the candidate. No local build or real-CYD PASS is claimed.

## Boundary after PASS

A PASS completes semantic native ownership of the show-stats pause point in recovered `Game_changeMap()`:

```text
CHANGEMAP pending intent
 -> exit stats
 -> player exit state
 -> stats-menu intent
```

Still outside:

```text
actual stats-menu rendering/input consumer
generic target map-name -> ID/catalog resolution
transition preflight and lifecycle handoff
actual Junction map swap
full native entity/monster gameplay
native ST_PLAYING loop/rendering
sound playback
```

The next bounded milestone should recover from merged main and establish the target transition/catalog preflight needed before replacing `/intro.bsp` with `/junction.bsp`, without yet performing the destructive map swap.
