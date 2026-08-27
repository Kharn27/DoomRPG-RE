# ESP32 documentation map

This file indexes the current classic-CYD Doom RPG port documentation.

## Source of truth

For recovery, use:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md);
3. [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md);
4. this file;
5. the latest relevant milestone archive.

If chat history and repository state disagree, the repository wins.

## Latest merged boundary

```text
PR   = #103 — native Action dialog + resume
main = 7ff701245b0fda41de3cda7bd2fb65cad15eb218
status = MERGED
```

## Current candidate boundary

```text
branch = agent/esp32-native-dialog-font-hotpath
base main = 7ff701245b0fda41de3cda7bd2fb65cad15eb218
hardware-tested implementation SHA = 777482b038088b232dcbfe64b2421d12aad3de15
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

Latest hardware archive:

- [`MAP1_NATIVE_DIALOG_FONT_HOTPATH.md`](MAP1_NATIVE_DIALOG_FONT_HOTPATH.md) — generic indexed-BMP grouped row reads, fixed 1024-B BSS scratch, exact dialog visual/semantic preservation and strong real-CYD fluidity improvement.

Previous relevant archives:

- [`MAP1_NATIVE_GAMEPLAY_DIALOG_RESUME.md`](MAP1_NATIVE_GAMEPLAY_DIALOG_RESUME.md)
- [`MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md`](MAP1_NATIVE_GENERIC_DOOR_ANIMATION.md)
- [`MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md`](MAP1_NATIVE_GENERIC_DOOR_MOVE_CLOSE.md)

## Permanent architectural conclusion

```text
A NEW BSP IS NOT A NEW ENGINE.
```

Production runtime is:

```text
/DoomRPG-ESP32.pak
 -> compact resident EspMapRuntime
 -> compact mutable overlays
 -> EspPlayerView
 -> EspNativeGameplaySession
 -> generic renderer/HUD/input/collision/events/actions/dialog UI
```

Historical `MAP1_*`, `ENTRANCE*` and `JUNCTION*` names are evidence only. After startup validation, probes are witnesses only and cannot stop live gameplay.

## Current real-CYD hardware proof

Entrance startup/playback canon:

```text
file=/intro.bsp
name=Entrance
resourceMapId=1
spawn tile=904
position=544,1824,36
angle=64
```

Generic session has reached:

```text
first world frame = YES
sprites/glows = YES
native HUD = YES
resident render caches = YES
touch = YES
TURN/MOVE = YES
native collision = YES
SELECT front-tile provenance = YES
SELECT regular-door execution = YES
MOVE EXIT/ENTER regular-door events = YES
regular-door animation = YES
SELECT EV_DIALOG = YES
progressive dialog text = YES
Action fast-forward = YES
4-line paging = YES
dialog close + script resume = YES
generic indexed-BMP font hotpath = YES
shapeData = NULL
mediaTexels = NULL
legacy entities = 0
legacy monsters = 0
```

## Current gameplay boundary

Production-enabled:

```text
resident load / spawn / player-view
planes / walls / sprites / glows / HUD
12-zone calibrated touch + transient 120-ms feedback
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
static/entity/line collision
dynamic per-line collision
SELECT front-tile resolver/provenance
bounded SELECT EV_OPENLINE / EV_CLOSELINE
bounded MOVE source EXIT + destination ENTER regular-door events
generic regular-door visual interpolation
bounded SELECT EV_DIALOG / EV_DIALOGNOBACK preflight path
native dialog owner/typewriter/paging/fast-forward/close
zero-or-one state-only dialog continuation via 11/19/20
bounded indexed-BMP grouped row-read hotpath
```

Hardware-proven production UI opcode:

```text
8 EV_DIALOG
```

Implemented in the same bounded dialog family but not yet hardware-proven:

```text
26 EV_DIALOGNOBACK
```

The engine still does not broad-enable legacy `Game_executeEvent`.

Semantically deferred / fail-closed:

```text
EV_FORCEMESSAGE / EV_NOTE production UI semantics
broad MOVE tile-event execution beyond owned door family
unbounded/mixed dialog continuations
secret/MOVELINE animation
door sound playback
legacy entity relink objects
PASS_TURN
menu/automap/weapon gameplay
combat / monsters / generic turn advance
unsupported opcode families
```

## Native dialog proof

Entrance event 88, front tile 841:

```text
state 0:
  cmd0 opcode8  EV_DIALOG string88 = ELIGIBLE
  cmd1 opcode19 EV_NEXTSTATE       = ELIGIBLE
state 1:
  cmd2 opcode8  EV_DIALOG string89 = ELIGIBLE
  cmd3 opcode11 EV_CHANGESTATE     = ELIGIBLE
```

Hardware sequence:

```text
string88 = 102 B / 7 lines
Action during typewriter -> FASTFORWARD
next Action -> PAGE start=4/7
close -> packClosed=yes
resume -> EV_NEXTSTATE state 0->1
second SELECT -> string89 = 10 B / 1 line
close -> EV_CHANGESTATE state 1->0
repeated cycle = PASS
```

Known dialog fingerprints:

```text
page1 complete = 1cf6fa50
page transition/start = 35de63a8
page2 complete = 0741a2e6
world after resume = ed061192
```

## Dialog font hotpath proof

The previous correct dialog renderer was I/O-chatty because every 9x12 glyph called `EspAssetPack_readRange()` once for each of its 12 BMP rows.

For `a.bmp`:

```text
source = 144x72 indexed BMP
filePitch = 72 B
glyph band = 12 rows = 864 B
```

The hardware-tested generic optimization changes the indexed-BMP blit path to group consecutive source rows into one bounded 1024-B BSS scratch owner:

```text
before = 12 readRange(72 B) calls / glyph
now    = 1 readRange(864 B) call / glyph band when it fits
BSS = 1024 B
heap allocation = none
```

No dialog/script/input semantics changed and no map-wide decoded font pool was introduced.

Previous large-dialog scale:

```text
paints = 22
fontReads = 3482 .. 3650
font/resource bytes = 250626 .. 262722
```

Optimized real-CYD runs:

```text
natural typing:
  paints = 34
  fontReads = 583
  logical bytes = 502050

with earlier fast-forward:
  paints = 17
  fontReads = 164
  logical bytes = 140034
```

The user reported the optimized dialog as "hyper fluide". Page FNVs and the event state 0->1->0 cycle remained exact.

The logical-byte counter is not physical SD traffic after grouping: grouped requests are larger, and repeated 864-B exact ranges can be served by the resident PAK cache. The decisive result is the collapse in range-call count plus the real-CYD fluidity gain.

Memory evidence:

```text
previous dialog heap8 ~= 30916
hotpath heap8 = 29892
fixed delta = 1024 B
largest8 = 16372 stable
```

## Generic door proof

Entrance OPEN:

```text
SELECT tile837 -> event86 -> EV_OPENLINE line275 -> 0->1
```

MOVE CLOSE:

```text
EXIT tile838 flags=0x00000420 -> event87 -> EV_CLOSELINE line275 -> 1->0
```

Regular-door animation remains hardware-valid:

```text
OPEN  6c5debde -> 2d05fe08 -> a522f925 -> 7105fa5f
CLOSE 35e3784d -> d005cd93 -> 808e96c7 -> 808e96c7
```

Permanent animator owner = 76 B BSS, max 8 lines, immutable `EspMapRuntime`.

## Render-cache canon

```text
owner=21160 B
payload=16384 B
SMALL-COLD=2119886 us
SMALL-WARM=256807 us
LARGE-LEARN=247770 us
LARGE-WARM=229719 us
large entries=2
```

The plane renderer uses bounded 2048-B leases rather than one contiguous map-wide texture allocation.

## Stable Entrance canons

```text
sourceBytes=21823
crc32=623f34e4
sourceFNV=d5cc751f
runtimeFNV=c3882516
snapshotFNV=b3811f3d
mapFNV=cd99b98e
scriptFNV=f9e3d9df
lineFNV=e5e74861
textureFNV=f1fc1875
automapFNV=669b1aa7
topologyFNV=3f321e43
spawn=904
direction=64
```

## Event/script executor boundary

Generic `EspMapOpcodeExecutor` remains intentionally limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

Door and dialog production paths are separate bounded native semantic owners around resident event/filter provenance.

## Historical Junction canons remain valid

Junction is a second hardware corpus for the same engine, not the engine identity.

```text
sourceFNV=fefaf5ca
runtimeFNV=bc432a0f
lineState baseline FNV=3658710d
fresh player=992,1888,36
angle=64
tile=943
HUD frame=ba3e5182
HUD viewport=9206eb24
HUD bands=6c2aa46f
HUD stateFNV=4756db9c
```

## Permanent invariants

```text
classic CYD / ESP32-D0WD-V3
4 MB flash
no PSRAM
160x120 RGB565 framebuffer = 38400 B
shapeData=NULL
mediaTexels=NULL
runtime ZIP forbidden for migrated paths
/DoomRPG-ESP32.pak is native backing store
legacy Game.entities=0
legacy Game.monsters=0
unsupported semantic families fail closed
```

## Recommended next direction

The dialog hotpath is now a coherent accepted boundary. Do not continue optimizing it merely because further micro-optimizations are possible.

After merge and exact `main` recovery, prefer the next real gameplay semantic exposed by resident data, likely one of:

```text
EV_FORCEMESSAGE / EV_NOTE production UI semantics
or another bounded event family encountered while progressing through Entrance
```

If later profiling shows a new UI bottleneck, incremental glyph painting is a possible bounded optimization, but the current dialog experience is hardware-accepted.

## Merge boundary

```text
base main = 7ff701245b0fda41de3cda7bd2fb65cad15eb218
hardware-tested implementation SHA = 777482b038088b232dcbfe64b2421d12aad3de15
REAL-CYD HARDWARE PASS
dialog grouped font reads = YES
dialog FNVs preserved = YES
dialog script state cycle preserved = YES
subjective dialog fluidity = strong PASS
fixed memory cost = 1024 B BSS
immutable runtime = YES
MERGE-READY = YES
```

All commits after `777482b038088b232dcbfe64b2421d12aad3de15` are documentation-only closeout. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
