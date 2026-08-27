# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + `DOCUMENTATION.md` + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #100 — native gameplay SELECT front-tile
main = be4a9a666245663da7866a8aa0aa40b98339d076
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md`](MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md).

## Current candidate milestone

```text
branch = agent/esp32-native-entrance-startup-route
base   = be4a9a666245663da7866a8aa0aa40b98339d076
hardware-tested implementation SHA = 87d7923b42eda0f36a1c7daded33ca0c4f5a4958
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

Evidence: [`MAP1_NATIVE_ENTRANCE_STARTUP_ROUTE.md`](MAP1_NATIVE_ENTRANCE_STARTUP_ROUTE.md).

## Critical startup correction — hardware proven

The original game startup route is now recovered and enforced:

```text
cinematic intro
 -> startupMap = 1
 -> MAP_INTRO = 1
 -> /intro.bsp
 -> map name = Entrance
```

`/intro.bsp` is **not** the cinematic itself. It is the first post-cinematic gameplay BSP, Entrance.

`/level01.bsp` is the next catalog resource (`MAP_SECTOR01 = 2`) and must not replace `/intro.bsp` merely because of its filename.

The previous visible boot to Junction came from historical ESP32 validation scaffolding continuing automatically past Entrance:

```text
Entrance resident
 -> target preflight
 -> ResidentHandoff
 -> CommittedTransition
 -> Junction
```

That automatic progression is now cut after the source-only transition preflight. The production startup path preserves Entrance resident and never calls the destructive/committing Junction probes.

## Current hardware PARK — Entrance

Real classic CYD:

```text
resourceMapId = 1
file = /intro.bsp
name = Entrance
startupMap = 1
sourceBytes = 21823
crc32 = 623f34e4
runtime arena = 14095 B
runtimeFNV = c3882516
snapshotBytes = 96
snapshotFNV = b3811f3d
payload = 17891 B
spawnHeader = 904
spawnDirection = 64
```

Resident owner fingerprints:

```text
mapStateFNV = cd99b98e
scriptFNV = f9e3d9df
lineFNV = e5e74861
textureFNV = f1fc1875
automapFNV = 669b1aa7
topologyFNV = 3f321e43
```

Structural/topology cardinalities:

```text
nodes = 223
lines = 480
sprites = 344
events = 93
byteCodes = 265
strings = 94
native topology entities = 220
enemies = 30
destructibles = 13
legacy Game.entities = 0
legacy Game.monsters = 0
```

Startup route witness:

```text
cinematicIntro=done
entranceResident=yes
targetPreflightOnly=yes
residentHandoff=no
committedTransition=no
junctionResident=no
junctionGameplay=no
spawnDeferred=yes
firstFrameDeferred=yes
packClosed=yes
legacyRuntimeClear=yes
shapeData=NULL
mediaTexels=NULL
```

Hardware integrity:

```text
frameFNV = faa62417 -> faa62417 exact
heap8 = 64464 -> 64464
largest8 = 34804 -> 34804
observer allocation = none
stable heartbeat heap = 130172
stable heartbeat heap8 = 64464
stable heartbeat largest8 = 34804
```

No Guru Meditation, reboot or probe failure was reported.

## Permanent memory / architecture invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
runtime ZIP = forbidden for migrated map/graphics paths
backing     = /DoomRPG-ESP32.pak
legacy Game.entities = 0
legacy Game.monsters = 0
```

Native architecture remains:

```text
original Doom RPG data/behavior
 -> ESP32-native parsers
 -> compact immutable resident map
 -> explicit compact mutable overlays
 -> native event/script semantics by bounded family
 -> native gameplay
 -> native renderer
```

Do not reintroduce map-wide `shapeData`, map-wide `mediaTexels`, runtime ZIP reads, or pointer-heavy legacy world ownership.

## Historical Junction hardware canon remains valid

The startup correction does not invalidate prior Junction hardware work. It only removes the automatic boot skip.

Stable Junction canons from merged milestones include:

```text
resource = /junction.bsp
resourceMapId = 9
gameplayLoadMapId = 2
sourceBytes = 21051
sourceCRC32 = 4a2c5800
sourceFNV = fefaf5ca
runtimeFNV = bc432a0f
lineState baseline FNV = 3658710d
fresh player = 992,1888,36
fresh angle = 64 / North
fresh tile = 943
HUD frame = ba3e5182
HUD viewport = 9206eb24
HUD bands = 6c2aa46f
HUD stateFNV = 4756db9c
```

Prior hardware-proven Junction gameplay components remain reusable:

```text
native transition/residency
spawn/player/view ownership
native PLAYING service
walls + textured planes
billboards + glows
HUD
calibrated touch input
TURN_LEFT/TURN_RIGHT
FORWARD/BACK/STRAFE
static + sprite/line collision
dynamic per-line open/close collision
SELECT front-tile/event/filter/line provenance
```

They are currently not in the production startup route because Entrance must be brought to the same gameplay boundary first.

## Important gameplay facts already recovered

Junction arrival door witness remains canonical:

```text
tile 975 -> event 63 -> EV_OPENLINE(35)
line35 flags = 0x00000505
LOCKED bit = yes
open bit = no
```

Legacy door semantics refuse locked lines before toggling open state. Do not fake key ownership or bypass the lock.

Generic `EspMapOpcodeExecutor` remains intentionally limited to opcodes:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

All unsupported opcodes remain fail-closed until dedicated milestones own them.

## Current production boundary

```text
cinematic finished = yes
Entrance resident = yes
Entrance player spawn = no
Entrance first native frame = no
Entrance native ST_PLAYING = no
Junction resident = no
Junction gameplay = no
shapeData = NULL
mediaTexels = NULL
legacy entities = 0
legacy monsters = 0
```

The visible framebuffer intentionally remains the last intro frame at this PARK. That is expected for this milestone.

## Recommended next bounded milestone

Next objective:

```text
canonical Entrance resident
 -> consume BSP header spawn tile 904
 -> consume direction 64
 -> prepare native player/view placement
 -> render first native Entrance frame
 -> preserve compact resident owners
 -> no automatic CHANGEMAP
 -> no Junction handoff
```

Reuse the permanent native spawn/view/render APIs already hardware-proven on Junction where their contracts are map-generic. Do not duplicate them as Entrance-specific permanent architecture.

After the first Entrance frame passes on real hardware, reattach the already-proven HUD/input/TURN/MOVE/SELECT chain to Entrance in bounded milestones. Entrance is the desired corpus for real openable doors, computer/password interactions and other early-game semantics.

## Merge recommendation

```text
REAL-CYD HARDWARE PASS
hardware-tested implementation SHA = 87d7923b42eda0f36a1c7daded33ca0c4f5a4958
MERGE-READY = YES
```

Every commit after `87d7923...` is documentation-only. After merge, recover the exact new `main` SHA and reread this file, `DOCUMENTATION.md`, and [`MAP1_NATIVE_ENTRANCE_STARTUP_ROUTE.md`](MAP1_NATIVE_ENTRANCE_STARTUP_ROUTE.md) before creating the next `agent/*` branch.
