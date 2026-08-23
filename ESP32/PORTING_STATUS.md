# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #70 — native player/view owner
main = 8a82891bb8d9c62582170cc4b3b74d270849e77b
hardware-tested firmware = fe1630ad5618dfd35bbbc555de8f9762d0b046f8
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-post-spawn-refresh
base   = 8a82891bb8d9c62582170cc4b3b74d270849e77b
hardware-tested firmware = 1761e2929ec50260a1f27373ce477530b84d041a
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Active evidence: [`MAP1_NATIVE_HUD_REFRESH.md`](MAP1_NATIVE_HUD_REFRESH.md).

This milestone routes the recovered `Hud.isUpdate=true` write into a permanent 8-byte native HUD dirty owner and atomically consumes only the player/view HUD-pending bit. Facing, `Player_setup`, tile-enter and `ST_PLAYING` remain explicitly pending.

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

State:

```text
EspPlayerViewState = 44 B
persistent heap = 0 B
```

Hardware-proven initial Junction owner:

```text
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
hudRefresh=1
facingRefresh=1
playerSetup=1
tileEnter=1
```

## Permanent native HUD dirty owner

Files:

```text
ESP32/include/esp_hud_refresh_state.h
ESP32/src/esp_hud_refresh_state.c
```

API:

```text
EspHudRefresh_reset()
EspHudRefresh_isReady()
EspHudRefresh_view()
EspHudRefresh_preparePostSpawn()
EspHudRefresh_routePostSpawn()
```

State:

```text
EspHudRefreshState = 8 B
persistent heap = 0 B
```

Real-CYD Junction canon:

```text
stateFNV=6965ee06
reason=POST_SPAWN
refreshPending=1
routed=1
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
```

Routing atomically transfers ownership of only the HUD pending bit:

```text
PlayerView beforeFNV=d1131d18
PlayerView afterFNV =d17fa0d1
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
placementExact=yes
```

## Recovered post-spawn ordering

Exact legacy order is:

```text
Game_spawnPlayer:
  placement
  Render.viewZOld=4
  Hud.isUpdate=true
  checkFacingEntity()            # transient, old orientation vectors
  Player_setup()
  Game_executeTile(...)

caller -> finishRotation():
  recalc viewSin/viewCos/viewStep
  Game_executeTile(... | 0x400)
  checkFacingEntity()            # durable final facing
```

The first facing write is transitory and not consumed by `Player_setup()` or the intervening tile execution. Native facing must therefore stay pending until after the correct setup/tile/finishRotation ordering rather than being calculated early from stale or incomplete world state.

## Hardware fail-closed proof for HUD routing

```text
nullView=1
nullOutput=1
inactive=1
loadType=1
missingHud=1
missingFacing=1
reset=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

## Resident / RAM integrity

Real CYD for the HUD milestone:

```text
snapshotFNV=bc9071e9->bc9071e9
targetLeftResident=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes

heap8=72940->72940
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat:

```text
heap=138704
heap8=72940
largest8=34804
```

Absolute heap is build-context dependent; same-build zero delta is the hardware proof.

## Legacy / framebuffer integrity

Same-build witnesses for the HUD milestone:

```text
placementFNV=5d1076bf->5d1076bf
playerFNV=a1725bcb->a1725bcb
frameFNV=7a95b5b5->7a95b5b5
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
8 B post-spawn HUD dirty owner
explicit facing/setup/tile-enter pending boundary
```

Still intentionally outside:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
native Player_setup-equivalent initialization
initial tile-enter execution
finishRotation-equivalent orientation preparation
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The player/view and HUD owners deliberately have different lifetimes from the map-resident arena and are not folded into the seven-owner resident snapshot.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next direction after merge

After merge, recover from true `main` before implementation. The next bounded semantic is the native `Player_setup()` equivalent. Keep initial tile-enter, finishRotation/final-facing and `ST_PLAYING` separate unless the next legacy audit proves a tighter safe boundary.

## Merge recommendation

```text
MERGE agent/esp32-native-post-spawn-refresh
```

Hardware-tested firmware:

```text
1761e2929ec50260a1f27373ce477530b84d041a
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
