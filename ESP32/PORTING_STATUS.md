# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md) + [`DOCUMENTATION.md`](DOCUMENTATION.md) + the latest relevant milestone archive override chat memory.

## Latest merged baseline

```text
PR   = #101 — generic Entrance startup/gameplay route
main = 33b05385771b45acabff6dcf14d1da2c18d1818f
status = MERGED
```

The merged route establishes `/intro.bsp` / Entrance as the first playable map and proves the generic resident-map engine through spawn, renderer, sprites, HUD, cache priming, touch, TURN/MOVE and collision.

## Current candidate — native Action/door execution

```text
branch = agent/esp32-native-action-select-exec
base main = 33b05385771b45acabff6dcf14d1da2c18d1818f
hardware-tested implementation SHA = ed353c0799520b82464c0066c3a53c731488c168
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

Latest hardware archives:

- [`MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md`](MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md)
- [`MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md`](MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md)

## Critical engine rule

**A new BSP is not a new engine.**

Production runtime remains:

```text
/DoomRPG-ESP32.pak
 -> compact immutable EspMapRuntime
 -> compact mutable overlays
 -> EspPlayerView
 -> EspNativeGameplaySession
 -> generic renderer/HUD/input/collision/events/actions
```

Do not create permanent Entrance/Junction/level-specific engines. Unsupported data or behavior must be implemented as a bounded generic family, fail closed elsewhere, and be hardware-tested before broadening.

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

The production Action/SELECT path now resolves the current front tile, preserves event/filter provenance, and executes only the bounded regular-door family when exactly eligible.

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

This is data-driven: tile/event/line identity comes from the resident BSP and compact overlays. There is no Entrance-specific door branch.

SELECT remains fail-closed/deferred for unsupported/complex event semantics. Example event 87 on tile 838 correctly resolves `EV_CLOSELINE` as `FLAGS_MISMATCH` for SELECT and does not mutate.

## Generic MOVE door events — hardware PASS

MOVE now reproduces bounded legacy source EXIT and destination ENTER event phases around a cardinal committed move.

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
rollback lease = closed only after render success
```

Only eligible `EV_OPENLINE` / `EV_CLOSELINE` are owned in this movement-event boundary. Unsupported or complex eligible MOVE semantics remain fail-closed/deferred.

## Generic regular-door animation — hardware PASS

Hardware-tested implementation SHA: `ed353c0799520b82464c0066c3a53c731488c168`.

Permanent native animator:

```text
owner = 76 B BSS
max active lines = 8
legacy animFrames = 4
moving frames = 3
step = 16
EspMapRuntime mutation = none
```

SELECT OPEN sequence on line 275:

```text
FRAME 1/4 moving frame=6c5debde wallPixels=11297
FRAME 2/4 moving frame=2d05fe08 wallPixels=10481
FRAME 3/4 moving frame=a522f925 wallPixels=9665
FRAME 4/4 stable frame=7105fa5f wallPixels=9339
```

MOVE CLOSE sequence on the same line:

```text
FRAME 1/4 moving frame=35e3784d wallPixels=9340
FRAME 2/4 moving frame=d005cd93 wallPixels=9556
FRAME 3/4 moving frame=808e96c7 wallPixels=9652
FRAME 4/4 stable frame=808e96c7 wallPixels=9652
```

Both paths report:

```text
generic=yes
immutableRuntime=yes
render=ok
COMPLETE transitions=1 frames=4 state=stable transaction=committed
```

The world raster and sprite-depth pass both see the same transient animated line geometry. Fully-open lines continue through the generic dynamic line adapter.

Stable heartbeat after the supplied animation run:

```text
heap = 97448
heap8 = 31740
largest8 = 8692
```

No Guru Meditation or reboot was reported.

## Performance observation

The tester considers the door animation correct but slightly slow, similar to navigation latency. This is not a correctness blocker.

In the supplied run:

```text
PlatformVideo_present ~= 34 ms
complete gameplay redraws around this area ~= 0.32-0.35 s
```

Therefore future performance work should target bounded recomposition/cache/redraw scheduling and on-demand redraw, not premature optimization of `PlatformVideo_present()`.

No timing tweak belongs in this hardware-PASS closeout.

## Event/script boundary

Generic `EspMapOpcodeExecutor` remains intentionally limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

The new production Action/MOVE door paths do not broad-enable legacy `Game_executeEvent`; they own a separate bounded regular-door semantic family around resident event/filter provenance.

Still deferred / fail-closed:

```text
SELECT dialog/UI families
broad MOVE tile-event opcode execution
secret/MOVELINE animation
door sound playback
legacy entity relink objects
PASS_TURN
menu/automap/weapon gameplay
combat / monsters / turn advance
unsupported opcode families
```

Two log tokens are now historical/stale rather than semantic truth:

```text
animation=deferred
tileEvents=deferred
```

The regular-door animation and bounded movement-door event path are hardware-live. The strings are intentionally left unchanged in the docs-only closeout so no post-test code commit invalidates the tested SHA.

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
base main = 33b05385771b45acabff6dcf14d1da2c18d1818f
hardware-tested implementation SHA = ed353c0799520b82464c0066c3a53c731488c168
Entrance visible/walkable/turnable = YES
sprites/HUD/touch = YES
TURN/MOVE/collision = YES
SELECT EV_OPENLINE = YES
MOVE EXIT EV_CLOSELINE = YES
generic regular-door animation = YES
immutable runtime = YES
shapeData/mediaTexels = NULL
MERGE-READY = YES
```

Every commit after `ed353c0799520b82464c0066c3a53c731488c168` must remain documentation-only for this closeout. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
