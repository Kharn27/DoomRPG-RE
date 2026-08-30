# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary

```text
main = bb95082917d2bb3504243015de358d40a8d11788
branch = agent/esp32-sprite-storage-profile
base main = bb95082917d2bb3504243015de358d40a8d11788
hardware-tested final code HEAD = abb66f4a44c196b01511df66d1230c72040546e2
status = REAL-CYD HARDWARE PASS
merge-ready = YES, after documentation-only tail verification
```

This branch is a bounded renderer/storage profiling milestone. It does not add a
new renderer architecture or a new permanent asset pool. It instruments the
resident sprite pass and fixes one renderer-stack failure exposed while testing.

## Permanent architecture

```text
A NEW BSP IS NOT A NEW ENGINE.
```

Production path:

```text
/DoomRPG-ESP32.pak
 -> native BSP reader
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> EspPlayerView
 -> native event/action/gameplay
 -> native renderer
```

Hard invariants:

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

Do not reintroduce a map-wide texel pool, a desktop Entity pointer graph, or ZIP
runtime ownership for migrated map data.

## Entrance canonical resident witness

```text
resourceMapId = 1
resource = /intro.bsp
name = Entrance
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
runtime arena = 14095 B
runtimeFNV = c3882516
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

These are regression witnesses only; production behavior must not branch on
Entrance-specific values.

## Generic session baseline

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

Resident asset-cache canon:

```text
owner = 21160 B
payload = 16384 B
entries = 256
large learned entries = 2 in the normal priming sequence
```

## Native Action / gameplay boundary inherited from main

Real-CYD hardware already validates:

```text
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
native collision/topology
SELECT event-first routing
EV_SHOW / EV_HIDE / EV_UNLOCK
EV_OPENLINE / EV_CLOSELINE
EV_DIALOG / EV_DIALOGNOBACK
EV_FORCEMESSAGE / EV_NOTE
state ops 11 / 19 / 20
regular door animation
mutable line texture variants in the native renderer
native idle weapon rendering
generic weapon attack frame 1 -> idle frame 0
weapon pickup eType=5
empty/human Action feedback with ~1200 ms lifetime
adjacent extinguisher fire removal with rollback-safe presentation
```

Still fail-closed/deferred:

```text
generic monster/destructible HP/damage combat
ammo/XP/sound/turn consequences
remote extinguisher miss/no-effect transaction
player-stat/inventory/ammo pickups
EV_CHANGEMAP / EV_GIVEMAP / EV_PASSWORD / EV_SAVEGAME / EV_CHECK_KEY
```

The desktop/J2ME remote-extinguisher contract remains: a farther visible fire
can enter attack presentation, misses outside the one-tile hit range, and reports
`No effect!`. Native `FIRE_RANGE_DEFERRED` is a temporary safe divergence until
generic combat/miss ownership exists.

## Sprite storage profile hardware pass

The new `[SPRITEPROFILE]` instrumentation measures the sprite phase without
changing renderer output or adding a second resident cache. It reports logical
frame loads plus deltas from the existing `EspAssetPack` resident cache.

### Cold session witness

On the first cold sprite pass:

```text
[SPRITEPROFILE]
us=641298
logicalReads=148
frameLoads=23
glowLoads=1
unique=12
frameBytes=18991
glowBytes=796
physicalReads=81
physicalBytes=12467
range=76H/69M/69S/3B
resident=1
cache=9175/16384
entries=176/256
```

This proves that the old ~545-640 ms sprite spikes can be caused by cold backing
storage/cache misses rather than by the sprite rasterizer alone.

### Warm steady-state witnesses

After the existing resident cache is primed, the same logical sprite workload is
much cheaper:

```text
logicalReads=148 frameLoads=23 unique=12
sprite phase ~= 22-23 ms
physicalReads=3 physicalBytes=4395
range ~= 145H/0M/0S/3B
```

During the reproduced gameplay route:

```text
MOVE 904->872: spriteUs=22560 physicalReads=3
TURN angle 64->0: spriteUs=19475 physicalReads=3
TURN angle 0->64: spriteUs=22508 physicalReads=3
MOVE 872->840: spriteUs=19474 physicalReads=3
```

At the previously crashing virage, after the wall guard recovery:

```text
[SPRITEPROFILE] us=10598 logicalReads=112 frameLoads=17 glowLoads=1 unique=10
physicalReads=0 physicalBytes=0 range=112H/0M/0S/0B
cache=11272/16384 entries=256/256
```

The immediately following move was even cheaper:

```text
[SPRITEPROFILE] us=9249 logicalReads=100 frameLoads=15 glowLoads=1 unique=10
physicalReads=0 physicalBytes=0 range=100H/0M/0S/0B
```

### Performance conclusion

The previous conclusion that the sprite renderer/storage path was the dominant
steady-state hotspot is superseded by this hardware evidence.

Current evidence says:

```text
cold sprite storage misses = can be very expensive (~641 ms observed)
warm sprite phase          = ~9-23 ms in reproduced route
raw VIDEO present          = ~34-35 ms here
normal full gameplay frame = ~208-267 ms here
recovery TURN frame        = 453208 us because the world pass is retried
next MOVE frame            = 283531 us
```

Therefore:

```text
steady-state primary candidate = world / plane / wall renderer
sprite steady-state            = secondary/small in this corpus
PlatformVideo_present          = not the first optimization target
```

Do not add a large sprite cache based only on `logicalReads`. Logical reads can
be fully served from the existing resident owner; the physical-read counters are
the relevant backing-store witness.

## Renderer crash recovery hardware pass

The original test exposed a real renderer failure at the turn from angle 64 to
128 near tile 840:

```text
[NATIVEFRAME] LEGACY_GUARD logical=15 actual=40 source=20480 successorActual=68 successorSource=32768 byte=11
[NATIVEFRAME] RETRY legacy compact guard after unwound SPAN_OOB line=277 actual=40
Guru Meditation Error: Stack canary watchpoint triggered (loopTask)
```

The recursive BSP walk could consume the remaining loopTask stack when the retry
progressed deeper than the first failed pass.

Final code HEAD `abb66f4a44c196b01511df66d1230c72040546e2` keeps the same
front-to-back DFS order but uses a compact bounded iterative traversal:

```text
node stack  = 65 x uint16_t = 130 B
depth stack = 65 x uint8_t  = 65 B
workspace   = 195 B temporary stack only
recursion   = none
new heap allocation = none
new permanent BSS owner = none
```

A first iterative version used a 520 B permanent BSS owner. On the real CYD that
changed boot heap topology by about 528 B and made the transitional ZIP inflater
fail on `menu.bsp`:

```text
old healthy MAPSTRUCT heap8 ~= 34996
bad BSS build MAPSTRUCT heap8 = 34468
[DOOM ERROR] out of memory allocating inflate state for menu.bsp
```

That version is superseded. The final transient 195 B traversal restored boot and
kept the non-recursive renderer.

Real-CYD final witness at the old crash location:

```text
[NATIVEFRAME] LEGACY_GUARD logical=15 actual=40 source=20480 successorActual=68 successorSource=32768 byte=11 owner=BSS bytes=16
[NATIVEFRAME] RETRY legacy compact guard after unwound SPAN_OOB line=277 actual=40
[NATIVEFRAME] BSP map=1 ... nodes=19 leaves=4 ...
[NATIVEFRAME] WALL requests=19 draws=19 spans=160 pixels=6328 ...
[NATIVEFRAME] RECOVERED legacy compact guard actual=40 successorActual=68 source=20480->32768
[RESIDENTGAMEPLAY] TURN n=3 seq=5 action=TURN_LEFT angle=64->128 committed=yes
```

Gameplay then continued normally:

```text
[RESIDENTGAMEPLAY] MOVE n=3 seq=6 action=FORWARD tile=840->839 ... committed=yes
```

No stack canary, no reboot, no renderer disable. This is a hardware PASS.

Permanent renderer rule:

```text
Do not use unbounded/recursive BSP traversal on classic CYD.
Keep traversal order explicit, bounded and fail-closed without solving stack
pressure by repeatedly increasing loopTask stack size.
```

## Dynamic line / wrapper rule

A linker wrapper must not call a higher-level API that can indirectly re-enter
that wrapped symbol. Use `__real_*`, an already materialized owner/view, or an
explicitly non-reentrant helper.

This remains the rule recovered from the earlier dynamic line texture recursion
failure.

## Pickup frontier

Current production pickup owner:

```text
eType=5 weapon
world remove = consumed-sprite bit overlay
ownership = native uint16 weapon mask
new weapon select = native HUD overlay
rollback = exact on redraw failure
```

Deferred:

```text
eType=3 player-stat / world item
eType=4 inventory
eType=6 ammo
eType=16 alternate ammo
weapon ammo/message/sound acquisition consequences
```

Armor helmets are expected not to be consumed at this boundary; do not patch
individual pickup subtypes.

## Build/include guardrails

Never shadow ESP-IDF/Arduino framework headers under `ESP32/include` to solve a
local compatibility problem. Prefer existing project clocks/APIs or a narrowly
project-named adapter. Do not inject legacy `DoomRPG.h` through a framework
include chain.

## Next bounded direction

After this branch merges, recover the new exact GitHub `main` SHA first.

The strongest next performance milestone is now:

```text
world / plane / wall renderer profile and optimization
```

Bounded goal:

```text
measure the steady-state world phase separately from sprites/present
identify physical PAK traffic versus CPU raster/projection cost
preserve exact framebuffer output
preserve shapeData == NULL and mediaTexels == NULL
avoid new map-wide or large permanent caches
keep the legacy-wall guard recovery fail-closed
```

The specific recovery turn is also useful as a secondary corpus because it
currently pays for a failed world pass plus a retry (~453 ms total frame).

Separate correctness families remain player-stat pickups, ammo/inventory, and
generic combat/miss/ammo/sound/turn ownership.
