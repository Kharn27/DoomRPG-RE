# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Architecture belongs in [`ARCHITECTURE.md`](ARCHITECTURE.md); this file keeps
the current Git boundary, hardware facts, canonical resident witnesses and the
explicit fail-closed frontier.

Repository state wins over chat history.

## Git boundary

Latest merged baseline:

```text
PR   = #109 — ESP32 legacy pre-render startup cleanup
main = fc6a271490bd06dad4ae8264ba106281864b53e1
status = MERGED
```

Current branch:

```text
branch = agent/esp32-legacy-config-mappings-startup
base main = fc6a271490bd06dad4ae8264ba106281864b53e1
PASS 1 physical implementation rename = 5e678d49610a98bdc0600b9af640f05b1ce2dd9b
hardware-tested final code HEAD = 571afd1b0665e74ad15f8ae5365433041c55a23e
status = REAL-CYD HARDWARE PASS
merge-ready = YES
```

This branch retires `config_mappings_probe.*` as the permanent production name
for the retained compatibility stage immediately after Render startup. The live
stage is now owned by:

```text
ESP32/src/esp_legacy_config_mappings_startup.c
ESP32/include/esp_legacy_config_mappings_startup.h
EspLegacyConfigMappingsStartup_start()
```

Its behavior is intentionally unchanged:

```text
EspRenderStartupBridge_start()
 -> Game_loadConfig()
 -> inspect mappings.bin allocation plan
 -> Render_loadMappings()
 -> validate the four mapping arrays/counts
 -> stop before Render_beginLoadMap()/BSP loading
```

A missing `Config` file remains valid on first boot. `mappings.bin` is still a
retained ZIP compatibility dependency. The separate
`esp_render_mapping_reload_guard.c` remains unchanged and still frees only the
four immutable mapping arrays immediately before `Render_beginLoadMap()` so the
legacy reload does not overlap them with the compressed/inflated mapping payload.

### Hardware evidence for config/mappings startup cleanup

PASS 1 at `5e678d49610a98bdc0600b9af640f05b1ce2dd9b` was a physical source rename only:

```text
ESP32/src/config_mappings_probe.c
 -> ESP32/src/esp_legacy_config_mappings_startup.c
```

GitHub detected 0 additions, 0 deletions and 0 content changes. The source blob
remained `f03d0cec214170f24e1587ea6585569a70604102`.

The real classic CYD normal `esp32-cyd` firmware passed with the retained mapping
plan and startup route intact:

```text
[MAPPINGS] Header texelOffsets=592 bitShapeOffsets=1300 textures=152 sprites=252
[MAPPINGS] Plan payload=8376B largestAlloc=5200B
[MAPPINGS] Render_loadMappings result=1
[CONFIGMAP] READY config path exercised and mappings resident
[MAPPINGS] RELEASE-BEFORE-MAP ... reason=bound-inflate-peak
```

PASS 2 at final code HEAD `571afd1b0665e74ad15f8ae5365433041c55a23e`
introduced the permanent API/header, switched the implementation and `main.cpp`
to that API directly, and physically removed `config_mappings_probe.h`. No
config behavior, mapping parser/allocation rule, reload guard or diagnostic log
semantics changed.

The same real-CYD run continued through the bounded intro and native Entrance
session with canonical runtime ownership intact:

```text
[NATIVEBOOT] READY map=1 gameplayLoadMapId=1 spawnTile=904 pos=544,1824 angle=64
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430
[ENGINECACHE] OWNER bytes=21160 payload=16384 entries=256
[ENGINECACHE] PRIMED ... largeEntries=2 heap8=27112 largest8=16372
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=92816 heap8=27052 largest8=16372
PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready
```

No visible/apparent `FAILED`, panic or reboot was reported. This establishes
`571afd1b` as the hardware-tested code boundary for this branch. Commits after
it must remain documentation-only.

## Cleanup / diagnostic guardrail

Cleanup is **not** a search-and-delete pass for files containing `probe`.

```text
esp32-cyd          = production/runtime hardware authority
esp32-cyd-bringup  = optional diagnostic profile; may intentionally retain probes
```

A probe-named file is cleanup debt only when it has become a real always-built
production implementation/consumer whose permanent responsibility is already
known. Bringup-only diagnostics, regression indicators and bounded observability
may keep probe naming while they remain useful. Do not remove a diagnostic merely
because the engine is progressing; first prove whether normal `esp32-cyd`,
`esp32-cyd-bringup`, linker wrappers or recovery workflows still consume it.

Bringup heap figures are not production RAM canons because extra instrumentation
can perturb memory.

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
PR #109  pre-render startup naming retired into EspLegacyPrerenderStartup_start()
```

Current config/mappings compatibility cleanup:

```text
PASS 1  physical source rename, bit-for-bit
        -> 5e678d49
        -> real-CYD PASS

PASS 2  permanent API + direct consumers + old header removal
        -> 571afd1b
        -> real-CYD PASS

RESULT  branch agent/esp32-legacy-config-mappings-startup = MERGE-READY
```

After merge, recover the new exact GitHub `main` SHA before starting another
branch. The next cleanup candidate must be **audited**, not assumed: determine
whether a remaining probe is a production owner, a bringup-only diagnostic, a
linker/regression witness, or still-needed development instrumentation before
renaming or removing it.
