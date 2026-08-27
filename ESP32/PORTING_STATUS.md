# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md) + [`DOCUMENTATION.md`](DOCUMENTATION.md) + the latest relevant milestone archive override chat memory.

## Latest merged baseline

```text
PR   = #102 — native Action/regular-door execution
main = a8e0b64dfd9c790f8896279f70427ce6fb3e9859
status = MERGED
```

PR #102 merged the generic resident Action door route developed on top of the already hardware-proven Entrance startup/gameplay engine.

## Current candidate — native Action dialog + resume

```text
branch = agent/esp32-native-action-dialog-resume
base main = a8e0b64dfd9c790f8896279f70427ce6fb3e9859
hardware-tested implementation SHA = 5c53a9a02bfb7c92e4dccb0b6eba424e7d015a9b
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

Latest hardware archive:

- [`MAP1_NATIVE_GAMEPLAY_DIALOG_RESUME.md`](MAP1_NATIVE_GAMEPLAY_DIALOG_RESUME.md) — bounded production `EV_DIALOG`, progressive text, paging/fast-forward, close, preflighted state-only continuation, repeated state-gated second dialog.

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

After `TransitionPreflightFinal` succeeds, historical startup probes have **no runtime blocking authority**. The live generic gameplay session is the production owner.

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

## Initial player/view canon

```text
spawn tile = 904
tileXY = 8,28
position = 544,1824,36
oldZ = 4
angle = 64
targetMapId = 1
gameplayLoadMapId = 1
source = BSP HEADER
```

## Generic renderer/HUD/cache hardware proof

Entrance initial graphics catalog:

```text
textures = 33
sprites = 45
storage = 3120 B
FNV = 29ffc14a
```

After dependency closure: sprites = 46.

Initial world frame:

```text
map = 1
angle = 64
frame = 71ca7465
walls = 8
wallPixels = 4430
presented = yes
```

Initial HUD:

```text
hp = 30/30
armor = 0/20
weapon = 2
ammo = 8
resources = 5
pixels = 7538
presented = yes
```

Permanent render-cache lifecycle is hardware-proven:

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

## Native gameplay hardware proof

Production-enabled on Entrance:

```text
12-zone calibrated touch + 120 ms transient feedback
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
native topology/entity/line collision
dynamic per-line collision
SELECT front-tile resolver/provenance
bounded SELECT regular-door execution
bounded MOVE EXIT/ENTER regular-door events
regular-door visual interpolation
bounded SELECT EV_DIALOG pause/paging/resume
```

Movement/turn examples remain stable:

```text
904 -> 872 -> 840
angle 64 -> 0 -> 192 -> 128 -> 64
```

Closed/locked Entrance collision witness:

```text
source = 904
dest = 936
line = 258
texture = 7
flags = 0x00000505
result = BLOCKED
```

## Generic SELECT regular-door execution — hardware PASS

Real-CYD Entrance witness:

```text
front = 352,1696
tile = 837
event = 86
line = 275
line flags = 0x00000205
opcode = 15 / EV_OPENLINE
open = 0 -> 1
locked = 0
final open frame = 7105fa5f
```

This route is data-driven from resident BSP/event/line state. No Entrance-specific door branch exists.

## Generic MOVE door events + animation — hardware PASS

Recovered movement flags:

```text
+X EXIT 0x20 / ENTER 0x08
-X EXIT 0x80 / ENTER 0x02
-Y EXIT 0x10 / ENTER 0x04
+Y EXIT 0x40 / ENTER 0x01
both phases include 0x400
ENTER also includes cardinal facing flag
```

Real-CYD close witness after opening line 275:

```text
source tile = 838
EXIT flags = 0x00000420
event = 87
opcode = 16 / EV_CLOSELINE
line = 275
open = 1 -> 0
destination tile = 839
ENTER flags = 0x80000408
final closed frame = 808e96c7
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

OPEN on line 275:

```text
6c5debde -> 2d05fe08 -> a522f925 -> 7105fa5f stable
```

CLOSE on line 275:

```text
35e3784d -> d005cd93 -> 808e96c7 -> 808e96c7 stable
```

## Native Action dialog + resume — hardware PASS

Hardware-tested implementation SHA: `5c53a9a02bfb7c92e4dccb0b6eba424e7d015a9b`.

Permanent bounded dialog owner:

```text
text capacity = 384 B
page lines = 4
typewriter cadence = 25 ms / character
close provenance owner = 12 B
PAK lease = open only while dialog is active, closed before world redraw
```

The Action executor accepts a dialog only when the filtered event begins with an eligible `EV_DIALOG` / `EV_DIALOGNOBACK` and has at most one eligible state-only continuation already owned by `EspMapOpcodeExecutor` (11/19/20). Anything broader remains fail-closed before UI presentation.

The real-CYD proof covers `EV_DIALOG` opcode 8 on Entrance event 88.

State 0:

```text
off 0 global252 opcode 8  EV_DIALOG     string88 = ELIGIBLE
off 1 global253 opcode 19 EV_NEXTSTATE          = ELIGIBLE
off 2 global254 opcode 8  string89              = STATE_MISMATCH
off 3 global255 opcode 11                       = STATE_MISMATCH
```

First dialog:

```text
string = 88
bytes = 102
lines = 7
back = 1
FASTFORWARD pageStart=0 lines=4 = PASS
PAGE start=4/7 = PASS
FASTFORWARD pageStart=4 lines=3 = PASS
CLOSE packClosed=yes = PASS
RESUME opcode19 state 0->1 = PASS
```

State 1 then exposes the second pair:

```text
off 2 global254 opcode 8  EV_DIALOG string89 = ELIGIBLE
off 3 global255 opcode 11 EV_CHANGESTATE      = ELIGIBLE
```

Second dialog:

```text
string = 89
bytes = 10
lines = 1
CLOSE packClosed=yes = PASS
RESUME opcode11 state 1->0 = PASS
```

The tester repeated the complete state 0 -> 1 -> 0 cycle several times successfully.

World redraw after dialog resume remained generic and successful:

```text
frame = ed061192
sprites = 6 / 6776
walls = 3
wallPixels = 7288
totalUs ~= 206700
presented = yes
```

Stable heartbeat during repeated dialog cycles:

```text
heap = 96624
heap8 = 30916
largest8 = 16372
```

No later `[NATIVEBOOT] BLOCKED`, stuck input, Guru Meditation or reboot was reported.

## Historical-probe runtime authority fix — hardware PASS

An earlier dialog run proved a stale architectural leak: the historical SELECT front-tile observer was invoked for a dialog-owned SELECT while the dialog legitimately kept the PAK open. Its old precondition failed, set the global probe blocking flag, and the startup bridge still treated that flag as authoritative in live gameplay.

The tested implementation fixes this permanently:

```text
before TransitionPreflightFinal = startup probes may block startup validation
after  TransitionPreflightFinal = startup probes are witnesses only
active dialog SELECT             = not sent to world/front-tile SELECT witness
EspNativeGameplaySession         = runtime authority
```

## Performance observation

Correctness is hardware-valid, but the tester reports occasional noticeable dialog/navigation latency.

Measured dialog scale:

```text
PlatformVideo_present ~= 34.3 ms
102-B / 7-line dialog close run:
  paints = 22
  fontReads = 3482 .. 3650
  font/resource bytes = 250626 .. 262722
DIALOG-RESUME world redraw ~= 206.7 ms
transient touch feedback = 120 ms
```

The thousands of font reads for a 102-byte text are an obvious bounded performance target. Future work should reduce redundant font/resource reads and presentation/recomposition cadence while preserving the logical 25-ms typewriter behavior.

Do **not** prematurely optimize `PlatformVideo_present()`. Doom RPG remains turn-based and should continue toward bounded redraw-on-demand.

## Event/script boundary

Generic `EspMapOpcodeExecutor` remains intentionally limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Production door and dialog Action routes do not broad-enable legacy `Game_executeEvent`; they own bounded native semantic families around resident event/filter provenance.

Hardware-proven dialog opcode in production:

```text
8 EV_DIALOG
```

Implemented but not yet hardware-proven in the same bounded dialog family:

```text
26 EV_DIALOGNOBACK
```

Still deferred / fail-closed:

```text
EV_FORCEMESSAGE / EV_NOTE production UI semantics
broad MOVE tile-event opcode execution
unbounded/mixed dialog continuations
secret/MOVELINE animation
door sound playback
legacy entity relink objects
PASS_TURN
menu/automap/weapon gameplay
combat / monsters / generic turn advance
unsupported opcode families
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
base main = a8e0b64dfd9c790f8896279f70427ce6fb3e9859
hardware-tested implementation SHA = 5c53a9a02bfb7c92e4dccb0b6eba424e7d015a9b
Entrance visible/walkable/turnable = YES
sprites/HUD/touch = YES
TURN/MOVE/collision = YES
SELECT regular doors = YES
MOVE regular-door events = YES
generic regular-door animation = YES
EV_DIALOG progressive UI = YES
Action fast-forward/paging = YES
EV_NEXTSTATE resume 0->1 = YES
state-gated second dialog = YES
EV_CHANGESTATE resume 1->0 = YES
repeated dialog cycle = YES
historical probes cannot block live gameplay = YES
immutable runtime = YES
shapeData/mediaTexels = NULL
MERGE-READY = YES
```

Every commit after `5c53a9a02bfb7c92e4dccb0b6eba424e7d015a9b` must remain documentation-only for this closeout. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
