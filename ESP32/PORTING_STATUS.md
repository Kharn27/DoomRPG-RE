# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #69 — native Junction spawn projection
main = 992f38374840113409e776fb82ce57ab014607e5
hardware-tested firmware = 08a3a29c5e4e4a64000fa12a877299bbb1e772a0
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-player-view
base   = 992f38374840113409e776fb82ce57ab014607e5
hardware-tested firmware = fe1630ad5618dfd35bbbc555de8f9762d0b046f8
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Active evidence: [`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md).

This milestone introduces the first permanent active native player/view owner. It applies the already hardware-proven Junction spawn projection to native state only while keeping HUD refresh, facing lookup, Player_setup, tile-enter and ST_PLAYING explicitly pending.

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
```

All real MAP_INTRO opcode IDs have explicit native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Hardware-proven fresh-map spawn

Permanent projection:

```text
EspPlayerSpawnState = 24 B
persistent heap = 0 B
```

Real Junction:

```text
stateFNV=ba6af4a7
targetMap=9
gameplayLoadMapId=2
spawnParam=00000000
source=HEADER
tileIndex=943
tile=15/29
world=992/1888
angle=64
viewZ=36
viewZOld=4
loadType=0
```

Packed override canon:

```text
spawnParam=00030167
tile=7/11
world=480/736
angle=192
stateFNV=e0a5110b
```

## Permanent native player/view owner

Files:

```text
ESP32/include/esp_player_view_state.h
ESP32/src/esp_player_view_state.c
```

API:

```text
EspPlayerView_reset()
EspPlayerView_isReady()
EspPlayerView_view()
EspPlayerView_applySpawn()
```

State:

```text
EspPlayerViewState = 44 B
persistent heap = 0 B
```

The owner keeps legacy-width placement fields:

```text
viewX/viewY/viewZ/viewAngle
destX/destY/destAngle
viewZOld
```

plus target/load identity and explicit pending follow-ups.

Recovered `Game_spawnPlayer()` writes `Hud.isUpdate=true` after placement and before facing lookup. Native state therefore owns:

```text
hudRefreshPending=1
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

without mutating the legacy HUD.

## Hardware-proven native player/view application

Real CYD:

```text
stateBytes=44
stateFNV=d1131d18
view=992/1888/36
viewAngle=64
dest=992/1888
destAngle=64
viewZOld=4
targetMap=9
gameplayLoadMapId=2
loadType=0
active=1
spawnApplied=1
```

Pending semantics:

```text
hudRefresh=1
facingRefresh=1
playerSetup=1
tileEnter=1
hudApplied=no
facingApplied=no
playerSetupApplied=no
tileEnterApplied=no
```

Packed override application:

```text
param=00030167
view=480/736/36
angle=192
dest=480/736
stateFNV=9ed47d08
sourceProjectionFNV=e0a5110b
```

## Hardware fail-closed proof

```text
nullSpawn=1
inactive=1
badGeometry=1
badPending=1
repeat=1
repeatAtomic=yes
reset=1
stateAtomic=yes
```

The active-owner once-only rule is hardware-proven.

## Resident / RAM integrity

Real CYD:

```text
snapshotFNV=bc9071e9->bc9071e9
targetLeftResident=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes

heap8=72956->72956
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat after the probe:

```text
heap=138720
heap8=72956
largest8=34804
```

Absolute heap is build-context dependent; the player/view milestone adds no heap allocation.

## Legacy / framebuffer integrity

Same-build witnesses:

```text
placementFNV=5d1076bf->5d1076bf
playerFNV=a1725bcb->a1725bcb
frameFNV=a3e3cc8e->a3e3cc8e
legacyRuntimeClear=yes
DoomCanvasMutation=no
GameMutation=no
PlayerMutation=no
RenderMutation=no
HudMutation=no
```

These hashes are same-build witnesses, not cross-build canons.

## Final hardware PARK

```text
state=9 / ST_INTRO
page=3
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativeSpawnState=yes
nativePlayerView=yes
spawnAppliedNative=yes
legacySpawnApplied=no
hudRefreshPending=yes
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Current architecture boundary

Hardware-proven native ownership now includes:

```text
compact immutable native map + explicit mutable owners
all MAP_INTRO opcode families
exit/save/change-map chain
Junction catalog/preflight
resident lifecycle
transactional committed map swap
Junction compact topology
fresh-map load semantic
24 B native spawn projection
44 B active native player/view owner
explicit HUD/facing/setup/tile-enter pending boundary
```

Still intentionally outside:

```text
actual stats-menu rendering/input
HUD refresh consumption
native facing-entity query
Player_setup-equivalent fresh-map initialization
initial tile-enter execution
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The player/view owner is intentionally not reset by `EspMapResidentLifecycle_resetAll()`: player state and map arena have different lifetimes. A future transition orchestrator must explicitly reset/reapply player placement.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next direction after merge

After merge, recover from true `main` before choosing the next milestone. The natural next small boundaries are explicit HUD-refresh consumption and compact-topology facing lookup. Do not collapse Player_setup, tile-enter and ST_PLAYING into one milestone without a fresh legacy audit.

## Merge recommendation

```text
MERGE agent/esp32-native-player-view
```

Hardware-tested firmware:

```text
fe1630ad5618dfd35bbbc555de8f9762d0b046f8
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
