# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + `DOCUMENTATION.md` + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #94 — native cardinal movement + collision
main = b5a4426eb0df1ef1506893d4bc08b5538543a7b3
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md).

## Current candidate milestone

```text
branch = agent/esp32-native-gameplay-render-hotpath
base   = b5a4426eb0df1ef1506893d4bc08b5538543a7b3
hardware-tested implementation SHA = a07455e34eadbacca7d23fb068ba4308f0b7f80a
status = REAL-CYD HARDWARE PASS
merge-ready = yes after docs-only closeout audit
```

Evidence: [`MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md`](MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md).

The real CYD proved a bit-exact gameplay-only world redraw that touches only the `160x80 @0,20` viewport, preserves both HUD bands in place, uses `0 B` temporary HUD save, performs no intermediate world present, and still produces exactly one final physical present.

The user reports **no notable fluidity improvement versus `main`**. Timing witnesses explain why: the eliminated intermediate present was only ~34 ms while canonical North recomposition remains ~3.27 s, dominated by world/sprite/HUD PAK-backed work.

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
heapDelta=0 largestDelta=0
legacyRenderStable=yes packClosed=yes
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

The movement milestone permanently fixed wall cache raster addressing: `sourceTexelOffset` is PAK lookup/cache identity only; raster sampling stays in local cached texel space. `mediaTexels` remains NULL.

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
initial PAK reads=184 bytes=6344 rows=164 pixels=7538
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
style=neon double ring + vector glyph
palette=BLUE/GREEN/YELLOW/RED
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

TURN is live at arbitrary settled native tile-center positions. It does not call `Game_advanceTurn`, `Game_executeTile`, legacy `finishRotation`, entity/monster gameplay or facing refresh.

Canonical spawn cardinal proof:

```text
N / angle64: frame ba3e5182 viewport 9206eb24 HUD 6c2aa46f
E / angle0 : frame 8cfdfe34 viewport 17c48c15 HUD 1d908304
S / angle192: frame da1c4297 viewport 582c2ad8 HUD a78d0f96
W / angle128: frame 23ee0954 viewport de06a408 HUD 9281a6d1
N / angle64 round-trip: frame ba3e5182 viewport 9206eb24 HUD 6c2aa46f exact
```

## Hardware-proven native cardinal movement + collision

Permanent compact results from the merged movement milestone:

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

Merged hardware proof also includes a real entity blocker:

```text
FORWARD 911->910
collision=ENTITY
blocker=24
type=7
frame exact / no movement
```

Opened native lines remain deliberately fail-closed until dynamic line/entity relinking has its own milestone.

## Hardware-proven gameplay renderer hot path

Permanent boundary:

```text
EspNativeGameplayFrameStats = 104 B
world viewport = 160x80 @0,20
temporary HUD save = 0 B
world route physical present = none
final physical present = exactly one
HUD bands preserved in place during world/sprite redraw
```

Final real-CYD implementation SHA:

```text
a07455e34eadbacca7d23fb068ba4308f0b7f80a
```

Strict canonical North proof:

```text
frame=ba3e5182
viewport=9206eb24
HUD=6c2aa46f
frameStatsBytes=104
tempHud=0
routeNoPresent=1
finalPresent=1
heap8=66452->66452
largest8=29684->29684
exact=yes
```

The final fix replaced the old wrapper activation dependency on `!EspNativeFirstFrame_isReady()` with the real compact-native render context. This is required so plane injection and `tmpLine` preservation work for both boot and gameplay viewport routes without resetting the historical first-frame owner.

Representative successful-action scratch witnesses at this SHA:

```text
TURN probe execScratch = 484 B
MOVE probe execScratch = 540 B
stackHighWater = 172
```

### Measured performance truth

Canonical North hot-path:

```text
world   = 1261184 us
sprites = 1572941 us
HUD     =  387161 us
present =   34930 us
total   = 3265801 us
```

Approximate total share:

```text
world   ~38.6%
sprites ~48.2%
HUD     ~11.9%
present  ~1.1%
```

The user reports no notable fluidity improvement versus `main`, which is consistent with the measurements. Removing an ~34 ms redundant present cannot materially change a ~3.27 s heavy frame.

Other real-CYD samples:

```text
TURN N->E total=1835575 us
TURN E->N total=3265560 us / canonical round-trip exact
MOVE 943->911 total=3038743 us
MOVE 911->879 total=2952445 us
MOVE 879->847 total=2916327 us
```

Across those actions:

```text
tempHud=0
routeNoPresent=1
finalPresent=1
heap8=66452->66452
largest8=29684->29684
legacyStable=yes
residentStable=yes
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

## Current performance diagnosis

The remaining latency is now measured renderer/storage debt, not presentation debt.

Current world phase still:

```text
open PAK
 -> validate/scan complete disk index
 -> disk-backed entry searches
 -> rebuild 30 resolved wall descriptors
 -> transient 3-slot wall cache
 -> plane reads
 -> close PAK
```

Current sprite phase then independently:

```text
build BSP visibility
 -> open/validate PAK again
 -> resolve mappings/palettes/bitshape/texel sources again
 -> load one frame at a time
 -> canonical North: 21 base + 7 glow loads / 172 PAK reads
 -> only 9 unique base logical sprite IDs
 -> close PAK
```

Current compass phase then independently:

```text
open/validate PAK again
 -> reopen k.bmp/o.bmp/a.bmp
 -> 63 PAK reads
 -> ~387 ms
 -> close PAK
```

Do **not** prioritize `PlatformVideo_present()` optimization: hardware proves it is only ~1% of canonical heavy-frame time.

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
persistent bounded render-resource/frame caches
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
a07455e34eadbacca7d23fb068ba4308f0b7f80a
```

No code may change after this tested SHA without another hardware run. Documentation commits after it must be `.md` only.

## Next bounded milestone after merge

After the user merges, recover the exact new `main` SHA and reread this file, `DOCUMENTATION.md` and [`MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md`](MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md) before branching.

Preferred next frontier: **bounded persistent native render-resource/cache ownership**. Keep `/DoomRPG-ESP32.pak` as backing store while avoiding repeated full PAK validation/index scans, repeated source-metadata reconstruction and repeated identical sprite/HUD frame reads.

Suggested order:

```text
1. resident validated render-source / PAK-entry metadata
2. small bounded sprite-frame cache keyed by logical/actual frame
3. bounded resident compass/HUD render resources
4. evaluate bounded persistent wall/plane texel cache with explicit RAM budget
```

All output must remain bit-exact. Keep `shapeData == NULL`, `mediaTexels == NULL`, no runtime ZIP, no map-wide texel pool, bounded RAM, explicit eviction, and fail-closed behavior.

If behavior is prioritized instead, the next coherent semantic family remains post-move `Game_eventFlagsForMovement` / tile-event / turn-advance behavior. Do not combine that semantic work with the performance-cache milestone.
