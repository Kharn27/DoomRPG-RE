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

## Current merge-ready milestone

```text
branch = agent/esp32-native-finish-rotation-second-tile
base   = 2decae5067438dc1a2d9c29335cfc0cad5538645
hardware-tested firmware = df4f62687d99eb3b3e9569ae6861b6909d59c82d
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md`](MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md).

This milestone hardware-proves the second `Game_executeTile()` inside recovered `DoomCanvas_finishRotation()`. Final durable facing and `ST_PLAYING` remain explicitly pending.

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
 -> 24 B native finishRotation second-tile owner
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
Junction second-tile FNV       = 09e58e0d
```

All real MAP_INTRO opcode IDs already have explicit native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

The generic `EspMapOpcodeExecutor` deliberately remains tiny and executes only IDs 11/19/20. Other opcode families keep dedicated owners/probes.

## Hardware-proven current player / finishRotation boundary

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

FinishRotation orientation:

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
```

Latest hardware boundary — second finishRotation tile dispatch:

```text
EspPlayerFinishRotationTileState=24 B
stateFNV=09e58e0d
world=992/1888
tile=943
flags=10000400
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
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
```

The same event 61 is present in both fresh-map tile calls, but hardware proved zero eligible commands in both exact contexts:

```text
first tile  flags=0x1000040f -> eligible=0
second tile flags=0x10000400 -> eligible=0
```

Script state therefore remains byte-for-byte canonical:

```text
script FNV bc9b18ff -> bc9b18ff
changed=no
```

Input owners remain unchanged:

```text
PlayerView FNV=1bd0f09b
InitialTile FNV=f73e28b2
Orientation FNV=acc754a6
facingRefreshPending=1
tileEnterPending=0
```

## Recovered fresh-map ordering

```text
Game_spawnPlayer:
  placement
  Render.viewZOld=4
  Hud.isUpdate=true
  checkFacingEntity()                    # transient previous-vector result
  Player_setup()
  Game_executeTile(... 0x1000040f)       [hardware-proven]

caller -> DoomCanvas_finishRotation():
  viewSin/viewCos/viewStepX/viewStepY     [hardware-proven]
  Game_executeTile(... 0x10000400)        [hardware-proven]
  checkFacingEntity()                     [next]
```

The transient first facing result remains deliberately unowned.

## Second-tile fail-closed proof

```text
nullView=1
nullInitial=1
nullOrientation=1
nullOutput=1
inactive=1
tilePending=1
missingFacing=1
angle=1
blocked=1
initialMismatch=1
orientationInactive=1
orientationMismatch=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

## Latest hardware RAM / integrity baseline

Second-tile milestone on real CYD:

```text
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
nonScriptStable=yes

heap8=72796->72796
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat:

```text
heap=138560
heap8=72796
largest8=34804
```

Same-build equality witnesses:

```text
gameFNV=c655ff85->c655ff85
playerFNV=774ed642->774ed642
canvasFNV=1b7ba23f->1b7ba23f
renderFNV=f9344dec->f9344dec
frameFNV=e6066fb0->e6066fb0
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
nativeInitialTile=yes
nativeOrientation=yes
nativeSecondTile=yes
secondTilePending=no
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

The player/view, HUD, fresh-map session, initial-tile, orientation and second-tile owners have lifetimes distinct from the seven-owner resident arena.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next bounded milestone after merge

Recover from the exact post-merge `main`. The next exact legacy operation is:

```text
DoomCanvas_checkFacingEntity()   # durable final facing result
```

Design the next owner/query against the compact resident topology and hardware-proven player position/orientation. Do not revive legacy entity/render runtime. Keep `ST_PLAYING` separate until durable facing ownership is hardware-proven.

## Merge recommendation

```text
MERGE agent/esp32-native-finish-rotation-second-tile
```

Hardware-tested firmware:

```text
df4f62687d99eb3b3e9569ae6861b6909d59c82d
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
