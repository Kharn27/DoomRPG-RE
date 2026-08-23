# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #64 — native player exit-state
main = 3759bcd12a3f6d36a6a696457110ab27474c24b8
hardware-tested firmware = f8c5a1c398c0946025aef976f7a997589bae4923
```

Merged evidence: [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md).

## Current candidate

```text
branch = agent/esp32-native-stats-menu-intent
base   = 3759bcd12a3f6d36a6a696457110ab27474c24b8
firmware candidate = 1dddbe86788389400d6e2186595174e723c72f5c
status = IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md).

This milestone owns the semantic `MENU_MAP_STATS` / `MENU_MAP_STATS_OVERALL` branch intent of recovered `Game_changeMap()` without mutating legacy Menu/Game/Render state or loading a map.

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
entities    = 0
monsters    = 0
ST_PLAYING  = not reached
```

## MAP_INTRO identity

```text
/intro.bsp / Entrance
bytes=21823 crc32=623f34e4 loadMapId=1
nodes=223 lines=480 mapSprites=344 events=93 byteCodes=265
strings=94 stringData=7779 maxString=313
```

All real MAP_INTRO opcode IDs have native ownership/execution boundaries:

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

Level-exit stats, player exit-state/result and current stats-menu intent are caller-owned values. Current candidate target remains `18008 B` persistent heap.

## Hardware-proven fingerprints

```text
arenaFNV                 = c3882516
mapStateFNV              = cd99b98e
scriptFNV                = f9e3d9df
lineStateFNV             = e5e74861
lineTextureStateFNV      = f1fc1875
automapStateFNV          = 669b1aa7
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
playerExitInitialFNV     = 940b0171
playerExitAppliedFNV     = 298eaaa4
playerExitResultFNV      = 5d10a566
playerExitAllMasksFNV    = c93e8128
playerExitLiveFNV        = 57fce418
```

Latest same-build legacy witnesses from PR #64:

```text
playerExitLegacyFNV = f5cbf9f5
transitionFNV       = f450c49f
```

## Hardware-proven CHANGEMAP target

Real MAP_INTRO command:

```text
name=/junction.bsp
targetMap=9 / MAP_JUNCTION
spawnParam=0
showStats=1
effects=03
pending=1
```

Recovered legacy `Game_changeMap()` show-stats branch:

```text
Player_addLevelStats(player, true)
menu->mapNameId = targetMapId
if targetMapId == MAP_END_GAME:
    MenuSystem_setMenu(MENU_MAP_STATS_OVERALL)
else:
    MenuSystem_setMenu(MENU_MAP_STATS)
game->changeMapParam = 0
```

No target map load occurs in this branch before the stats UI.

## Hardware-proven level-exit consumer chain

Native level-exit stats:

```text
EspMapLevelExitStats = 20 B
loadMapId=1 showStats=1
secrets=0/6 monsters=0/30
completionLevelBit=00000001
effects=1f
statsFNV=bd41bcfa
persistentHeapBytes=0
```

Native player exit state:

```text
EspPlayerExitState       = 28 B
EspPlayerExitApplyResult = 28 B
initialFNV=940b0171
appliedFNV=298eaaa4
resultFNV=5d10a566
allMasksFNV=c93e8128
persistentHeapBytes=0
```

It stores no Entity/familiar pointer and leaves legacy Player unchanged.

## Current permanent stats-menu intent API

Files:

```text
ESP32/include/esp_stats_menu_intent.h
ESP32/src/esp_stats_menu_intent.c
```

ABI target:

```text
EspStatsMenuIntent = 4 B
```

Fields:

```text
targetMapId
menuKind       # NONE=0, LEVEL=1, OVERALL=2
active
consumePending
```

API:

```text
EspStatsMenuIntent_reset(intent)
EspStatsMenuIntent_prepare(targetMapId, showStats, intent)
```

Permanent semantics:

```text
showStats=0 -> NOT_APPLICABLE + zero intent
showStats=1,target!=13 -> LEVEL
showStats=1,target==13 -> OVERALL
success -> active=1 consumePending=1
```

Permanent code has no dependency on legacy `Menu_t`, `MenuSystem_t`, `Game_t`, `Render_t` or `DoomCanvas_t`, performs no PAK/ZIP I/O and allocates nothing.

Target map-name -> ID resolution is intentionally not owned here; it remains future transition/catalog work. Real MAP_INTRO target ID `9` is already hardware-proven.

## Candidate real-CYD proof target

Expected real Junction projection:

```text
intentBytes=4
targetMap=9
menuKind=1 / LEVEL
active=1
consumePending=1
sourceStatsFNV=bd41bcfa
intentFNV=96afe901   # static prediction; hardware final truth
```

End-game branch proof:

```text
targetMap=13 / MAP_END_GAME
menuKind=2 / OVERALL
endGameFNV=deea91b4  # static prediction
```

Direct-load gate:

```text
showStats=0
status=NOT_APPLICABLE
intent all zero
zeroFNV=4b95f515     # static prediction
```

Fail closed target:

```text
target0=1
target14=1
showStats2=1
nullIntent=1
reset=1
stateAtomic=yes
```

RAM/integrity target:

```text
persistentHeapBytes=0
heap8 delta=0
largest8 delta=0
framebuffer unchanged
lineStateFNV=e5e74861
spriteTopologyFNV=3f321e43
legacy Player unchanged
legacy menu/transition unchanged
PAK closed
legacy Render runtime clear
entities=0 monsters=0
```

## Validation

Build/flash normal environment:

```text
esp32-cyd
```

Branch / firmware:

```text
agent/esp32-native-stats-menu-intent
1dddbe86788389400d6e2186595174e723c72f5c
```

Capture `[STATSMENUPROBE]`, `[STATSMENU]` and stable `[ALIVE]` lines.

No CI status is published for the candidate. No local build or hardware PASS is claimed.

## Current architecture boundary

Hardware-proven ownership through PR #64:

```text
compact immutable native map + explicit mutable owners
all 16 real MAP_INTRO opcode families
SAVEGAME durable route
CHANGEMAP pending transition intent
SHOW/HIDE compact sprite/entity topology
native level-exit stats snapshot
native player exit-state application
```

Candidate adds:

```text
native stats-menu semantic intent / pending-consume projection
```

Still intentionally outside:

```text
actual stats-menu rendering/input consumer
generic map target catalog/name resolution
transition preflight / source-target lifecycle handoff
actual CHANGEMAP / Junction map swap
full native entity/monster gameplay
legacy-world-free gameplay loop
native gameplay renderer
ST_PLAYING progression
sound playback
```

Do not merge the candidate until the exact firmware above passes on the real classic CYD and every later commit remains documentation-only.
