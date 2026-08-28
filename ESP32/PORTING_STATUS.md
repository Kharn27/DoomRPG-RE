# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Architecture belongs in [`ARCHITECTURE.md`](ARCHITECTURE.md); this file keeps
only the current Git boundary, hardware facts, canonical resident witnesses and
the explicit fail-closed frontier.

Repository state wins over chat history.

## Git boundary

Latest merged baseline:

```text
PR   = #106 — ESP32 native engine cleanup
main = 439d963c39d1dc6435ee9bcd91b3292fc5b59c53
status = MERGED
```

Current branch:

```text
branch = agent/esp32-native-renderer-naming
base main = 439d963c39d1dc6435ee9bcd91b3292fc5b59c53
physical implementation rename = b192b5fe8039b4afd539b658bd01869e383dd2e5
generic API promotion = a17dc54bf804bd0113463158ba32a68537294f27
hardware-tested final code HEAD = cc2a8c6e7baa6c1267cde2629feead68cb2602de
status = REAL-CYD HARDWARE PASS
merge-ready = YES after docs-only finalization
```

The branch removes the last historical Junction naming from the active sprite
renderer without changing its rendering algorithm:

```text
ESP32/src/esp_native_junction_sprite_renderer.c
 -> ESP32/src/esp_native_sprite_renderer.c

EspNativeJunctionSpriteStats
 -> EspNativeSpriteStats

EspNativeJunctionSprite_render()
 -> EspNativeSpriteRenderer_render()

ESP32/include/esp_native_junction_sprite_renderer.h
 -> removed
```

The first physical file rename was bit-for-bit. The API promotion then made
`esp_native_sprite_renderer.h` the real owner of the stats structure and public
symbol. The final code checkpoint replaced only the remaining historical
identifiers in the implementation and physically removed the compatibility
header.

### Hardware evidence for renderer naming cleanup

The normal `esp32-cyd` firmware was rebuilt/flashed and exercised on the real
classic CYD during the naming pass.

At the first rename checkpoint, observed serial/runtime behavior included:

```text
[RESIDENTGAMEPLAY] READY map=current ...
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes ...
shapeData=0x0
mediaTexels=0x0
heap8 before dialog lazy owner = 27052
heap8 after dialog lazy owner = 26420
largest8 = 16372
```

Movement, rotation and scientist dialog event 88 remained functional. The dialog
opened, fast-forwarded and paged normally with the same lazy owner behavior.

The promoted generic API checkpoint was then hardware-tested successfully. The
final code HEAD `cc2a8c6e` was also rebuilt/flashed and run on the real CYD; the
reported result was that the game behaved as before with no visible/apparent
`FAILED` condition. No new hardware failure was reported.

This establishes `cc2a8c6e` as the hardware-tested code boundary for this branch.
Commits after it must remain documentation-only.

## Permanent rule

```text
A NEW BSP IS NOT A NEW ENGINE.
```

Production map path:

```text
/DoomRPG-ESP32.pak
 -> EspBspReader inventory
 -> EspMapResidentLifecycle
 -> compact immutable EspMapRuntime
 -> explicit compact mutable owners
 -> EspPlayerView
 -> EspNativeGameplaySession
 -> generic renderer / HUD / input / events / dialog
```

No future level should create another `native_mapN_*` ladder or level-specific
renderer.

## Hardware / memory invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
backing     = /DoomRPG-ESP32.pak
```

Migrated world data must remain compact/offset-based. Do not reintroduce map-wide
shape/media texel pools or a desktop Entity pointer graph.

## Entrance canonical resident witness

Entrance remains the hardware corpus for initial new-game startup, not a
specialized engine path.

```text
resourceMapId = 1
resource = /intro.bsp
name = Entrance
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
runtime arena = 14095 B
runtimeFNV = c3882516
snapshotBytes = 96
snapshotFNV = b3811f3d
resident payload = 17891 B
spawn tile = 904
spawn direction = 64
spawn position = 544,1824,36
oldZ = 4
```

Owner fingerprints:

```text
mapStateFNV  = cd99b98e
scriptFNV    = f9e3d9df
lineFNV      = e5e74861
textureFNV   = f1fc1875
automapFNV   = 669b1aa7
topologyFNV  = 3f321e43
```

Cardinalities:

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
```

These values are regression witnesses only. Production behavior must never
select an implementation based on them.

## Generic session baseline

Current initial session canon:

```text
targetMapId = 1
gameplayLoadMapId = 1
angle = 64
graphics textures = 33
graphics sprites = 45 -> 46 after dependency closure
catalog storage = 3120 B
catalog FNV = 29ffc14a
initial world frame = 71ca7465
initial walls = 8 / 4430 pixels
HUD hp = 30/30
HUD armor = 0/20
HUD weapon = 2
HUD ammo = 8
```

Resident render-cache historical canon:

```text
owner = 21160 B
payload = 16384 B
SMALL-COLD  = 2119886 us
SMALL-WARM  = 256807 us
LARGE-LEARN = 247770 us
LARGE-WARM  = 229719 us
large entries = 2
```

Do not optimize `PlatformVideo_present()` without profiling evidence; the game
is turn-based and redraw/presentation is demand-driven.

## Production gameplay boundary

Hardware-proven native gameplay includes:

```text
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
native topology/entity/line collision
dynamic line/door collision
SELECT front-tile provenance
EV_OPENLINE / EV_CLOSELINE
MOVE source EXIT + destination ENTER bounded event routes
regular-door bounded visual interpolation
EV_DIALOG / EV_DIALOGNOBACK presentation
progressive dialog / paging / fast-forward / close
saved dialog continuation transaction
EV_SHOW / EV_HIDE / EV_UNLOCK continuation
state ops 11 / 19 / 20
EV_FORCEMESSAGE top-bar owner/painter
EV_NOTE bounded prefix before dialog
native idle first-person weapon painting
eType=5 weapon consumed-bit/ownership/auto-select overlay
resident opcode/pickup corpus diagnostics
```

The engine still does **not** broad-enable legacy `Game_executeEvent()`.

## Event frontier

Known Entrance opcode IDs:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

Currently production-bounded:

```text
7  EV_SHOW
8  EV_DIALOG
11 EV_CHANGESTATE
13 EV_UNLOCK
15 EV_OPENLINE
16 EV_CLOSELINE
18 EV_HIDE
19 EV_NEXTSTATE
20 EV_PREVSTATE
24 EV_FORCEMESSAGE
26 EV_DIALOGNOBACK
40 EV_NOTE
```

Still intentionally deferred/fail-closed:

```text
2  EV_CHANGEMAP  -> live transition consumer pending
9  EV_GIVEMAP    -> automap production route pending
10 EV_PASSWORD   -> password input UI pending
27 EV_SAVEGAME   -> save consumer pending
41 EV_CHECK_KEY  -> native player-key owner pending
```

`EspMapOpcodeExecutor` itself remains intentionally limited to 11/19/20; other
owned families use their dedicated native semantic modules.

## Pickup frontier

Current production pickup owner:

```text
eType=5 weapon
world remove = consumed-sprite bit overlay
ownership = native uint16 weapon mask
new weapon select = native HUD overlay
scope = current map/runtime arena
rollback = exact on redraw failure
```

Deferred:

```text
weapon ammo increment / acquisition feedback / sound
eType=3 world/player-stat item
eType=4 inventory item
eType=6 ammo
eType=16 alternate ammo
```

## Historical Junction corpus

Junction remains useful only as a second regression corpus for the same engine:

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
angle = 64
fresh tile = 943
```

There is no longer an active Junction-named sprite renderer source/header/API.
Do not turn these regression values back into a Junction-specific implementation.

## Cleanup milestones

The major engine cleanup merged as PR #106 and is the current `main` baseline.
It removed the historical MAP1/Junction/Entrance probe ladders from production
and deleted their retired source/header archaeology from the active tree.

The current renderer naming milestone is hardware complete:

```text
PASS 1  physical generic renderer implementation filename
        -> b192b5fe
        -> real-CYD PASS

PASS 2  generic public stats/API ownership
        -> a17dc54b
        -> real-CYD PASS

PASS 3  remove remaining Junction identifiers + compatibility header
        -> cc2a8c6e
        -> real-CYD PASS

RESULT  branch agent/esp32-native-renderer-naming = MERGE-READY
```

After merge, recover the new exact GitHub `main` SHA before starting another
branch. The next bounded cleanup should audit one active `*probe*` compatibility
family at a time; do not delete those transitional modules blindly.
