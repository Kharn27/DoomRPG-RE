# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #72 — native fresh-map Player_setup
main = 9077ae4496bdcc06b6b99846332ab43b38943a8a
hardware-tested firmware = d808d895e97daef5d454ca06d5fda1738e99b147
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_PLAYER_SETUP.md`](MAP1_NATIVE_PLAYER_SETUP.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-initial-tile-enter
base   = 9077ae4496bdcc06b6b99846332ab43b38943a8a
hardware-tested firmware = d8fb3e0e372b89d95c37cce558420f7fcb474419
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_INITIAL_TILE_ENTER.md`](MAP1_NATIVE_INITIAL_TILE_ENTER.md).

This milestone hardware-proves the first fresh-map `Game_executeTile()` dispatch at Junction and consumes only `tileEnterPending`. `finishRotation()`, its second tile event, durable facing and `ST_PLAYING` remain explicitly pending.

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
bytes=21823
crc32=623f34e4
sourceFNV=d5cc751f
gameplayLoadMapId=1
spawnIndex=904
spawnDirection=64
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

Junction resident owner FNVs:

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
 -> 24 B native fresh-map Player_setup session owner
 -> 24 B native initial tile-enter owner
```

Canonical fingerprints:

```text
levelExitStatsFNV              = bd41bcfa
playerExitAppliedFNV           = 298eaaa4
statsMenuIntentFNV             = 96afe901
catalogFNV                     = ce322e3f
transitionPreflightFNV         = 108e5c7b
committed WAIT_STATS FNV       = 66fe636a
committed READY FNV            = 0ef58ea8
committed ROLLBACK FNV         = 2dec1442
committed COMMITTED FNV        = 2c595a62
Junction spawn FNV             = ba6af4a7
packed override FNV            = e0a5110b
Junction player/view FNV       = d1131d18
packed override view FNV       = 9ed47d08
post-HUD player/view FNV       = d17fa0d1
Junction HUD refresh FNV       = 6965ee06
Player_setup semantic FNV      = 3b27c6a1
post-setup player/view FNV     = c21fba3c
Junction initial-tile FNV      = f73e28b2
post-initial-tile player FNV   = 1bd0f09b
```

All real MAP_INTRO opcode IDs already have explicit native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

The generic `EspMapOpcodeExecutor` deliberately remains tiny and executes only IDs 11/19/20. Other opcode families keep dedicated owners/probes.

## Hardware-proven current player state

Fresh-map spawn projection:

```text
EspPlayerSpawnState=24 B
stateFNV=ba6af4a7
targetMap=9
gameplayLoadMapId=2
world=992/1888
angle=64
viewZ=36
viewZOld=4
loadType=0
```

Post-Player_setup:

```text
EspPlayerFreshMapState=24 B
setup semanticFNV=3b27c6a1
PlayerView FNV=c21fba3c
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=1
```

Latest hardware boundary — post initial tile-enter:

```text
EspPlayerInitialTileState=24 B
stateFNV=f73e28b2
world=992/1888
tile=943
flags=1000040f
eventFound=1
eventIndex=61
eventState=0
eventFlags=0
blocked=0
eligible=0
executed=0
removed=0
skipAdvanceTurn=0
playerKeys=00000000

PlayerView FNV c21fba3c -> 1bd0f09b
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=0
```

The real tile 943 event exists, but no command is eligible under the exact fresh-map flags. Therefore script state remains byte-for-byte unchanged:

```text
script FNV bc9b18ff -> bc9b18ff
changed=no
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

Hardware-proven boundary now reaches through the first `Game_executeTile()` call. The transient first facing write remains deliberately unowned because it is overwritten later and is not consumed by Player_setup or the initial tile dispatch.

## Initial tile-enter hardware proof

Recovered call:

```text
worldX=992
worldY=1888
tileX=15
tileY=29
tileIndex=943
destAngle=64
DoomCanvas_flagForFacingDir(64)=0x10000000
base flags=0x0000040f
input flags=0x1000040f
```

Permanent API:

```text
EspPlayerInitialTile_reset()
EspPlayerInitialTile_isReady()
EspPlayerInitialTile_view()
EspPlayerInitialTile_prepare()
EspPlayerInitialTile_route()
EspPlayerView_consumeTileEnter()
```

Hardware fail-closed gates:

```text
nullView=1
nullSetup=1
nullOutput=1
angle=1
blocked=1
missingTile=1
missingFacing=1
setupMismatch=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

No fallback to legacy `Game_executeEvent()` is permitted. Eligible unsupported commands would still fail closed; the real initial Junction dispatch simply has `eligible=0`.

## Latest hardware RAM / integrity baseline

Initial tile-enter milestone on real CYD:

```text
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
runtimeStable=yes
nonScriptMutableStable=yes

heap8=72868->72868
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat:

```text
heap=138632
heap8=72868
largest8=34804
```

Same-build equality witnesses:

```text
gameFNV=c655ff85->c655ff85
playerFNV=774ed642->774ed642
frameFNV=7a95b5b5->7a95b5b5
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

These equality FNVs are same-build witnesses, not cross-build canons.

## Current hardware PARK

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativePlayerSetup=yes
nativeInitialTile=yes
tileEnterPending=no
facingPending=yes
finishRotationPending=yes
secondTilePending=yes
finalFacingPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Current architecture boundary

Hardware-proven native ownership now includes:

```text
compact immutable native map + explicit mutable owners
all MAP_INTRO opcode families as dedicated semantic boundaries
exit/save/change-map chain
Junction catalog/preflight
resident lifecycle
transactional committed map swap
fresh-map spawn/load projection
active native player/view owner
post-spawn HUD dirty ownership
fresh-map Player_setup-equivalent per-level session root
first fresh-map tile dispatch
```

Still intentionally outside:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
weapon restore/select ownership when disabledWeapons!=0
finishRotation-equivalent orientation preparation
second tile event
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The player/view, HUD, fresh-map session and initial-tile owners have lifetimes distinct from the seven-owner resident arena.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next bounded milestone after merge

Recover from the true post-merge `main` before implementation. The natural next operation is the `DoomCanvas_finishRotation()` orientation preparation (`viewSin`, `viewCos`, `viewStepX`, `viewStepY`). Keep its second `Game_executeTile(... | 0x400)` and final durable facing as later boundaries unless a fresh legacy audit proves a tighter safe grouping.

## Merge recommendation

```text
MERGE agent/esp32-native-initial-tile-enter
```

Hardware-tested firmware:

```text
d8fb3e0e372b89d95c37cce558420f7fcb474419
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
