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

## Current hardware candidate

```text
branch = agent/esp32-native-initial-tile-enter
base   = 9077ae4496bdcc06b6b99846332ab43b38943a8a
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

Candidate evidence/design: [`MAP1_NATIVE_INITIAL_TILE_ENTER.md`](MAP1_NATIVE_INITIAL_TILE_ENTER.md).

The candidate owns only the first fresh-map `Game_executeTile()` dispatch after hardware-proven `Player_setup()`. It keeps `finishRotation()`, the second tile event, durable facing and `ST_PLAYING` explicitly pending. If Junction tile 943 exposes an eligible opcode outside the existing tiny 11/19/20 executor, the probe must stop fail-closed and print the exact command rather than falling back to legacy execution.

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
 -> 24 B native fresh-map Player_setup session owner
```

Canonical fingerprints:

```text
levelExitStatsFNV          = bd41bcfa
playerExitAppliedFNV       = 298eaaa4
statsMenuIntentFNV         = 96afe901
catalogFNV                 = ce322e3f
transitionPreflightFNV     = 108e5c7b
committed WAIT_STATS FNV   = 66fe636a
committed READY FNV        = 0ef58ea8
committed ROLLBACK FNV     = 2dec1442
committed COMMITTED FNV    = 2c595a62
Junction spawn FNV         = ba6af4a7
packed override FNV        = e0a5110b
Junction player/view FNV   = d1131d18
packed override view FNV   = 9ed47d08
post-HUD player/view FNV   = d17fa0d1
Junction HUD refresh FNV   = 6965ee06
Player_setup semantic FNV  = 3b27c6a1
post-setup player/view FNV = c21fba3c
```

All real MAP_INTRO opcode IDs already have explicit native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

Important distinction: the generic `EspMapOpcodeExecutor` deliberately remains tiny and currently executes only IDs 11/19/20. Other opcode families have dedicated owners/probes and must not be silently wired into a generic event path without a dedicated milestone.

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

Active player/view before HUD routing:

```text
EspPlayerViewState=44 B
stateFNV=d1131d18
hudRefreshPending=1
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

Post-HUD:

```text
EspHudRefreshState=8 B
HUD FNV=6965ee06
PlayerView FNV=d17fa0d1
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

Post-Player_setup — latest merged hardware boundary:

```text
EspPlayerFreshMapState=24 B
setup semanticFNV=3b27c6a1
raw same-run stateFNV=d0ab146e at startMs=27538
startExact=yes
moves=0
xpGained=0
berserkerTics=0
familiarActive=0
notebookEmpty=1
weaponRestorePerformed=0

PlayerView FNV=c21fba3c
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=1
```

`d0ab146e` is a same-run witness only because `levelStartTimeMs` is dynamic. `3b27c6a1` is the normalized hardware canon.

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

## Initial tile candidate contract

Recovered legacy call at the current real Junction spawn:

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

Permanent candidate files:

```text
ESP32/include/esp_player_initial_tile.h
ESP32/src/esp_player_initial_tile.c
```

Candidate owner:

```text
EspPlayerInitialTileState = 24 B
persistent heap = 0 B
```

Candidate API:

```text
EspPlayerInitialTile_reset()
EspPlayerInitialTile_isReady()
EspPlayerInitialTile_view()
EspPlayerInitialTile_prepare()
EspPlayerInitialTile_route()
EspPlayerView_consumeTileEnter()
```

`prepare()` is side-effect free and preflights the full filtered command set. The route executes only already-supported 11/19/20 state mutations, maps recovered MCODE_FLAG_REMOVE/`arg2 & 0x200` into `EspMapScriptState`, rolls script state back on failure, then consumes only `tileEnterPending`.

Any eligible unsupported opcode returns:

```text
ESP_PLAYER_INITIAL_TILE_OPCODE_DEFERRED
```

with no player/script mutation. The hardware probe prints the exact real opcode/args so the next family can be implemented deliberately.

Predicted one-bit post-route player/view fingerprint:

```text
post-initial-tile PlayerView FNV = 1bd0f09b
```

This is **not yet a hardware canon**.

## Last hardware RAM / integrity baseline

Player_setup milestone on real CYD:

```text
snapshotFNV=bc9071e9->bc9071e9
targetLeftResident=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes

heap8=72900->72900
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat:

```text
heap=138664
heap8=72900
largest8=34804
```

Legacy/framebuffer same-build witnesses from that PASS:

```text
placementFNV=5d1076bf->5d1076bf
playerSetupFNV=ea247b9a->ea247b9a
frameFNV=9eb7ce0f->9eb7ce0f
legacyRuntimeClear=yes
DoomCanvasMutation=no
GameMutation=no
PlayerMutation=no
RenderMutation=no
HudMutation=no
```

## Latest merged hardware PARK

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

## Candidate hardware outcomes

The normal `esp32-cyd` firmware chains `native_junction_initial_tile_probe` after the proven Player_setup probe.

A complete candidate PASS must reach:

```text
[JUNCTIONTILE] READY ...
PlayerView c21fba3c -> 1bd0f09b
tileEnterPending=0
facingRefreshPending=1
finishRotationPending=yes
ST_PLAYING=no
```

and prove zero same-build heap delta plus no legacy/framebuffer mutation. Script FNV may change only if an eligible supported state opcode executes; immutable runtime and all non-script resident owners must remain stable.

If the real event contains a currently unsupported eligible opcode, the correct result is instead:

```text
[JUNCTIONTILE] DEFERRED ... code=<id> arg1=<...> arg2=<...> failClosed=yes
```

with `tileEnterPending=1`. That is discovery, not tile-enter PASS; implement that exact opcode family in a bounded follow-up before promoting this milestone.

## Current architecture boundary

Hardware-proven native ownership through merged `main` includes:

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
```

Candidate only:

```text
bounded first fresh-map tile dispatch
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

## Next action

Build/flash the normal environment:

```text
esp32-cyd
```

Use the exact `[JUNCTIONTILE]` Serial block as hardware truth. Do not mark this candidate merge-ready until a complete native route is proven; a deferred-opcode discovery must be implemented and re-tested first.
