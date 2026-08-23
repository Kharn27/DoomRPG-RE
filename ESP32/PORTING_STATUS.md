# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #75 — native finishRotation second tile
main = 7a0e57cf13d02320be3a238dc73499a023c9f04c
hardware-tested firmware = df4f62687d99eb3b3e9569ae6861b6909d59c82d
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md`](MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-durable-facing
base   = 7a0e57cf13d02320be3a238dc73499a023c9f04c
hardware-tested firmware = 660c797e2168260a861c185fae9e812769b46156
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_DURABLE_FACING.md`](MAP1_NATIVE_DURABLE_FACING.md).

This milestone hardware-proves the final durable `DoomCanvas_checkFacingEntity()`
at the end of recovered `DoomCanvas_finishRotation()`. `finishRotation()` is now
semantically complete natively; `ST_PLAYING` remains explicitly deferred.

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

Resident owner FNVs:

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
 -> Junction transition preflight
 -> resident lifecycle / committed swap
 -> 24 B fresh-map spawn projection
 -> 44 B active player/view owner
 -> 8 B HUD dirty owner
 -> 24 B Player_setup session owner
 -> 24 B initial tile owner
 -> 24 B finishRotation orientation owner
 -> 24 B finishRotation second-tile owner
 -> 32 B durable facing owner
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
Junction durable-facing FNV    = 95aa1108
post-facing player/view FNV    = afcdcf74
```

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20. All real
MAP_INTRO opcode families already have dedicated native boundaries.

## Hardware-proven durable facing boundary

Exact legacy operation:

```text
DoomCanvas_checkFacingEntity()
```

Exact Junction ray:

```text
traceStart=(992,1857)
traceEnd=(992,1665)
traceFlags=0x0001f6ff
tiles=(15,29)->(15,28)->(15,27)->(15,26)
```

Permanent native reconstruction:

```text
EspMapTopologyQuery_findLinkedOnTile()
EspPlayerFacingState = 32 B
persistent heap = 0 B
```

The resolver uses immutable native sprite/line/block data, compact
`EspMapSpriteTopology`, `EspMapLineState` and bounded `/entities.db` PAK reads.
It never revives legacy `Entity_t`, `entityDb[1024]`, `Game_trace()` or a
`Player.facingEntity` write.

Real-CYD result:

```text
stateFNV=95aa1108
kind=0 / none
hitIndex=65535
hitTile=65535
entityType=255
entitySubType=255
legacyIdentity=00000000
traceEntities=0
active=1
targetMap=9
gameplayLoadMapId=2
loadType=0
```

The hardware therefore proves that Junction fresh-map durable facing has no
facing target on the recovered legacy ray.

PlayerView transition:

```text
beforeFNV=1bd0f09b
afterFNV=afcdcf74
hudRefreshPending=0
facingRefreshPending=0
playerSetupPending=0
tileEnterPending=0
consumedOnlyFacing=yes
```

Input owners stayed unchanged:

```text
InitialTile FNV=f73e28b2
Orientation FNV=acc754a6
SecondTile FNV=09e58e0d
```

Fail-closed hardware proof:

```text
nullView=1
nullInitial=1
nullOrientation=1
nullSecond=1
nullOutput=1
missingFacing=1
tilePending=1
angle=1
initialMismatch=1
orientationInactive=1
secondInactive=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

## Latest hardware RAM / integrity baseline

Durable-facing milestone on real CYD:

```text
snapshotFNV=bc9071e9->bc9071e9
unchanged=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes

heap8=72736->72736
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same-build equality witnesses:

```text
gameFNV=c655ff85->c655ff85
playerFNV=c64e7862->c64e7862
canvasFNV=1b7ba23f->1b7ba23f
renderFNV=f9344dec->f9344dec
frameFNV=9eb7ce0f->9eb7ce0f
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
FacingEntityMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

These equality FNVs are same-build witnesses, not cross-build canons.

## Recovered fresh-map ordering now hardware-complete through finishRotation

```text
Game_spawnPlayer:
  placement                              [hardware-proven]
  Render.viewZOld=4                      [owned natively]
  Hud.isUpdate=true                      [hardware-proven]
  checkFacingEntity() transient          [deliberately unowned]
  Player_setup()                         [hardware-proven]
  Game_executeTile(...0x1000040f)        [hardware-proven]

caller -> DoomCanvas_finishRotation():
  viewSin/viewCos/viewStepX/viewStepY     [hardware-proven]
  Game_executeTile(...0x10000400)         [hardware-proven]
  checkFacingEntity() durable             [hardware-proven]
```

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
nativeFacing=yes
facingPending=no
finishRotationComplete=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Current architecture boundary

Hardware-proven native ownership now includes the complete recovered fresh-map
spawn/setup/tile/finishRotation sequence through durable facing.

Still intentionally outside:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
weapon restore/select ownership when disabledWeapons!=0
caller-side ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next bounded milestone after merge

After merge, read the exact new `main` SHA and recover the caller-side state
progression immediately following the now-complete `DoomCanvas_finishRotation()`.
Keep the next milestone limited to the smallest native state transition toward
`ST_PLAYING`; do not bundle entity gameplay, rendering, AI or legacy world
reconstruction into it.

## Merge recommendation

```text
MERGE agent/esp32-native-durable-facing
```

Hardware-tested firmware:

```text
660c797e2168260a861c185fae9e812769b46156
```

Every later commit on this branch must remain documentation-only unless another
firmware is flashed.
