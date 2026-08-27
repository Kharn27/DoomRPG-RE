# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md) + `DOCUMENTATION.md` override chat memory.

## Latest merged baseline

```text
PR   = #100 — native gameplay SELECT front-tile
main = be4a9a666245663da7866a8aa0aa40b98339d076
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md`](MAP1_NATIVE_GAMEPLAY_SELECT_FRONT_TILE.md).

## Current candidate — generic Entrance gameplay engine

```text
branch = agent/esp32-native-entrance-startup-route
base   = be4a9a666245663da7866a8aa0aa40b98339d076
hardware-tested implementation SHA = 300561cfc9b4d06af769fda54613d837fa738f58
status = REAL-CYD HARDWARE PASS
merge-ready = YES
post-test commits = documentation-only
```

The original startup correction, Entrance spawn, first frame, HUD, renderer, caches and native gameplay have now been carried through to one map-generic production lifecycle. The compact permanent contract is documented in [`NATIVE_ENGINE_RECOVERY.md`](NATIVE_ENGINE_RECOVERY.md).

## Hardware-proven production route

Real classic CYD now runs:

```text
cinematic intro
 -> startupMap=1
 -> /intro.bsp / Entrance resident
 -> BSP header spawn tile 904 / direction 64
 -> settled EspPlayerView
 -> generic EspNativeGameplaySession
 -> graphics catalog
 -> first world frame
 -> native HUD
 -> sprite dependency closure
 -> resident small cache cold/warm
 -> exact 2048-B large-range learn/warm
 -> collision catalog
 -> touch + TURN/MOVE gameplay
```

No automatic `ResidentHandoff` or `CommittedTransition` to Junction occurs at boot.

Historical per-milestone probes remain regression witnesses only. Production gameplay must not require them.

## Critical engine rule

**A new BSP is not a new engine.**

Future maps must follow the same resident-map pipeline:

```text
PAK BSP
 -> compact EspMapRuntime
 -> compact mutable overlays
 -> player/view
 -> EspNativeGameplaySession
 -> renderer/HUD/input/collision/events
```

Do not create permanent `ENTRANCE_*`, `JUNCTION_*` or `LEVEL_X_*` gameplay/render/cache pipelines. If a later BSP exposes unsupported data or behavior, implement that family generically, fail closed elsewhere, hardware-test it, then reuse it across all maps.

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

## Initial player/view hardware canon

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

The two startup tile-event lookups found event 90 but it was ineligible, so startup required no additional opcode family.

## Generic renderer/HUD hardware proof

Entrance initial graphics catalog:

```text
textures = 33
sprites = 45
storage = 3120 B
FNV = 29ffc14a
```

After renderer dependency closure:

```text
sprites = 46
```

Initial frame:

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

Sprites render correctly across multiple Entrance views. Hardware examples include 13 draws / 3789 pixels, 8 / 3049, 6 / 6776 and valid zero-visible-sprite frames.

## Render-cache hardware canon

The permanent storage lifecycle is now explicit and map-generic:

```text
small owner begin
 -> SMALL-COLD
 -> SMALL-WARM
 -> enable exact 2048-B ranges with zero owner growth
 -> LARGE-LEARN
 -> LARGE-WARM
 -> arm gameplay
```

Entrance real-CYD evidence:

```text
resident owner = 21160 B
payload = 16384 B
range records = 256
heap8 after owner = 31956
largest8 after owner = 8692

SMALL-COLD  total = 2119886 us
SMALL-WARM  total = 256807 us
LARGE-LEARN total = 247770 us
LARGE-WARM  total = 229719 us

payload after prime = 14645 / 16384 B
large entries = 2
```

The plane renderer no longer relies on one contiguous 12288-B frame allocation; its six 2048-B cache slots are bounded small leases, so Entrance does not fail merely because the post-cache heap is fragmented.

## Native gameplay hardware proof

The generic resident gameplay service is active on Entrance:

```text
touch = invisible 12-zone + 120 ms transient feedback
TURN_LEFT / TURN_RIGHT = active
FORWARD / BACK / STRAFE = active
collision = native topology + entity defs + dynamic line state
```

Hardware movement/turn examples:

```text
904 -> 872 -> 840
angle 64 -> 0 -> 192 -> 128 -> 64
```

Movement back to tile 904 and repeated redraws remained stable.

Closed/locked Entrance line collision is proven:

```text
source = 904
dest = 936
line = 258
texture = 7
flags = 0x00000505
type = 0
defTile = 312
result = BLOCKED
```

Stable post-prime heartbeat from the supplied run:

```text
heap = 97664
heap8 = 31956
largest8 = 8692
shapeData = NULL
mediaTexels = NULL
```

No Guru Meditation or reboot was reported during the walk/turn interaction sequence.

## SELECT / Action boundary

The Action/SELECT touch zone is classified and the map-generic front-tile resolver is active, but semantic execution is deliberately deferred.

Entrance hardware evidence:

```text
front tile 841 -> event 88
eligible = EV_DIALOG + EV_NEXTSTATE

front tile 936 -> event 91
line 258 locked
eligible = EV_OPENLINE(258)
```

The resolver preserves frame, heap, resident owners and line state exactly; it does not execute bytecode or mutate doors. The gameplay service currently reports `SELECT ... semantic-not-enabled`.

**Recommended next milestone:** enable the Action/SELECT button through bounded native execution of the already-recovered safe families needed by Entrance. Do not broad-enable legacy `Game_executeEvent`.

`PASS_TURN`, menu, automap, weapon actions and ordinary MOVE tile-event execution also remain deferred until dedicated gameplay milestones own their semantics.

## Event/script boundary

Generic `EspMapOpcodeExecutor` remains intentionally limited to:

```text
11 EV_CHANGESTATE
19 EV_NEXTSTATE
20 EV_PREVSTATE
```

All unsupported opcode families remain fail-closed until their dedicated permanent owner/API is enabled.

Historical `MAP1_*` semantic probes have already recovered UI/string/status/dialog/notebook, key/password, line door/unlock, automap grant, save/change-map, show/hide and exit-state behavior. These are evidence and reusable native components, not boot prerequisites.

## Historical Junction canon remains valid

The startup correction and Entrance generic-engine proof do not invalidate prior Junction hardware work.

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

Junction is now a second hardware corpus for the same engine, not the engine identity.

## Permanent memory / architecture invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
runtime ZIP = forbidden for migrated map/graphics paths
backing     = /DoomRPG-ESP32.pak
legacy Game.entities = 0
legacy Game.monsters = 0
```

Prefer compact immutable arenas, explicit small mutable owners, bounded caches and small buffers. Do not reintroduce pointer-heavy desktop ownership or map-wide decoded graphics.

## Merge recommendation

```text
REAL-CYD HARDWARE PASS
hardware-tested implementation SHA = 300561cfc9b4d06af769fda54613d837fa738f58
Entrance visible = YES
Entrance walkable/turnable = YES
sprites = YES
HUD = YES
touch = YES
TURN/MOVE = YES
collision = YES
render cache prime = YES
shapeData/mediaTexels = NULL
MERGE-READY = YES
```

Every commit after `300561cfc9b4d06af769fda54613d837fa738f58` is documentation-only closeout. After merge, recover the exact new `main` SHA, reread this file + `NATIVE_ENGINE_RECOVERY.md` + `DOCUMENTATION.md`, then branch for the bounded native Action/SELECT execution milestone.
