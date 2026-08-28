# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Architecture belongs in [`ARCHITECTURE.md`](ARCHITECTURE.md); this file keeps
the current Git boundary, hardware facts, canonical resident witnesses and the
explicit fail-closed frontier.

Repository state wins over chat history.

## Git boundary

Latest merged baseline:

```text
PR   = #107 — ESP32 native renderer naming cleanup
main = d0643defd772f83fba07e171950a70b104bbeb6f
status = MERGED
```

Current branch:

```text
branch = agent/esp32-render-startup-bridge
base main = d0643defd772f83fba07e171950a70b104bbeb6f
API promotion/shim checkpoint = 9b3cdec3e7f84540b5d405d7e31dd8e8c9a89cce
hardware-tested final code HEAD = 88dc75f9f8e9c7ccd972bf66439eb3dd65b29127
status = REAL-CYD HARDWARE PASS
merge-ready = YES after docs-only finalization
```

This branch promotes the retained CYD `Render_startup()` compatibility path
from historical `probe` naming into a permanent bridge API:

```text
ESP32/include/esp_render_startup_bridge.h
EspRenderStartupBridge_start()
ESP32/src/render_startup_bridge.c
```

The old `ESP32/include/render_startup_probe.h` compatibility shim is physically
removed. `main.cpp` and the implementation now include/call the permanent API
directly. The bridge behavior itself is unchanged: it reuses the existing
160x120 RGB565 PlatformVideo framebuffer, loads the retained sintable/palette
resources required by legacy Render helpers, keeps `piDIB == NULL`, and owns the
`Render_startup` / `Render_free` linker compatibility boundary.

### Hardware evidence for render-startup bridge cleanup

The normal `esp32-cyd` firmware was rebuilt/flashed on the real classic CYD for
both bounded naming passes.

PASS 1 at `9b3cdec3e7f84540b5d405d7e31dd8e8c9a89cce` proved that the generic API could
own the compiled symbol while the old header remained only as a source shim.
Observed heartbeat state was:

```text
PRERENDER=ready
RENDER=ready
MAPPINGS=ready
MENUBSP=ready
heap=92816
heap8=27052
largest8=16372
```

PASS 2 at final code HEAD `88dc75f9f8e9c7ccd972bf66439eb3dd65b29127`
removed the shim and switched the two real consumers directly to
`EspRenderStartupBridge_start()`. The real CYD again reached the generic native
session successfully:

```text
[NATIVEBOOT] READY ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes ...
[ALIVE] ... PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready
heap=92816 heap8=27052 largest8=16372
```

No visible/apparent `FAILED`, panic or reboot was reported. The game behaved as
before. This establishes `88dc75f9` as the hardware-tested code boundary for
this branch. Commits after it must remain documentation-only.

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

Resident render-cache stable owner/capacity canon:

```text
owner = 21160 B
payload = 16384 B
large entries after learn = 2
```

Timing figures are observational and can vary between runs. The final
render-startup hardware PASS still showed the expected cold/warm ordering with
no ownership regression.

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

There is no active Junction-named sprite renderer source/header/API. Do not turn
these regression values back into a Junction-specific implementation.

## Recent cleanup milestones

Merged PR #106 removed the historical MAP1/Junction/Entrance probe ladders and
retired their source/header archaeology from the active tree.

Merged PR #107 completed generic sprite-renderer naming:

```text
ESP32/src/esp_native_sprite_renderer.c
ESP32/include/esp_native_sprite_renderer.h
EspNativeSpriteStats
EspNativeSpriteRenderer_render()
```

Current render-startup bridge cleanup:

```text
PASS 1  permanent API ownership + compatibility shim
        -> 9b3cdec3
        -> real-CYD PASS

PASS 2  direct consumers + physical shim removal
        -> 88dc75f9
        -> real-CYD PASS

RESULT  branch agent/esp32-render-startup-bridge = MERGE-READY after docs-only finalization
```

After merge, recover the new exact GitHub `main` SHA before starting another
branch. Continue cleanup one active `*probe*` compatibility family at a time;
do not mass-delete transitional modules blindly.
