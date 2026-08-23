# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #74 — native finishRotation orientation
main = 2decae5067438dc1a2d9c29335cfc0cad5538645
hardware-tested firmware = 850f1651db7ca1943d5647a00099dfb48c9de284
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md`](MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md).

## Current hardware candidate

```text
branch = agent/esp32-native-finish-rotation-second-tile
base   = 2decae5067438dc1a2d9c29335cfc0cad5538645
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

Candidate evidence/design: [`MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md`](MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md).

This candidate owns only the second `Game_executeTile()` inside recovered
`DoomCanvas_finishRotation()`. Final durable facing and `ST_PLAYING` remain
explicitly pending. Unsupported eligible opcodes must fail closed and be
reported exactly; no legacy event fallback is permitted.

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
 -> 24 B native finishRotation orientation owner
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
Junction orientation FNV       = acc754a6
```

All real MAP_INTRO opcode IDs already have explicit native ownership/execution
boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

The generic `EspMapOpcodeExecutor` deliberately remains tiny and executes only
IDs 11/19/20. Other opcode families keep dedicated owners/probes.

## Hardware-proven current player/orientation boundary

Post initial tile-enter:

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

PlayerView FNV=1bd0f09b
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=0
```

The first tile event exists but has no eligible command under `0x1000040f`, so
script state remains `bc9b18ff`.

Latest merged hardware boundary — finishRotation orientation preparation:

```text
EspPlayerOrientationState=24 B
stateFNV=acc754a6
destAngle=64
viewSin=65536
viewCos=0
viewStepX=0
viewStepY=-64
legacySin=65536
legacyCos=0
legacyStepX=0
legacyStepY=-64
exact=yes
prepared=1
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
```

PlayerView remains:

```text
1bd0f09b -> 1bd0f09b
unchanged=yes
facingRefreshPending=1
```

## Recovered fresh-map ordering

```text
Game_spawnPlayer:
  placement
  Render.viewZOld=4
  Hud.isUpdate=true
  checkFacingEntity()            # transient previous-vector result
  Player_setup()
  Game_executeTile(...)          # first tile dispatch

caller -> DoomCanvas_finishRotation():
  viewSin/viewCos/viewStepX/viewStepY  [hardware-proven]
  Game_executeTile(... | 0x400)        [CURRENT CANDIDATE]
  checkFacingEntity()                  [deferred]
```

At the current Junction path the second call is exactly:

```text
world=992/1888
tile=943
destAngle=64
facing flag=0x10000000
input flags=0x10000400
```

## Current second-tile candidate

Permanent files:

```text
ESP32/include/esp_player_finish_rotation_tile.h
ESP32/src/esp_player_finish_rotation_tile.c
```

Candidate owner:

```text
EspPlayerFinishRotationTileState = 24 B
persistent heap = 0 B
```

Candidate API:

```text
EspPlayerFinishRotationTile_reset()
EspPlayerFinishRotationTile_isReady()
EspPlayerFinishRotationTile_view()
EspPlayerFinishRotationTile_prepare()
EspPlayerFinishRotationTile_route()
```

The route requires the canonical PlayerView/InitialTile/Orientation owners,
performs a complete side-effect-free filtered-command preflight, executes only
already-supported 11/19/20 state commands, maps recovered `arg2 & 0x200` removal
into `EspMapScriptState`, rolls all script changes back on failure and never
mutates PlayerView/InitialTile/Orientation.

Any eligible unsupported command returns:

```text
ESP_PLAYER_FINISH_ROTATION_TILE_OPCODE_DEFERRED
```

with exact opcode/command diagnostics and no mutation.

No candidate second-tile FNV is promoted before the real event eligibility is
observed on hardware.

Temporary probe:

```text
ESP32/include/native_junction_finish_rotation_tile_probe.h
ESP32/src/native_junction_finish_rotation_tile_probe.c
```

Complete route marker:

```text
[JUNCTIONTILE2] READY ...
```

Fail-closed discovery marker:

```text
[JUNCTIONTILE2] DEFERRED ... code=<id> arg1=<...> arg2=<...> failClosed=yes
```

A DEFERRED result is discovery, not milestone PASS.

## Latest hardware RAM / integrity baseline

Orientation milestone on real CYD:

```text
snapshotFNV=bc9071e9->bc9071e9
unchanged=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes

heap8=72828->72828
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat:

```text
heap=138592
heap8=72828
largest8=34804
```

Same-build equality witnesses:

```text
gameFNV=ea6207e5->ea6207e5
playerFNV=2c811802->2c811802
canvasFNV=1b7ba23f->1b7ba23f
renderFNV=f9344dec->f9344dec
frameFNV=faa62417->faa62417
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

The implementation performs no HUD write; no separate HUD fingerprint is
promoted by the orientation probe.

## Current hardware PARK

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativeInitialTile=yes
nativeOrientation=yes
orientationPending=no
secondTilePending=yes
finalFacingPending=yes
finishRotationComplete=no
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Current architecture boundary

Hardware-proven native ownership includes:

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
fresh-map Player_setup-equivalent session root
first fresh-map tile dispatch
finishRotation orientation preparation
```

Candidate only:

```text
second finishRotation tile dispatch
```

Still intentionally outside:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
weapon restore/select ownership when disabledWeapons!=0
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The player/view, HUD, fresh-map session, initial-tile, orientation and second-tile
owners have lifetimes distinct from the seven-owner resident arena.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next action

Build/flash the normal environment:

```text
esp32-cyd
```

Use the exact `[JUNCTIONTILE2]` Serial block as hardware truth. Do not mark this
candidate merge-ready until a complete native route is proven. If the probe
reports `DEFERRED`, implement that exact opcode family/integration and re-test
before promotion.
