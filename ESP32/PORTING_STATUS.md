# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #73 — native initial tile-enter
main = 0bc171affad8416ed1a7918a4a67fd4d53d61efe
hardware-tested firmware = d8fb3e0e372b89d95c37cce558420f7fcb474419
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_INITIAL_TILE_ENTER.md`](MAP1_NATIVE_INITIAL_TILE_ENTER.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-finish-rotation-orientation
base   = 0bc171affad8416ed1a7918a4a67fd4d53d61efe
hardware-tested firmware = 850f1651db7ca1943d5647a00099dfb48c9de284
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md`](MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md).

This milestone owns only the four orientation writes at the start of recovered `DoomCanvas_finishRotation()`. The second tile dispatch, final durable facing and `ST_PLAYING` remain explicitly pending.

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

All real MAP_INTRO opcode IDs already have explicit native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

The generic `EspMapOpcodeExecutor` deliberately remains tiny and executes only IDs 11/19/20. Other opcode families keep dedicated owners/probes.

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

The first tile event exists but has no eligible command under the exact fresh-map flags, so script state remains `bc9b18ff`.

Latest hardware boundary — finishRotation orientation preparation:

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

PlayerView remains exactly:

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
  viewSin   = sinTable[destAngle & 255]      [hardware-proven]
  viewCos   = sinTable[(destAngle + 64)&255] [hardware-proven]
  viewStepX = (viewCos * 64) >> 16           [hardware-proven]
  viewStepY = ((-viewSin) * 64) >> 16        [hardware-proven]
  Game_executeTile(... | 0x400)              [next]
  checkFacingEntity()                        [deferred]
```

The transient first facing result remains deliberately unowned.

## Orientation fail-closed proof

```text
nullView=1
nullTile=1
nullOutput=1
inactive=1
tilePending=1
missingFacing=1
angle=1
tileInactive=1
tileMismatch=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

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

The implementation performs no HUD write; this probe does not materialize a separate HUD fingerprint, so none is promoted as a canon.

These equality FNVs are same-build witnesses, not cross-build canons.

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

Still intentionally outside:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
weapon restore/select ownership when disabledWeapons!=0
second finishRotation tile dispatch
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The player/view, HUD, fresh-map session, initial-tile and orientation owners have lifetimes distinct from the seven-owner resident arena.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next bounded milestone after merge

Recover from the exact post-merge `main`. The next operation is the second `Game_executeTile()` inside `DoomCanvas_finishRotation()`:

```text
world=992/1888
tile=943
destAngle=64
DoomCanvas_flagForFacingDir(64)=0x10000000
input flags=0x10000400
```

Keep final durable `checkFacingEntity()` and `ST_PLAYING` separate unless a fresh audit proves a tighter safe boundary.

## Merge recommendation

```text
MERGE agent/esp32-native-finish-rotation-orientation
```

Hardware-tested firmware:

```text
850f1651db7ca1943d5647a00099dfb48c9de284
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
