# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md) + [`DOCUMENTATION.md`](DOCUMENTATION.md) + the latest relevant milestone archive override chat memory.

## Latest merged baseline

```text
PR   = #103 — native Action dialog + resume
main = 7ff701245b0fda41de3cda7bd2fb65cad15eb218
status = MERGED
```

PR #103 merged the bounded production `EV_DIALOG` presenter, Action fast-forward/paging, close and state-only script resume on top of the generic resident gameplay engine.

## Current candidate — dialog font hotpath

```text
branch = agent/esp32-native-dialog-font-hotpath
base main = 7ff701245b0fda41de3cda7bd2fb65cad15eb218
hardware-tested implementation SHA = 777482b038088b232dcbfe64b2421d12aad3de15
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

Latest hardware archive:

- [`MAP1_NATIVE_DIALOG_FONT_HOTPATH.md`](MAP1_NATIVE_DIALOG_FONT_HOTPATH.md) — generic indexed-BMP grouped row reads, 1024-B BSS scratch, dialog fingerprints/semantics preserved and large real-CYD fluidity gain.

Previous relevant archives:

- [`MAP1_NATIVE_GAMEPLAY_DIALOG_RESUME.md`](MAP1_NATIVE_GAMEPLAY_DIALOG_RESUME.md)
- [`MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md`](MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md)
- [`MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md`](MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md)

## Critical engine rule

**A new BSP is not a new engine.**

Production runtime remains:

```text
/DoomRPG-ESP32.pak
 -> compact immutable EspMapRuntime
 -> compact mutable overlays
 -> EspPlayerView
 -> EspNativeGameplaySession
 -> generic renderer/HUD/input/collision/events/actions/dialog UI
```

Historical `MAP1_*`, `ENTRANCE*` and `JUNCTION*` probes/log labels are regression evidence only. Unsupported behavior must be implemented as a bounded generic family and remain fail-closed elsewhere.

After `TransitionPreflightFinal` succeeds, historical startup probes have no runtime blocking authority. `EspNativeGameplaySession` is the live runtime owner.

## Permanent memory / architecture invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
runtime ZIP = forbidden for migrated paths
backing     = /DoomRPG-ESP32.pak
legacy Game.entities = 0
legacy Game.monsters = 0
```

Prefer compact immutable arenas, explicit small mutable owners, bounded caches and small buffers. Never reintroduce map-wide decoded graphics or pointer-heavy desktop ownership just to recover one behavior.

## Entrance resident canon

```text
resourceMapId = 1
file = /intro.bsp
name = Entrance
startupMap = 1
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
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

## Generic session / renderer / HUD / cache — hardware PASS

Initial player/view:

```text
spawn tile = 904
position = 544,1824,36
oldZ = 4
angle = 64
targetMapId = 1
gameplayLoadMapId = 1
source = BSP HEADER
```

Graphics catalog and initial frame:

```text
textures = 33
sprites = 45 -> 46 after dependency closure
catalog storage = 3120 B
catalog FNV = 29ffc14a
initial world frame = 71ca7465
initial walls = 8 / 4430 pixels
```

Initial HUD:

```text
hp = 30/30
armor = 0/20
weapon = 2
ammo = 8
resources = 5
pixels = 7538
```

Permanent render-cache lifecycle:

```text
resident owner = 21160 B
payload = 16384 B
range records = 256
SMALL-COLD  = 2119886 us
SMALL-WARM  = 256807 us
LARGE-LEARN = 247770 us
LARGE-WARM  = 229719 us
payload after prime = 14645 / 16384 B
large entries = 2
```

Interactive gameplay frames remain view-dependent, commonly around 0.2-0.4 s on the current full-frame compositor. `PlatformVideo_present()` is about 34 ms and is not the principal world-render bottleneck.

## Native gameplay boundary — hardware PASS

Production-enabled on Entrance:

```text
12-zone calibrated touch + 120-ms transient feedback
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
native topology/entity/line collision
dynamic per-line collision
SELECT front-tile resolver/provenance
SELECT EV_OPENLINE / EV_CLOSELINE regular doors
MOVE source EXIT + destination ENTER regular-door events
regular-door 4-frame visual interpolation
SELECT EV_DIALOG pause/typewriter/paging/fast-forward/close
zero-or-one state-only dialog continuation using 11/19/20
```

The engine still does not broad-enable legacy `Game_executeEvent`.

Semantically deferred / fail-closed:

```text
EV_FORCEMESSAGE / EV_NOTE production UI semantics
broad MOVE tile-event opcode execution beyond owned door family
unbounded/mixed dialog continuations
secret/MOVELINE animation
door sound playback
legacy entity relink objects
PASS_TURN
menu/automap/weapon gameplay
combat / monsters / generic turn advance
unsupported opcode families
```

## Generic regular-door proof

Entrance OPEN witness:

```text
front tile = 837
event = 86
line = 275
flags = 0x00000205
opcode = 15 / EV_OPENLINE
open = 0 -> 1
locked = 0
stable frame = 7105fa5f
```

MOVE-driven CLOSE witness:

```text
source tile = 838
EXIT flags = 0x00000420
event = 87
opcode = 16 / EV_CLOSELINE
line = 275
open = 1 -> 0
destination tile = 839
ENTER flags = 0x80000408
stable frame = 808e96c7
```

Permanent regular-door animator:

```text
owner = 76 B BSS
max active lines = 8
legacy animFrames = 4
moving frames = 3
step = 16
EspMapRuntime mutation = none
```

OPEN frames:

```text
6c5debde -> 2d05fe08 -> a522f925 -> 7105fa5f
```

CLOSE frames:

```text
35e3784d -> d005cd93 -> 808e96c7 -> 808e96c7
```

## Native Action dialog + resume — hardware PASS

Production dialog owner:

```text
text capacity = 384 B
page lines = 4
logical typewriter cadence = 25 ms / character
close provenance owner = 12 B
PAK lease = open only while dialog active, closed before world redraw
```

Real Entrance event 88, front tile 841:

```text
state 0:
  off0 global252 opcode8  EV_DIALOG string88 = ELIGIBLE
  off1 global253 opcode19 EV_NEXTSTATE       = ELIGIBLE

state 1:
  off2 global254 opcode8  EV_DIALOG string89 = ELIGIBLE
  off3 global255 opcode11 EV_CHANGESTATE     = ELIGIBLE
```

Hardware behavior:

```text
string88 = 102 B / 7 lines
Action during typing -> FASTFORWARD
next Action -> PAGE start=4/7
close -> packClosed=yes
resume opcode19 -> state 0->1
next SELECT -> string89 = 10 B / 1 line
close -> resume opcode11 -> state 1->0
repeated cycle = PASS
```

Known visual fingerprints remain:

```text
page1 complete = 1cf6fa50
page transition/start = 35de63a8
page2 complete = 0741a2e6
world after resume = ed061192
```

Historical startup probes cannot block live gameplay after startup validation, and dialog-owned SELECT is not sent to the historical world SELECT witness.

## Dialog font hotpath — hardware PASS

Hardware-tested implementation SHA: `777482b038088b232dcbfe64b2421d12aad3de15`.

Recovered hotpath:

```text
a.bmp font = 144x72 indexed BMP
glyph = 9x12
filePitch = 72 B
glyph band = 864 B
```

Before this milestone, one glyph caused twelve `readRange(72 B)` calls. The indexed-BMP renderer now groups consecutive source rows into a bounded 1024-B BSS scratch owner:

```text
before = 12 range calls / glyph
now    = 1 range call / 12-row glyph band when it fits
heap allocation = none
```

The optimization is generic to `EspNativeIndexedBmp_blit()` and preserves top-down/bottom-up orientation, palette, transparency, clipping and exact framebuffer output.

Previous 102-B dialog scale:

```text
paints = 22
fontReads = 3482 .. 3650
font/resource bytes = 250626 .. 262722
```

Real-CYD optimized runs:

```text
natural run:
  paints = 34
  fontReads = 583
  logical bytes = 502050

fast-forwarded run:
  paints = 17
  fontReads = 164
  logical bytes = 140034
```

The tester reported the dialog as "hyper fluide". The page fingerprints and state 0->1->0 semantics remained exact.

The logical-byte counter is not a direct physical-SD cost metric after grouping: each range request is larger, while the resident exact-range cache can satisfy repeated 864-B requests. The important hardware result is the large reduction in call/range-lookup count and the subjective fluidity gain.

Memory evidence:

```text
previous dialog heap8 ~= 30916
hotpath heap8 = 29892
expected fixed BSS delta = 1024 B
largest8 = 16372 stable
```

This is a fixed owner, not a leak.

Do not optimize `PlatformVideo_present()` merely because it remains around 34.3 ms. The accepted hotpath fixed the real bounded asset-I/O overhead without changing presentation semantics.

## Event/script boundary

Generic `EspMapOpcodeExecutor` remains intentionally limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Production door and dialog Action routes own separate bounded native semantic families around resident event/filter provenance.

Hardware-proven production UI opcode:

```text
8 EV_DIALOG
```

Implemented but not yet hardware-proven in the same bounded family:

```text
26 EV_DIALOGNOBACK
```

## Historical Junction canon remains valid

Junction is a second hardware corpus for the same generic engine, not an engine identity.

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
HUD frame = ba3e5182
HUD viewport = 9206eb24
HUD bands = 6c2aa46f
HUD stateFNV = 4756db9c
```

## Merge recommendation

```text
REAL-CYD HARDWARE PASS
base main = 7ff701245b0fda41de3cda7bd2fb65cad15eb218
hardware-tested implementation SHA = 777482b038088b232dcbfe64b2421d12aad3de15
Entrance visible/walkable/turnable = YES
sprites/HUD/touch = YES
TURN/MOVE/collision = YES
SELECT regular doors = YES
MOVE regular-door events = YES
generic regular-door animation = YES
EV_DIALOG progressive UI/resume = YES
state 0->1->0 repeated = YES
indexed-BMP grouped font reads = YES
dialog visual fingerprints preserved = YES
dialog subjective fluidity = strong PASS
historical probes cannot block live gameplay = YES
immutable runtime = YES
shapeData/mediaTexels = NULL
MERGE-READY = YES
```

Every commit after `777482b038088b232dcbfe64b2421d12aad3de15` must remain documentation-only for this closeout. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
