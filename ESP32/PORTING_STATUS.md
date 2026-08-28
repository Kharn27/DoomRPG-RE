# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md) + [`DOCUMENTATION.md`](DOCUMENTATION.md) + the latest relevant milestone archive override chat memory.

## Latest merged baseline

```text
PR   = #104 — native dialog font hotpath
main = 37d4bc760e0715216f1adaea9d69548a5ab31ab7
status = MERGED
```

PR #104 merged the hardware-proven grouped indexed-BMP dialog-font row-read hotpath on top of the resident native gameplay/dialog engine.

## Current candidate — gameplay interaction expansion

```text
branch = agent/esp32-native-gameplay-force-message
base main = 37d4bc760e0715216f1adaea9d69548a5ab31ab7
hardware-tested implementation SHA = 0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

Latest hardware archive:

- [`MAP1_NATIVE_GAMEPLAY_INTERACTION_EXPANSION.md`](MAP1_NATIVE_GAMEPLAY_INTERACTION_EXPANSION.md) — production FORCE_MESSAGE/NOTE interaction ownership, bounded mixed dialog continuation, native weapon presentation/pickup boundary, systematic corpus diagnostics, and the no-PSRAM mappings reload peak guard.

Previous relevant archives:

- [`MAP1_NATIVE_DIALOG_FONT_HOTPATH.md`](MAP1_NATIVE_DIALOG_FONT_HOTPATH.md)
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

Prefer compact immutable arenas, explicit small mutable owners, bounded caches and small buffers. Post-playing owners should be lazy when practical so they do not consume scarce boot/menu memory.

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

## Generic session / renderer / HUD / cache — hardware proven baseline

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

Graphics catalog and initial frame canon:

```text
textures = 33
sprites = 45 -> 46 after dependency closure
catalog storage = 3120 B
catalog FNV = 29ffc14a
initial world frame = 71ca7465
initial walls = 8 / 4430 pixels
```

Initial HUD canon:

```text
hp = 30/30
armor = 0/20
weapon = 2
ammo = 8
resources = 5
pixels = 7538
```

Permanent render-cache lifecycle canon:

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

Interactive gameplay frames remain view-dependent. `PlatformVideo_present()` is about 34 ms and is not the principal world-render bottleneck; do not optimize it reflexively.

## Native gameplay boundary — REAL-CYD hardware PASS

Production-enabled on Entrance now includes:

```text
12-zone calibrated touch + transient feedback
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
native topology/entity/line collision
dynamic per-line collision
SELECT front-tile resolver/provenance
SELECT EV_OPENLINE / EV_CLOSELINE regular doors
MOVE source EXIT + destination ENTER bounded event routes
regular-door 4-frame visual interpolation
EV_DIALOG / EV_DIALOGNOBACK presenter
progressive dialog/typewriter/paging/fast-forward/close
EV_FORCEMESSAGE top-bar fallback owner/painter
EV_NOTE bounded prefix before dialog
bounded saved dialog continuation (max 12 eligible commands)
post-dialog EV_SHOW / EV_HIDE / EV_UNLOCK
post-dialog state ops 11 / 19 / 20
native idle first-person weapon rendering
post-move eType=5 weapon world-remove/ownership/auto-select overlay
one-shot resident interaction/pickup corpus diagnostics
```

The engine still does not broad-enable legacy `Game_executeEvent`.

### Dialog continuation family

Supported post-dialog opcodes:

```text
7  EV_SHOW
18 EV_HIDE
13 EV_UNLOCK
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Transaction rollback covers script removed bits, state mutation, UNLOCK line lock/texture changes and SHOW/HIDE topology changes. Topology snapshot and the main continuation transaction owner are lazy-gameplay allocations.

A real motivating Entrance event is event 60, whose dialog pause is followed by mixed SHOW/state operations and later by DIALOGNOBACK/HIDE/state/UNLOCK/NEXTSTATE. The old zero-or-one-state continuation preflight rejected this valid legacy shape; the candidate now respects the recovered saved-continuation pause boundary.

### FORCE_MESSAGE / NOTE

`EV_FORCEMESSAGE` now drives the real resident 160x20 top-bar fallback without mutating legacy `Hud`. The owner retains a compact map string reference; text scratch is bounded to 384 B and visible text to 21 characters.

`EV_NOTE` remains a bounded prefix route into owned dialog execution. Its notebook/scratch owner is lazy and acquired only on the first real NOTE+dialog pair.

### Weapon presentation / pickup

The resident frame can paint the current idle first-person weapon from bounded PAK reads using recovered weapon sprite semantics (`logical sprite = 240 + weapon`).

Post-committed-move pickup support is intentionally limited to `eType=5` weapons:

```text
consumed world sprite = native bit overlay
new weapon ownership = native uint16 mask
auto-select = native HUD overlay
rerender/rollback = transactional
owner scope = exact runtime arena FNV + map
```

Still deferred inside the weapon-pickup family:

```text
ammo increment from EntityDef.parm
first-acquisition popup/dialog
sound
```

Other pickup families remain deferred:

```text
eType 3  world/player-stat item
eType 4  inventory item
eType 6  ammo
eType 16 alternate ammo
```

## Interaction corpus / fail-closed frontier

Known Entrance opcode IDs:

```text
2, 7, 8, 9, 10, 11, 13, 15, 16, 18, 19, 24, 26, 27, 40, 41
```

Production-bounded families now include the dialog/UI/state/door/topology routes listed above.

Still intentionally production-deferred/fail-closed:

```text
2  EV_CHANGEMAP  -> transition consumer
9  EV_GIVEMAP    -> automap production promotion
10 EV_PASSWORD   -> password input UI
27 EV_SAVEGAME   -> save consumer
41 EV_CHECK_KEY  -> native player key owner
```

Existing probe/intent APIs for these commands do not authorize live execution.

## No-PSRAM mappings reload peak recovery — hardware accepted

The final candidate initially failed during normal boot when `Render_beginLoadMap(MAP_MENU)` invoked a second `Render_loadMappings()`:

```text
[MAPPINGS] first load success
heap8 = 34036 B
largest8 = 9716 B
mapping payload = 8376 B
then: out of memory allocating inflate state for mappings.bin
```

The legacy loader inflates the new `mappings.bin` before freeing the four previously resident mapping arrays, creating avoidable double residency on the classic CYD.

The ESP32-only bounded guard now releases exactly these immutable arrays immediately before the real `Render_beginLoadMap()`:

```text
mediaTexelOffsets
mediaBitShapeOffsets
mediaTexturesIds
mediaSpriteIds
```

The real `Render_loadMappings()` still parses/rebuilds them and remains the sole owner. No mappings file format or BSP semantics changed.

The same closeout moved the dialog-chain transaction journal out of fixed BSS into a lazy gameplay allocation.

Tester acceptance at implementation SHA `0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc` establishes the corrected normal `esp32-cyd` hardware boundary. No unreported final heap/FNV values are invented here.

## Generic regular-door proof remains canonical

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

## Native dialog baseline remains canonical

Production dialog owner:

```text
text capacity = 384 B
page lines = 4
logical typewriter cadence = 25 ms / character
close provenance owner = 12 B
PAK lease = open only while dialog active, closed before world redraw
```

Previously proven Entrance event 88 behavior and visual fingerprints remain historical regression canons:

```text
page1 complete = 1cf6fa50
page transition/start = 35de63a8
page2 complete = 0741a2e6
world after resume = ed061192
```

The grouped dialog font hotpath remains merged and hardware proven; do not replace it with map-wide decoded font storage.

## Event/script executor boundary

Generic `EspMapOpcodeExecutor` remains intentionally limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Other production semantic families call their own bounded native owners. This separation is intentional.

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
base main = 37d4bc760e0715216f1adaea9d69548a5ab31ab7
hardware-tested implementation SHA = 0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc
resident gameplay = YES
FORCE_MESSAGE production top bar = YES
NOTE prefix = YES
bounded mixed dialog continuation = YES
SHOW/HIDE/UNLOCK continuation = YES
native idle weapon renderer = YES
eType=5 remove/ownership/auto-select = YES
interaction corpus diagnostics = YES
mappings reload peak guard = YES
ammo/acquisition popup = DEFERRED
unsupported opcodes remain fail closed = YES
immutable runtime = YES
shapeData/mediaTexels = NULL
MERGE-READY = YES
```

Every commit after `0c6f7ffac0e3cf47a60aa2315da04f551d2a51bc` must remain documentation-only for this closeout. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
