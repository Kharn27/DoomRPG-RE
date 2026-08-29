# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Architecture belongs in [`ARCHITECTURE.md`](ARCHITECTURE.md); this file keeps
the current Git boundary, hardware facts, canonical resident witnesses and the
explicit fail-closed frontier.

Repository state wins over chat history.

## Git boundary

Latest merged baseline:

```text
PR   = #108 — ESP32 render startup bridge cleanup
main = d65621cb0c308d648c4b578f2a474aee3cc481a4
status = MERGED
```

Current branch:

```text
branch = agent/esp32-legacy-prerender-startup
base main = d65621cb0c308d648c4b578f2a474aee3cc481a4
PASS 1 physical implementation rename = 3754ccec31e6f892f93dd936e047fe003d33989b
hardware-tested final code HEAD = e55465c3f0d93930c7946c1293a1e7ac4f149aae
status = REAL-CYD HARDWARE PASS
merge-ready = YES after docs-only finalization
```

This branch retires the historical `pre_render_probe.*` naming from a live
legacy-compatibility startup stage. The recovered desktop startup order is:

```text
DoomCanvas_startup()
 -> ParticleSystem_startup()
 -> MenuSystem_startup()
 -> EntityDef_startup()
 -> Render_startup()
```

The retained CYD stage between validated layout and the already-permanent Render
bridge is now owned by:

```text
ESP32/src/esp_legacy_prerender_startup.c
ESP32/include/esp_legacy_prerender_startup.h
EspLegacyPrerenderStartup_start()
```

It still initializes exactly `ParticleSystem`, `MenuSystem` and `EntityDef`,
preflights the same five ZIP compatibility resources, and stops before
`Render_startup()`. It is explicitly legacy bootstrap compatibility, not native
map/gameplay ownership.

### Hardware evidence for legacy pre-render startup cleanup

PASS 1 at `3754ccec31e6f892f93dd936e047fe003d33989b` was a physical source rename only:

```text
ESP32/src/pre_render_probe.c
 -> ESP32/src/esp_legacy_prerender_startup.c
```

GitHub detected 0 additions, 0 deletions and 0 content changes; the source blob
remained `aa431f90c21bab10edf5b140b887834d6524f9da`.

The normal `esp32-cyd` firmware was rebuilt/flashed and exercised on the real
classic CYD. Observed pre-render startup remained:

```text
ParticleSystem_startup used = 18712 B
MenuSystem_startup used = 4492 B
EntityDef_startup used = 2776 B
Entity defs = 115
pre-render total used = 25980 B
heap8 after pre-render = 50632
largest8 after pre-render = 32756
```

PASS 2 at final code HEAD `e55465c3f0d93930c7946c1293a1e7ac4f149aae`
introduced the permanent header/API, switched `main.cpp` and the implementation
to it directly, and physically removed `pre_render_probe.h`. No startup order,
resource, allocation or `[PRERENDER]` diagnostic behavior was changed.

The real CYD subsequently reached the full native gameplay session with:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430
[ENGINECACHE] OWNER bytes=21160 payload=16384 entries=256
[ENGINECACHE] PRIMED ... largeEntries=2 heap8=27112 largest8=16372
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=92816 heap8=27052 largest8=16372
PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready
```

No visible/apparent `FAILED`, panic or reboot was reported. This establishes
`e55465c3` as the hardware-tested code boundary for this branch. Commits after
it must remain documentation-only.

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

Timing figures are observational and can vary between runs. Do not optimize
`PlatformVideo_present()` without profiling evidence; Doom RPG is turn-based and
redraw/presentation is demand-driven.

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

```text
PR #106  historical MAP1/Junction/Entrance probe archaeology removed
PR #107  sprite renderer naming made fully generic
PR #108  Render startup probe naming retired into EspRenderStartupBridge_start()
```

Current legacy pre-render startup cleanup:

```text
PASS 1  physical source rename, bit-for-bit
        -> 3754ccec
        -> real-CYD PASS

PASS 2  permanent API + direct consumers + old header removal
        -> e55465c3
        -> real-CYD PASS

RESULT  branch agent/esp32-legacy-prerender-startup = MERGE-READY after docs-only finalization
```

After merge, recover the new exact GitHub `main` SHA before starting another
branch. Continue cleanup one active `*probe*` compatibility family at a time;
do not mass-delete transitional modules blindly.
