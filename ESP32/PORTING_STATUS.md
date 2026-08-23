# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #68 — native committed Junction transition
main = 00268a100c6662cb883f9a02d979b4f29eecbf12
hardware-tested firmware = 759b7f05a7c1940e98caf68e4041faa69b34cfc9
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-junction-spawn
base   = 00268a100c6662cb883f9a02d979b4f29eecbf12
hardware-tested firmware = 08a3a29c5e4e4a64000fa12a877299bbb1e772a0
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Active evidence: [`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md).

This milestone adds a permanent 24-byte pointer-free fresh-map player spawn projection. It reproduces the deterministic placement portion of recovered `Game_spawnPlayer()` for the already committed Junction map without applying placement to legacy state and without entering gameplay.

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

## Entrance canon

```text
resource=/intro.bsp
name=Entrance
bytes=21823
crc32=623f34e4
sourceFNV=d5cc751f
gameplayLoadMapId=1
spawnIndex=904
spawnDirection=64
camera=648
floorTex=145
ceilingTex=112
nodes=223
lines=480
mapSprites=344
events=93
byteCodes=265
strings=94
stringData=7779
```

Entrance native resident memory:

```text
snapshotBytes=96
snapshotFNV=b3811f3d
logical payload=17891 B
actual heap=18008 B
allocator overhead=117 B
```

Owner FNVs:

```text
runtime  = c3882516
map      = cd99b98e
script   = f9e3d9df
line     = e5e74861
texture  = f1fc1875
automap  = 669b1aa7
topology = 3f321e43
```

All real MAP_INTRO opcode IDs have explicit native ownership/execution boundaries:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

## Hardware-proven exit / transition chain

Real Entrance CHANGEMAP:

```text
event=1
commandOffset=1
globalCommand=2
arg1=80000000
mapName=/junction.bsp
targetMap=9
spawnParam=0
showStats=1
pending=1
```

Native chain now proven through fresh-map spawn projection:

```text
CHANGEMAP pending intent
 -> level-exit stats
 -> native player exit-state
 -> LEVEL stats-menu semantic intent
 -> immutable 13-map catalog
 -> Junction target preflight
 -> explicit resident lifecycle
 -> reversible Entrance -> Junction -> Entrance proof
 -> committed transition state machine
 -> real committed Entrance -> Junction resident swap
 -> fresh-map load semantic ownership
 -> deterministic native player spawn projection
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
```

## Junction source canon

Resource identity and gameplay progression identity are distinct:

```text
resourceMapId      = 9 / /junction.bsp
gameplayLoadMapId  = 2
hubProgressionGate = 1
```

Source:

```text
entryOffset=1974397
bytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
name=Junction
spawnIndex=943
spawnDirection=64
camera=0
floorTex=117
ceilingTex=151
nodes=77
lines=207
mapSprites=48
events=66
byteCodes=319
strings=126
stringData=12235
runtimePlan=8867 B
```

## Junction resident canon

```text
snapshotBytes=96
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
allocator overhead=130 B
```

Owner payloads:

```text
runtime   = 8867 B
map       = 1024 B
script    = 73 B
line      = 52 B
texture   = 26 B
automap   = 32 B
topology  = 336 B
```

Owner FNVs:

```text
runtime  = bc432a0f
map      = c5cdfc04
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = 0b2ae445
topology = d6e8df7d
```

Compact topology:

```text
entities=30
enemies=0
destructibles=3
```

These are compact native topology entities. Legacy `Game.entities` and `Game.monsters` remain zero.

## Permanent committed transition

```text
EspMapCommittedTransitionState = 24 B
persistent heap = 0 B
```

The state machine owns WAIT_STATS -> READY -> COMMITTED and automatic post-teardown rollback. PR #68 hardware-proved both a forced rollback and a final real commit leaving Junction resident.

Same firmware as the spawn milestone reran that prerequisite successfully:

```text
COMMITTED FNV=2c595a62
sourceHeap=65544
targetHeap=73012
targetHeapGain=7468
largest=34804->34804
targetSnapshotFNV=bc9071e9
packClosed=yes
```

The absolute heap baseline is build-context dependent; proven resident costs remain Entrance 18008 B and Junction 10540 B.

## Recovered Game_spawnPlayer semantics

Header fallback when `spawnParam == 0`:

```text
x     = mapSpawnIndex % 32
y     = mapSpawnIndex / 32
angle = mapSpawnDir
```

Packed override when `spawnParam != 0`:

```text
x     = spawnParam & 31
y     = (spawnParam >> 5) & 31
angle = (spawnParam >> 10) & 255
spawnParam cleared
```

Common placement writes:

```text
viewX = destX = x*64 + 32
viewY = destY = y*64 + 32
viewZ = 36
viewAngle = destAngle = angle
Render.viewZOld = 4
```

Legacy next performs facing refresh and, for a fresh map, `Player_setup()` plus the initial tile-enter event. Those remain outside the current milestone.

## Fresh-map load semantic

Hardware-proven normal CHANGEMAP context:

```text
loadType=0
gameIsLoaded=0
normalMapLoad=yes
savedGameLoad=no
activeLoadType=0
loadTypeMutation=no
```

Nonzero `loadType` belongs to saved-game restoration and remains fail-closed.

## Permanent native player spawn owner

Files:

```text
ESP32/include/esp_player_spawn_state.h
ESP32/src/esp_player_spawn_state.c
```

API:

```text
EspPlayerSpawn_reset()
EspPlayerSpawn_prepareCommitted()
```

State:

```text
EspPlayerSpawnState = 24 B
persistent heap = 0 B
```

The permanent function requires a committed transition, fresh-map load context, complete matching target inventory and the matching live resident runtime. It performs no PAK I/O, allocation or legacy mutation.

## Hardware-proven real Junction spawn

Because the committed transition retains `spawnParam=0`, the BSP header is authoritative.

Real CYD:

```text
stateBytes=24
stateFNV=ba6af4a7
targetMap=9
gameplayLoadMapId=2
spawnParam=00000000
source=HEADER
tileIndex=943
tileX=15
tileY=29
worldX=992
worldY=1888
angle=64
viewZ=36
viewZOld=4
loadType=0
active=1
```

Pending effects:

```text
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
spawnApplied=no
facingApplied=no
playerSetupApplied=no
tileEnterApplied=no
```

## Hardware-proven packed override

Probe-local committed transition copy:

```text
spawnParam=00030167
tileIndex=359
tile=7/11
world=480/736
angle=192
source=OVERRIDE
overrideUsed=1
headerIgnored=yes
stateFNV=e0a5110b
```

The real committed transition remained unchanged.

## Hardware fail-closed proof

```text
nullTransition=1
nullInventory=1
nullOutput=1
notCommitted=1
loadType=1
loadedWorld=1
targetMismatch=1
runtimeMismatch=1
badHeaderSpawn=1
reset=1
outputAtomic=yes
```

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

heap8=73012->73012
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

The spawn owner adds no persistent heap allocation.

## Legacy / framebuffer integrity

Same-probe equality witnesses:

```text
placementFNV=5d1076bf->5d1076bf
playerFNV=a1725bcb->a1725bcb
frameFNV=833705d2->833705d2
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
committedTransition=yes
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativeSpawnState=yes
spawnProjected=yes
spawnApplied=no
loadType=0
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

Stable post-probe heartbeat:

```text
heap=138776
heap8=73012
largest8=34804
SD=ready
VIDEO=ready
CORE=ready
```

The heartbeat's existing `ZIP=ready` label does not mean runtime ZIP map access. Runtime backing remains `/DoomRPG-ESP32.pak`.

## Current architecture boundary

Hardware-proven native ownership now includes:

```text
compact immutable Entrance + explicit mutable owners
all 16 MAP_INTRO opcode families
SAVEGAME route
CHANGEMAP transition intent
SHOW/HIDE compact topology
level-exit stats
player exit-state
stats-menu semantic intent
13-map resource catalog
Junction PAK/BSP preflight
resident lifecycle
reversible resident handoff
committed transition with rollback
real Entrance -> Junction resident swap
Junction compact topology
fresh-map loadType semantic
24 B native player spawn projection
header fallback + packed override decode
explicit facing/setup/tile-enter pending boundary
```

Still intentionally outside:

```text
actual stats-menu rendering/input
application of projected coordinates to native player/view state
native facing-entity computation
Player_setup-equivalent native initialization
initial tile-enter execution
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next direction after merge

After merge, recover from true `main` before choosing the next bounded milestone. The likely next layer is applying the hardware-proven spawn projection to a small native player/view owner, potentially followed separately by compact-topology facing lookup. Do not open full `ST_PLAYING` or collapse facing/setup/tile-enter blindly into the same milestone.

## Merge recommendation

```text
MERGE agent/esp32-native-junction-spawn
```

Hardware-tested firmware:

```text
08a3a29c5e4e4a64000fa12a877299bbb1e772a0
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
