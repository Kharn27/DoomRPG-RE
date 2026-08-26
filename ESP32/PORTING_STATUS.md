# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + `DOCUMENTATION.md` + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #96 — persistent gameplay render resource cache
main = 377fce3de5381373750a7fba29d0c83b8142c583
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md).

## Current candidate milestone

```text
branch = agent/esp32-native-gameplay-large-range-cache
base   = 377fce3de5381373750a7fba29d0c83b8142c583
hardware-tested implementation SHA = 1273f0205c0ba060972500bedd76effc974077bf
status = REAL-CYD HARDWARE PASS
merge-ready = yes after docs-only closeout audit
```

Evidence: [`MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md).

The real CYD proves two permanent renderer/storage boundaries in this milestone:

1. exact `2048 B` immutable texture ranges can borrow the unused tail of the existing `16 KiB` resident payload with **zero new heap owner**;
2. the legacy renderer's one-byte cross-block wall sampling at packed index `2048` can be reproduced with a **16 B BSS guard** after the raster/BSP stack has fully unwound, without restoring map-wide `mediaTexels`.

The user reports being able to roam throughout the level on the final tested firmware.

## Permanent hardware / memory invariants

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
native ST_PLAYING = reached
legacy ST_PLAYING = not reached
native PLAYING service = reached
broad legacy PLAYING loop = forbidden
```

## Hardware-proven map / resident canons

Entrance:

```text
resource=/intro.bsp
bytes=21823
crc32=623f34e4
sourceFNV=d5cc751f
gameplayLoadMapId=1
spawnIndex=904
spawnDirection=64
snapshotFNV=b3811f3d
logical payload=17891 B
historical heap=18008 B
```

Junction:

```text
resourceMapId=9 / /junction.bsp
gameplayLoadMapId=2
sourceBytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
spawnIndex=943
spawnDirection=64
payload=10410 B
historical heap cost=10540 B
entities metadata=30
enemies=0
destructibles=3
```

Current Junction resident owner FNVs:

```text
runtime  = bc432a0f
map      = 8dba0bb4
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = b699bd75
topology = d6e8df7d
snapshot = bb714d80
```

Absolute allocator values are witnesses, not semantic fingerprints.

## Native transition / player / gameplay chain

Hardware-proven sequence:

```text
CHANGEMAP intent
 -> level-exit stats
 -> native player exit-state
 -> LEVEL stats-menu semantic intent
 -> immutable 13-map catalog
 -> Junction preflight
 -> resident committed swap
 -> spawn projection
 -> player/view owner
 -> HUD dirty owner
 -> Player_setup owner
 -> initial tile
 -> finishRotation orientation + second tile
 -> durable facing
 -> post-load HUD clear / GIVEMAP / weapon self-select
 -> initial-save intent
 -> post-load flag + event/particle cleanup
 -> view invalidation
 -> native ST_PLAYING transition
 -> idle-time owner
 -> first native PLAYING service
 -> direct sparse graphics catalog
 -> walls + textured planes
 -> BSP-visible billboards
 -> implicit glow closure + glow companions
 -> native gameplay HUD
 -> native calibrated touch intent
 -> native TURN_LEFT/TURN_RIGHT
 -> native FORWARD/BACK/STRAFE + collision
 -> viewport-only native gameplay recomposition
 -> persistent bounded gameplay render resource owner
 -> shared-payload exact 2048 B reuse
 -> bounded legacy wall-block guard recovery
```

Stable pre-render/player fingerprints:

```text
levelExitStatsFNV               = bd41bcfa
playerExitAppliedFNV            = 298eaaa4
statsMenuIntentFNV              = 96afe901
mapCatalogFNV                   = ce322e3f
transitionPreflightFNV          = 108e5c7b
committed WAIT_STATS FNV        = 66fe636a
committed READY FNV             = 0ef58ea8
committed ROLLBACK FNV          = 2dec1442
committed COMMITTED FNV         = 2c595a62
Junction spawn FNV              = ba6af4a7
Junction durable-facing FNV     = 95aa1108
post-facing player/view FNV     = afcdcf74
Junction native ST_PLAYING      = 73bc9acd
Junction native PLAYING service = 4c50b853
```

Idle-time state contains uptime; stable contract remains `idleTimeAfter-timeBefore=8000`.

Generic `EspMapOpcodeExecutor` remains intentionally limited to opcodes 11/19/20 and fail-closes all others.

## Graphics / renderer recovery canons

Direct sparse catalog:

```text
record=40 B
textures=30
sprites=16
storage=1840 B
historical heap=1856 B
stateFNV=969d5a77
textureFNV=2dd5dfcf
spriteFNV=cfd036cf
```

Dependency-closed Junction catalog:

```text
135/140 -> 136 mode7
131 -> 144 remains generic future dependency
closed stateFNV=257444a5
textures=30
sprites=17
storage=1880 B
persistent increment=40 B
```

Canonical North walls + textured planes:

```text
frame=8910c2ed
viewport=032ffaed
viewport=160x80 @0,20
nodes=39 leaves=12 nodeCull=8
lines=62 backface=20 clip=8 occluder=0 spriteSpan=0
wallRequests/draws=34/34
wallSpans=166 wallPixels=4341
planes=12800 textures=6 cache=12795H/5M/0E reads=10240 B
legacyRenderStable=yes
```

Canonical North base billboards:

```text
mapSprites=48 candidates=21 rejected=27 hidden=0
candidateFNV=23ef1895
orderFNV=f16737cb
modes=0:14 / 7:7
mode7Pixels=311
draws=21 spans=219 pixels=1828 wallOccludedCols=62
frameLoads=21 uniqueLogical=9 frameBytes=12251 maxFrame=1020
preGlowFrame=299506eb
preGlowViewport=ae2246eb
```

Canonical North glow companions:

```text
companions=7 draws=7 spans=59 pixels=1917 wallOccludedCols=32
frameLoads=7 frameBytes=5572 maxFrame=796 packReads=172
completeFrame=b5218f24
completeViewport=9206eb24
```

The movement milestone permanently fixed wall cache raster addressing: `sourceTexelOffset` is PAK lookup/cache identity only; normal raster sampling stays in local cached texel space. `mediaTexels` remains NULL.

## Native gameplay HUD

Fresh Junction HUD:

```text
top y=0..19
world y=20..99
bottom y=100..119
health=30/30 armor=0/20 weapon=2 pistol ammoType=1 ammo=8 face=0 direction=N
EspNativeGameplayHudState=22 B
stateFNV=4756db9c
assets=a.bmp,k.bmp,l.bmp,m.bmp,o.bmp
```

Canonical framebuffer:

```text
pre-HUD frame=b5218f24
complete gameplay frame=ba3e5182
world viewport=9206eb24
HUD bands=6c2aa46f
```

HUD dirty owner:

```text
before FNV=6965ee06 refreshPending=1
after  FNV=40c66f99 refreshPending=0
consume only after successful paint=yes
```

## Native gameplay touch boundary

Permanent semantic owner:

```text
EspNativeGameplayInputState = 12 B
EspNativeGameplayTouchHit   = 6 B
one pending intent maximum
busy/unsupported = fail closed
```

Current touch layout:

```text
top HUD y=0..19:
  x0..31    MENU
  x32..127  PASS_TURN
  x128..159 AUTOMAP
world y=20..45: STRAFE_LEFT | FORWARD | STRAFE_RIGHT
world y=46..72: TURN_LEFT   | SELECT  | TURN_RIGHT
world y=73..99: PREV_WEAPON | BACK    | NEXT_WEAPON
bottom HUD y=100..119: unbound
```

Feedback contract:

```text
hold=120 ms
maxEdits=512
bounded static feedback storage ~2 KiB
current-frame FNV captured per tap
reverse exact restore before dispatch
runtime allocation=0 for feedback
PAK/SD reads=0 for feedback
```

## Hardware-proven native TURN family

```text
EspNativeGameplayTurnState = 24 B
EspNativeGameplayDispatchResult = 12 B
TURN_LEFT  => +64
TURN_RIGHT => -64
cardinal angles = 0/64/128/192
```

Cardinal semantic orientation:

```text
0   = East
64  = North
128 = West
192 = South
```

TURN is live at arbitrary settled native tile-center positions. It does not call `Game_advanceTurn`, `Game_executeTile`, legacy `finishRotation`, entity/monster gameplay or facing refresh.

Canonical spawn proof:

```text
N / angle64: frame ba3e5182 viewport 9206eb24 HUD 6c2aa46f
E / angle0 : frame 8cfdfe34 viewport 17c48c15 HUD 1d908304
S / angle192: frame da1c4297 viewport 582c2ad8 HUD a78d0f96
W / angle128: frame 23ee0954 viewport de06a408 HUD 9281a6d1
N / angle64 round-trip: frame ba3e5182 viewport 9206eb24 HUD 6c2aa46f exact
```

## Hardware-proven native cardinal movement + collision

Permanent compact results:

```text
EspNativeGameplayCollisionResult = 16 B
EspNativeGameplayMoveResult      = 24 B
EspPlayerViewState               = 44 B
```

Fresh Junction neighbors:

```text
FORWARD      943->911 delta=0,-64 flags=08 CLEAR
BACK         943->975 delta=0,+64 flags=1c CLEAR
STRAFE_LEFT  943->942 delta=-64,0 flags=01 WALL
STRAFE_RIGHT 943->944 delta=+64,0 flags=01 WALL
openLines=0
```

Real linked-entity blocker witnesses include:

```text
FORWARD 911->910 collision=ENTITY blocker=24 type=7
FORWARD 752->784 collision=ENTITY blocker=32 type=7
```

Blocked actions keep the framebuffer and position exact. Opened native lines remain deliberately fail-closed until dynamic line/entity relinking has its own milestone.

## Hardware-proven gameplay renderer hot path

Merged permanent boundary:

```text
EspNativeGameplayFrameStats = 104 B
world viewport = 160x80 @0,20
temporary HUD save = 0 B
world route physical present = none
final physical present = exactly one
HUD bands preserved in place during world/sprite redraw
```

Canonical framebuffer contract:

```text
frame=ba3e5182
viewport=9206eb24
HUD=6c2aa46f
tempHud=0
routeNoPresent=1
finalPresent=1
exact=yes
```

## Merged persistent gameplay render resource cache

PR #96 permanent owner:

```text
physical default PAK owner = persistent during native gameplay
logical world/sprite/HUD leases = open/close compatible
full PAK validation = once at resident begin
entry descriptor cache slots = 24
exact range payload = 16384 B
exact range key capacity = 256
small cacheable range max = 1024 B
owner struct = 21160 B
runtime ZIP = forbidden
shapeData = NULL
mediaTexels = NULL
```

Merged implementation hardware witness:

```text
cache=9225/16384 B after canonical cold frame
range entries=195/256
physicalResident=yes
logicalPackClosed=yes
```

Canonical warm North before the new large-range class:

```text
sdReads=22
sdBytes=45056
22 x 2048 B
world~204-209 ms in recent runs
sprites~9.4 ms
HUD~1.3 ms warm
present~34.9 ms
frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f
```

## Current shared-payload exact 2048 B cache

No second permanent heap owner is allocated.

Layout:

```text
small <=1024 B ranges grow upward
large exact 2048 B ranges grow downward
small ranges retain priority
large entries use the same 256-record table
activation allocates nothing
```

Canonical real-CYD proof:

```text
BASE  sdReads=22 sdBytes=45056 cache=9225/16384 B entries=195/256 large=off
LEARN slots=3 sdReads=22 sdBytes=45056 range=350H/22M/3S/19B
      cache=15369/16384 B entries=198/256 large=3 exact=yes
WARM  slots=3 sdReads=19 sdBytes=38912 range=353H/19M/0S/19B
      cache=15369/16384 B entries=198/256 large=3 exact=yes
READY savedReads=3 savedBytes=6144 ownerDelta=0 heapStable=yes
      frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f
```

The measured strict-frame total was about `232.9 ms` once the three retained large ranges were warm. The user reports gameplay is playable, but the perceptual gain from this cache alone is modest.

## Native wall packed-index boundary and final guard

Unrestricted roaming exposed the first exact bounded-wall overread at:

```text
view=992,1568,36
angle=64
line=90
logical=66
actual=140
flags=00002000
source=98304
packedIndex=2048
```

This witness is **not** a double-height wall (`0x00010000` is absent). The previously attempted double-height continuation hypothesis is retired.

Recovered legacy behavior:

```text
required actual wall blocks were sorted by source offset
 -> packed contiguously into legacy mediaTexels
 -> local block index 2048 therefore addressed byte 0 of the next admitted block
```

Permanent native repair:

```text
LegacyWallGuard = 16 B BSS
no extra 2048 B buffer
no FirstFrameWork growth
no PAK read from sampleWallSpan()
exact index 2048 only
all unrelated OOB stays fail-closed
```

Recovery flow:

```text
SPAN_OOB 2048
 -> renderer unwinds
 -> resolve next admitted compact wall block outside raster stack
 -> read one packed byte
 -> publish 16 B guard
 -> retry viewport frame
```

Explicit final real-CYD witness:

```text
logical=15
actual=40
source=20480
successorActual=108
successorSource=61440
byte=aa
guard owner=BSS bytes=16
retry line=33
recovered=yes
```

The corresponding move then succeeded:

```text
pos 992,1696 -> 992,1760
tile 847 -> 879
frame ece49fb2 -> ea36efee
legacyStable=yes
residentStable=yes
orientationStable=yes
```

The user then roamed throughout the level on the same firmware. The Serial excerpt is partial because the run produced too much output; do not invent missing per-position fingerprints.

## Final RAM / stack truth for current candidate

Stable long-run witness from hardware-tested SHA `1273f020...`:

```text
heap=105424
heap8=39756
largest8=14836
stackHighWater=924
MOVE execScratch=540 B
TURN execScratch=484 B
```

The immediately preceding stable pre-guard heap was `39772 B`. The exact `16 B` reduction matches `sizeof(LegacyWallGuard)`.

Across the long roaming run:

```text
heap stable
largest stable
legacyStable=yes
residentStable=yes
shapeData=NULL
mediaTexels=NULL
no Guru Meditation
no reboot
```

## Known visual anomaly deliberately NOT closed by this milestone

The user still observes a confusing renderer/view symptom around the start/arrival door:

- when backing away while oriented away from the door, the view can appear as if a half-turn occurred;
- a similarly confusing view can appear when facing/approaching the door.

Serial semantic orientation remains coherent and MOVE preserves it. TURN changes it only by the proven cardinal `+/-64` steps. Therefore do **not** alter `EspPlayerViewState` or gameplay orientation merely to hide the visual symptom.

Treat this as the preferred next renderer investigation: camera transform / BSP visibility / wall orientation / geometry relationship around that door needs a small dedicated hardware witness.

## Hardware-selected classic CYD presentation

```text
framebuffer       = raw RGB565
logical size      = 160x120
physical size     = 320x240
resampling        = exact nearest-neighbour x2
software sat/gamma transform = none
software R/B swap = none
TFT byte swap     = ON
panel inversion   = ON
TFT_RGB_ORDER     = TFT_BGR
ILI9341 driver    = ILI9341_2_DRIVER
SPI frequency     = 55 MHz
gamma             = 00 15 17 07 11 06 2b 56 3c 05 10 0f 3f 3f 0f
```

## Current hardware PARK

```text
legacyState=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeST_PLAYING=yes
nativePlayingService=yes
nativeGraphicsCatalog=yes
nativeFirstFrame=yes
texturedPlanes=yes
nativeBaseBillboards=yes
bspVisibleOnly=yes
intrinsicMode7=yes
glowCompanions=yes
nativeHud=yes
nativeInput=yes
nativeTurnDispatch=yes
nativeMovementDispatch=yes
nativeGameplayViewportHotPath=yes
persistentRenderResourceOwner=yes
residentPakPhysical=yes
logicalRenderPakLeasesClose=yes
smallExactRangeCache=yes
largeExact2048RangeCache=yes
legacyWallGuard=yes
TURN_LEFT=yes
TURN_RIGHT=yes
FORWARD/BACK/STRAFE semantic execution=yes
static wall collision=yes
compact linked entity collision=yes
dynamic opened-line collision=fail-closed
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

## Still intentionally outside

```text
start-door visual renderer/view anomaly fix
post-recovery optimization of guard metadata reads
Game_eventFlagsForMovement
post-move tile event execution
actual turn advancement
dynamic line/entity collision relinking / opened-door collision
SELECT interaction
weapon switching execution
PASS_TURN execution
MENU/AUTOMAP execution from gameplay
entity/monster activation and AI
facing refresh after gameplay actions
first-person weapon overlay
native durable save storage
cross-map durable SAVEGAME route payload
native queued-event payload ownership for non-empty contexts
native particle runtime for non-empty contexts
sound playback
```

## Probe completion semantics

Historical temporary probes may set `done=1` on terminal failure. `*_isDone()` alone is not a PASS certificate. Downstream code must revalidate exact predecessor owners/world state. Serial logs from the real CYD remain the final truth.

## Merge recommendation

```text
MERGE-READY after docs-only audit
```

Hardware-tested implementation SHA:

```text
1273f0205c0ba060972500bedd76effc974077bf
```

No code may change after this tested SHA without another hardware run. Documentation commits after it must be `.md` only.

## Next bounded milestone after merge

After the user merges, recover the exact new `main` SHA and reread this file, `DOCUMENTATION.md` and [`MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md) before branching.

Preferred next milestone: **start-door renderer/view anomaly witness**.

Keep it narrow:

```text
no semantic orientation mutation until divergence is proven
capture player view angle/position
capture transformed renderer viewX/viewY/viewAngle
capture relevant BSP/line IDs and flags around the door
compare opposite-facing and BACK/FORWARD views at the same settled tile centers
keep success path silent
fail closed
```

Do not mix this renderer diagnosis with post-move tile events, turn advancement, doors/dynamic line relinking, or other gameplay semantics.
