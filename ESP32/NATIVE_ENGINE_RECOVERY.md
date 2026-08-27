# Doom RPG ESP32-native generic engine recovery

This document is the compact recovery contract for the permanent native engine. It synthesizes the historical `MAP1_*.md` archive into the production architecture that every resident BSP must use.

The milestone files are evidence. They are not runtime architecture.

## Source of truth

Recover in this order:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md);
3. this file;
4. [`DOCUMENTATION.md`](DOCUMENTATION.md);
5. only then the specific `MAP1_*.md` archive needed for a behavior or hardware witness.

If chat history and repository state disagree, the repository wins.

## Fundamental engine rule

**A new map is not a new engine.**

```text
/DoomRPG-ESP32.pak
 -> stream/parse requested BSP
 -> compact immutable EspMapRuntime
 -> bounded mutable overlays
 -> EspPlayerView
 -> EspNativeGameplaySession
 -> renderer / HUD / input / collision / events / actions / UI
```

Do not add permanent `LEVEL_X_*`, `ENTRANCE_*` or `JUNCTION_*` gameplay pipelines because a different BSP is loaded.

A map may expose a previously unsupported data or behavior family. That is a bounded generic engine extension:

```text
recover exact legacy behavior
 -> design small permanent native owner/API
 -> fail closed outside supported cases
 -> add strict regression witness
 -> test on real CYD
 -> document only after PASS
```

## Runtime authority

Historical probes are executable regression witnesses only.

```text
before TransitionPreflightFinal:
  startup probes may fail-closed startup validation

after TransitionPreflightFinal:
  startup probes have no blocking authority
  EspNativeGameplaySession is the runtime owner
```

A historical probe must never stop a live gameplay session again.

## Permanent hardware / memory invariants

```text
board = ESP32-2432S028R classic CYD
MCU = ESP32-D0WD-V3 dual core 240 MHz
flash = 4 MB
PSRAM = none
logical framebuffer = 160x120 RGB565 = 38400 B
shapeData == NULL
mediaTexels == NULL
runtime ZIP forbidden for migrated paths
native backing store = /DoomRPG-ESP32.pak
legacy Game.entities == 0
legacy Game.monsters == 0
```

Prefer compact immutable arenas, explicit small mutable owners, bounded caches and small buffers. Never reintroduce map-wide decoded graphics or pointer-heavy desktop ownership merely to recover one behavior.

## Hardware-proven generic session

Entrance `/intro.bsp` proved that the engine previously validated through Junction is truly map-generic when its owners are initialized explicitly.

Canonical Entrance resident facts:

```text
resourceMapId = 1
file = /intro.bsp
name = Entrance
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
runtimeFNV = c3882516
runtime arena = 14095 B
spawn tile = 904
position = 544,1824,36
angle = 64
```

Owner fingerprints:

```text
mapStateFNV = cd99b98e
scriptFNV = f9e3d9df
lineFNV = e5e74861
textureFNV = f1fc1875
automapFNV = 669b1aa7
topologyFNV = 3f321e43
```

The required gameplay-session lifecycle is:

```text
resident EspMapRuntime
 -> settled EspPlayerView
 -> EspNativeGraphicsCatalog
 -> first world frame
 -> initial HUD
 -> sprite dependency closure
 -> resident PAK cache
 -> SMALL-COLD
 -> SMALL-WARM
 -> enable exact 2048-B shared-payload ranges
 -> LARGE-LEARN
 -> LARGE-WARM
 -> entity-definition collision catalog
 -> resident gameplay service
 -> touch / TURN / MOVE / SELECT / dialog UI
```

Render-cache hardware canon:

```text
owner = 21160 B
payload = 16384 B
range records = 256
SMALL-COLD  = 2119886 us
SMALL-WARM  = 256807 us
LARGE-LEARN = 247770 us
LARGE-WARM  = 229719 us
large entries = 2
payload used = 14645 / 16384 B
```

`PlatformVideo_present()` is about 34 ms. Do not treat it as the default optimization target.

## Permanent runtime owners

### Resident map

```text
EspMapRuntime                immutable BSP-derived arena
EspMapState                  compact mutable map state
EspMapScriptState            event state + removed-command bits
EspMapLineState              mutable line open/closed state
texture/automap state        bounded mutable overlays
native topology              compact entity/line topology
```

No map-wide `shapeData` or `mediaTexels`.

### Player/view

`EspPlayerView` is the generic player/camera owner for the current resident map. Spawn and transition logic prepare it; renderer and gameplay consume it. No renderer or input path should infer a hardcoded level identity.

### Graphics / renderer

`EspNativeGraphicsCatalog` maps only graphics required by the current resident runtime. Sprite dependencies are expanded explicitly.

The generic compositor owns:

```text
160x80 world viewport at y=20
textured floor/ceiling
walls
dynamic per-line render state
bounded BSP-visible sprites/glows
HUD direction repaint
one final 160x120 presentation per world redraw
```

The plane renderer uses bounded 2048-B leases. It must not depend on one contiguous 12288-B allocation.

### HUD

The native HUD is resident-map generic. Entrance hardware proof includes:

```text
hp = 30/30
armor = 0/20
weapon = 2
ammo = 8
resources = 5
pixels = 7538
```

### Input

The hardware-proven input layer is the invisible 12-zone CYD layout with 120-ms transient touch feedback. Touch callbacks queue intents; gameplay semantics execute in the resident gameplay service.

### Collision

Movement collision consumes resident topology + entity-definition types + mutable line state. Open regular doors are immediately non-blocking because renderer and collision share the same `EspMapLineState` owner.

## Gameplay semantics now hardware-proven

Production-usable:

```text
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
static/entity/line collision
dynamic line collision
SELECT front-tile event provenance
SELECT regular EV_OPENLINE / EV_CLOSELINE
MOVE source EXIT + destination ENTER regular-door events
regular-door visual interpolation
SELECT EV_DIALOG pause/typewriter/paging/fast-forward/close
state-only dialog continuation through opcodes 11/19/20
```

SELECT is no longer merely an observer. The observer remains a regression witness for world SELECT provenance; production Action execution is owned by the native gameplay Action/dialog paths.

The engine still does **not** broad-enable legacy `Game_executeEvent`.

## Generic regular doors

Entrance hardware witnesses prove data-driven regular-door behavior with no map-specific branch.

OPEN:

```text
SELECT tile837
 -> event86
 -> EV_OPENLINE
 -> line275
 -> open 0->1
```

MOVE-driven CLOSE:

```text
EXIT tile838 flags=0x00000420
 -> event87
 -> EV_CLOSELINE
 -> line275
 -> open 1->0
```

Regular-door animation owner:

```text
76 B BSS
max active lines = 8
4 legacy frames
3 moving frames
step = 16
EspMapRuntime remains immutable
```

Known OPEN frames:

```text
6c5debde -> 2d05fe08 -> a522f925 -> 7105fa5f
```

Known CLOSE frames:

```text
35e3784d -> d005cd93 -> 808e96c7 -> 808e96c7
```

Secret/MOVELINE doors remain a separate unsupported family.

## Generic Action dialog + resume

The bounded dialog family owns `EV_DIALOG` and implements `EV_DIALOGNOBACK` through the same API, though only opcode 8 is hardware-proven so far.

Permanent bounded dialog state:

```text
text capacity = 384 B
page = 4 lines
logical typewriter cadence = 25 ms/character
close provenance owner = 12 B
PAK lease stays logically open only while dialog active
```

A dialog is accepted only when the filtered event begins with an eligible dialog command and has zero or one eligible state-only continuation already supported by `EspMapOpcodeExecutor`.

Entrance event 88 proves the complete state cycle:

```text
state 0:
  EV_DIALOG string88
  EV_NEXTSTATE

Action/paging/close
 -> EV_NEXTSTATE state 0->1

state 1:
  EV_DIALOG string89
  EV_CHANGESTATE

Action/close
 -> EV_CHANGESTATE state 1->0
```

Known dialog fingerprints:

```text
page1 complete = 1cf6fa50
page transition/start = 35de63a8
page2 complete = 0741a2e6
world after resume = ed061192
```

The user repeatedly exercised state 0->1->0 on real hardware.

## Dialog/font hotpath

Hardware-tested implementation:

```text
base main = 7ff701245b0fda41de3cda7bd2fb65cad15eb218
SHA = 777482b038088b232dcbfe64b2421d12aad3de15
status = REAL-CYD HARDWARE PASS
```

The previous correct dialog renderer called `EspAssetPack_readRange()` once per BMP source row. Doom's `a.bmp` font uses:

```text
source = 144x72 indexed BMP
glyph = 9x12
filePitch = 72 B
12-row glyph band = 864 B
```

`EspNativeIndexedBmp_blit()` now groups consecutive source rows into a permanent bounded 1024-B BSS scratch:

```text
before = 12 x readRange(72 B) per glyph
now    = 1 x readRange(864 B) for a full glyph band
heap allocation = none
```

This optimization is generic to indexed-BMP blits. It preserves top-down/bottom-up orientation, palette, transparency, clipping and exact framebuffer output.

Hardware before:

```text
102-B / 7-line dialog
paints = 22
fontReads = 3482 .. 3650
```

Hardware after:

```text
natural run: paints=34 fontReads=583
fast-forward run: paints=17 fontReads=164
```

The user described the result as "hyper fluide". The known dialog FNVs and script state cycle remained unchanged.

Memory cost is exact and bounded:

```text
previous dialog heap8 ~= 30916
hotpath heap8 = 29892
fixed delta = 1024 B
largest8 = 16372 stable
```

Do not replace this with a large decoded-font pool. If later profiling requires another UI optimization, incremental glyph painting is a possible separate bounded milestone, not a requirement for the accepted current boundary.

## Event/script architecture

`EspMapOpcodeExecutor` remains intentionally limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Door and dialog production semantics are separate bounded native owners around resident event/filter provenance.

Currently hardware-proven production UI opcode:

```text
8 EV_DIALOG
```

Implemented but not yet hardware-proven:

```text
26 EV_DIALOGNOBACK
```

Still deferred / fail-closed:

```text
EV_FORCEMESSAGE / EV_NOTE production UI semantics
broad MOVE event execution outside owned families
unbounded/mixed dialog continuations
secret/MOVELINE animation
door sound playback
legacy entity relink objects
PASS_TURN
menu / automap / weapon gameplay
combat / monsters / generic turn advance
unsupported opcode families
```

## How to add the next BSP

For every future level:

```text
1. stream BSP from /DoomRPG-ESP32.pak
2. validate parse bounds/inventory
3. publish the same compact resident owners
4. prepare spawn/transition EspPlayerView
5. enter the same EspNativeGameplaySession
6. play
```

If a new level fails, identify the first unsupported data or semantic family and extend the engine generically. Do not fork the engine by level.

## Historical Junction canon

Junction remains a second hardware corpus for this same engine:

```text
resource = /junction.bsp
resourceMapId = 9
gameplayLoadMapId = 2
sourceFNV = fefaf5ca
runtimeFNV = bc432a0f
lineState baseline FNV = 3658710d
fresh player = 992,1888,36
angle = 64
tile = 943
HUD frame = ba3e5182
HUD viewport = 9206eb24
HUD bands = 6c2aa46f
HUD stateFNV = 4756db9c
```

## Current merge boundary

```text
branch = agent/esp32-native-dialog-font-hotpath
base main = 7ff701245b0fda41de3cda7bd2fb65cad15eb218
hardware-tested implementation SHA = 777482b038088b232dcbfe64b2421d12aad3de15
REAL-CYD HARDWARE PASS
Entrance visible/walkable/turnable = YES
sprites/HUD/touch/TURN/MOVE/collision = YES
SELECT regular doors = YES
MOVE regular-door events = YES
regular-door animation = YES
EV_DIALOG progressive UI/resume = YES
state cycle 0->1->0 = YES
indexed-BMP grouped font hotpath = YES
dialog fluidity = strong PASS
shapeData/mediaTexels = NULL
MERGE-READY = YES after docs-only closeout
```

All commits after `777482b038088b232dcbfe64b2421d12aad3de15` must remain documentation-only for this milestone closeout.
