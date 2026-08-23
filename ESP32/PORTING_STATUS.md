# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #71 — native post-spawn HUD refresh
main = 02b7f143a12e6df86ada094af10ef580ad572aad
hardware-tested firmware = 1761e2929ec50260a1f27373ce477530b84d041a
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_HUD_REFRESH.md`](MAP1_NATIVE_HUD_REFRESH.md).

## Current hardware candidate

```text
branch = agent/esp32-native-player-setup
base   = 02b7f143a12e6df86ada094af10ef580ad572aad
firmware candidate = d808d895e97daef5d454ca06d5fda1738e99b147
status = IMPLEMENTED / REAL-CYD HARDWARE VALIDATION PENDING
```

Active evidence: [`MAP1_NATIVE_PLAYER_SETUP.md`](MAP1_NATIVE_PLAYER_SETUP.md).

This candidate owns only recovered fresh-map `Player_setup()` semantics. It introduces a permanent 24-byte native per-level session root, consumes only `playerSetupPending`, and leaves initial tile-enter, `finishRotation()`/final-facing and `ST_PLAYING` explicitly pending.

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
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING  = not reached
```

## Hardware-proven map canons

Entrance:

```text
resource=/intro.bsp
bytes=21823 crc32=623f34e4 sourceFNV=d5cc751f
gameplayLoadMapId=1 spawnIndex=904 spawnDirection=64
snapshotFNV=b3811f3d
logical payload=17891 B
actual heap=18008 B
```

Junction:

```text
resourceMapId=9 / /junction.bsp
gameplayLoadMapId=2
sourceBytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
spawnIndex=943
spawnDirection=64
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
entities=30
enemies=0
destructibles=3
```

Junction owner FNVs:

```text
runtime  = bc432a0f
map      = c5cdfc04
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = 0b2ae445
topology = d6e8df7d
```

## Hardware-proven transition/player chain

```text
CHANGEMAP pending intent
 -> level-exit stats
 -> native player exit-state
 -> LEVEL stats-menu semantic intent
 -> immutable 13-map catalog
 -> Junction target preflight
 -> explicit resident lifecycle
 -> reversible Entrance -> Junction -> Entrance
 -> committed transition with post-teardown rollback
 -> real committed Entrance -> Junction residency
 -> fresh-map load semantic
 -> 24 B native Junction spawn projection
 -> 44 B active native player/view owner
 -> 8 B native post-spawn HUD dirty owner
```

Canonical fingerprints:

```text
levelExitStatsFNV        = bd41bcfa
playerExitAppliedFNV     = 298eaaa4
statsMenuIntentFNV       = 96afe901
catalogFNV               = ce322e3f
transitionPreflightFNV   = 108e5c7b
committed WAIT_STATS FNV = 66fe636a
committed READY FNV      = 0ef58ea8
committed ROLLBACK FNV   = 2dec1442
committed COMMITTED FNV  = 2c595a62
Junction spawn FNV       = ba6af4a7
packed override FNV      = e0a5110b
Junction player/view FNV = d1131d18
packed override view FNV = 9ed47d08
post-HUD player/view FNV = d17fa0d1
Junction HUD refresh FNV = 6965ee06
```

All real MAP_INTRO opcode IDs have explicit native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Hardware-proven current player/HUD state

Fresh-map spawn projection:

```text
EspPlayerSpawnState=24 B
stateFNV=ba6af4a7
targetMap=9 gameplayLoadMapId=2
world=992/1888 angle=64 viewZ=36 viewZOld=4 loadType=0
```

Active player/view before HUD routing:

```text
EspPlayerViewState=44 B
stateFNV=d1131d18
hudRefreshPending=1
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

Hardware-proven post-HUD state:

```text
EspHudRefreshState=8 B
HUD FNV=6965ee06
PlayerView FNV=d17fa0d1
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

HUD milestone RAM proof:

```text
snapshotFNV=bc9071e9->bc9071e9
heap8=72940->72940
largest8=34804->34804
persistentHeapBytes=0
packClosed=yes
```

Same-build HUD witnesses only:

```text
placementFNV=5d1076bf->5d1076bf
playerFNV=a1725bcb->a1725bcb
frameFNV=7a95b5b5->7a95b5b5
```

## Recovered fresh-map ordering

Exact legacy order is:

```text
Game_spawnPlayer:
  placement
  Render.viewZOld=4
  Hud.isUpdate=true
  checkFacingEntity()            # transient, previous orientation vectors
  Player_setup()
  Game_executeTile(...)          # initial tile execution

caller -> DoomCanvas_finishRotation():
  recalc viewSin/viewCos/viewStep
  Game_executeTile(... | 0x400)
  checkFacingEntity()            # durable final facing
```

The first facing write is transitory and is not consumed by `Player_setup()` or the intervening tile execution. Native facing remains pending until after the correct setup/tile/orientation ordering.

## Current candidate: native fresh-map Player_setup

Permanent files:

```text
ESP32/include/esp_player_fresh_map_state.h
ESP32/src/esp_player_fresh_map_state.c
```

Expected ABI:

```text
EspPlayerFreshMapState = 24 B
persistent heap = 0 B
```

Supported fresh-map result:

```text
levelStartTimeMs=<sampled runtime value>
moves=0
xpGained=0
berserkerTics=0
familiarActive=0
notebookEmpty=1
weaponRestorePerformed=0
targetMapId=9
gameplayLoadMapId=2
loadType=0
setupApplied=1
active=1
```

`levelStartTimeMs` is dynamic. Cross-run proof uses a normalized FNV with only that field zeroed:

```text
candidate setup semanticFNV = 3b27c6a1
```

The player/view ABI remains 44 B. Candidate ownership transfer:

```text
before FNV=d17fa0d1
 after FNV=c21fba3c
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=1
```

`c21fba3c` and `3b27c6a1` are predictions until real-CYD confirmation.

### Weapon restore boundary

Recovered `Player_setup()` calls `Player_restoreWeapons()` only when `disabledWeapons != 0`. That branch can mutate weapons, select another weapon, and request a view refresh. The current milestone therefore requires:

```text
disabledWeapons=0
```

A nonzero value returns `ESP_PLAYER_FRESH_MAP_WEAPON_RESTORE_DEFERRED` before any native mutation. The probe reads the real legacy value as a hardware precondition; permanent APIs remain legacy-free.

### Candidate fail-closed matrix

```text
nullView
nullHud
nullOutput
inactive
loadType
hudPending
missingFacing
missingSetup
missingTile
hudMismatch
weaponRestore
reset
prepareAtomic
repeat
repeatAtomic
```

All must pass on hardware.

## Current hardware PARK before candidate

```text
state=9 / ST_INTRO
page=3
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativeHudRefresh=yes
hudDirty=yes
hudRouted=yes
hudRefreshPending=0
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Expected PARK after candidate PASS

```text
state=9 / ST_INTRO
page=3
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativeHudRefresh=yes
nativePlayerSetup=yes
setupApplied=yes
hudDirty=yes
facingPending=yes
playerSetupPending=no
tileEnterPending=yes
finishRotationPending=yes
finalFacingPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Architecture boundary

Hardware-proven now:

```text
compact immutable native map + explicit mutable owners
all MAP_INTRO opcode families
exit/save/change-map chain
Junction catalog/preflight
resident lifecycle
transactional committed map swap
fresh-map spawn/load projection
active native player/view owner
post-spawn HUD dirty ownership
```

Current candidate adds:

```text
fresh-map Player_setup-equivalent per-level session root
```

Still outside:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
weapon-restore/select ownership for disabledWeapons!=0
initial tile-enter execution
finishRotation-equivalent orientation preparation
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next direction after hardware PASS + merge

The next exact operation is initial tile-enter execution at the hardware-proven Junction player position `(992,1888)`. Do not fold `finishRotation()`, the second tile execution, durable facing, and `ST_PLAYING` into that milestone without a fresh legacy audit.

## Merge recommendation

```text
DO NOT MERGE YET — hardware validation pending
```

Firmware candidate:

```text
d808d895e97daef5d454ca06d5fda1738e99b147
```

Every later commit before hardware validation must remain documentation-only unless a compile/probe failure requires a new firmware candidate.
