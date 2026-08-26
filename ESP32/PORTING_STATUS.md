# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + `DOCUMENTATION.md` + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #95 — native gameplay renderer viewport hot path
main = f98a0b8e9eb4cbd38bf5678a1ce60c4989766985
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md`](MAP1_NATIVE_GAMEPLAY_RENDER_HOTPATH.md).

## Current candidate milestone

```text
branch = agent/esp32-native-gameplay-render-resource-cache
base   = f98a0b8e9eb4cbd38bf5678a1ce60c4989766985
hardware-tested implementation SHA = 1e8c6a5f8fd1e6d01588b1c74dd4fc4e3b961e95
status = REAL-CYD HARDWARE PASS
merge-ready = yes after docs-only closeout audit
```

Evidence: [`MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md).

The real CYD proves that one bounded persistent `/DoomRPG-ESP32.pak` owner plus exact small-range reuse removes the dominant repeated storage latency while keeping the canonical framebuffer bit-exact. Warm canonical North recomposition drops to `264828 us`, and the user reports the game is now clearly more playable.

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

Real linked-entity blocker witness:

```text
FORWARD 911->910
collision=ENTITY
blocker=24
type=7
frame exact / no movement
```

Opened native lines remain deliberately fail-closed until dynamic line/entity relinking has its own milestone.

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

Hardware-tested implementation SHA for that merged milestone:

```text
a07455e34eadbacca7d23fb068ba4308f0b7f80a
```

Canonical framebuffer contract remains:

```text
frame=ba3e5182
viewport=9206eb24
HUD=6c2aa46f
tempHud=0
routeNoPresent=1
finalPresent=1
exact=yes
```

## Hardware-proven persistent gameplay render resource cache

Permanent owner contract:

```text
physical default PAK owner = persistent during native gameplay
logical world/sprite/HUD leases = open/close compatible
full PAK validation = once at resident begin
entry descriptor cache slots = 24
exact range payload = 16384 B
exact range key capacity = 256
cacheable range max = 1024 B
large world ranges = PAK-backed bypass
runtime ZIP = forbidden
shapeData = NULL
mediaTexels = NULL
```

Final real-CYD implementation SHA:

```text
1e8c6a5f8fd1e6d01588b1c74dd4fc4e3b961e95
```

Owner hardware witness:

```text
owner struct=21160 B
heap8=66372->40832
observed heap cost=25540 B
largest8=13812 B
cache=9225/16384 B after canonical cold frame
range entries=195/256
physicalResident=yes
logicalPackClosed=yes
```

Canonical North COLD:

```text
physicalOpen=0 validate=0
sdReads=280 sdBytes=55541
entry=6H/9M
range=155H/195M/195S/22B
world=1044890 us
sprites=505972 us
HUD=378835 us
present=34925 us
total=1974252 us
frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f exact=yes
```

Canonical North WARM:

```text
physicalOpen=0 validate=0
sdReads=22 sdBytes=45056
entry=15H/0M
range=350H/0M/0S/22B
world=209454 us
sprites=9604 us
HUD=1317 us
present=34908 us
total=264828 us
frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f
heapStable=yes shapeData=NULL mediaTexels=NULL
```

Storage reduction:

```text
280 -> 22 physical reads
258 reads saved
~92.1% fewer physical reads
```

The remaining warm physical traffic is exactly:

```text
22 reads
45056 B
22 x 2048 B
```

That is the deliberate >1024 B bypass class and is now the dominant measured storage debt.

Interactive hardware samples with the owner kept resident:

```text
FORWARD 943->911 total=255050 us
FORWARD 911->879 total=272550 us
TURN_RIGHT 64->0 @ tile879 total=160497 us
TURN_LEFT 0->64 @ tile879 total=272608 us
TURN_LEFT 64->128 @ tile879 total=260054 us
TURN_RIGHT 128->64 @ tile879 total=272618 us
FORWARD 879->847 total=235073 us
FORWARD 847->815 total=290621 us
TURN_RIGHT 64->0 @ tile815 total=203124 us
```

Across that sequence:

```text
heap8=40832->40832
largest8=13812->13812
tempHud=0
routeNoPresent=1
finalPresent=1
legacyStable=yes
residentStable=yes
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

One first West-direction HUD repaint measured `110734 us`; already-warm compass paths measured about `1.3 ms`. Keep this as a hardware witness rather than treating every HUD direction as fully warmed from boot.

## Current performance truth

Before the resident owner, the immediate predecessor North frame in the same firmware measured:

```text
world=1295232 us
sprites=1610031 us
HUD=400296 us
present=34935 us
total=3350141 us
```

Warm resident North:

```text
world=209454 us
sprites=9604 us
HUD=1317 us
present=34908 us
total=264828 us
```

This is approximately `12.65x` lower total recomposition time, or about `92.1%` lower latency for this strict repeated pose. The user reports that the result is now clearly more playable.

Warm North phase share is now roughly:

```text
world   ~79.1%
sprites  ~3.6%
HUD      ~0.5%
present ~13.2%
```

The next renderer debt is therefore no longer broad repeated metadata/sprite/HUD work. It is the remaining 2048-byte world texture range class plus the fixed physical present cost.

Do not grow permanent RAM casually: the no-PSRAM hardware now has a proven post-owner witness of only `40832 B` heap8 and `13812 B` largest block.

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
2048-byte persistent wall/plane texture range caching
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
1e8c6a5f8fd1e6d01588b1c74dd4fc4e3b961e95
```

No code may change after this tested SHA without another hardware run. Documentation commits after it must be `.md` only.

## Next bounded milestone after merge

After the user merges, recover the exact new `main` SHA and reread this file, `DOCUMENTATION.md` and [`MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_RENDER_RESOURCE_CACHE.md) before branching.

Preferred renderer frontier: **remaining 2048-byte world texture reads, without increasing permanent RAM unless hardware evidence justifies it**.

Current warm witness is exactly `22 x 2048 B` physical reads. First investigate whether the already allocated 16 KiB resident payload can be partitioned/reused for a tiny bounded wall/plane cache while retaining the small-range wins. Preserve bit-exact output, explicit ownership, fail-closed behavior and the post-owner RAM floor.

Do not combine this storage milestone with new gameplay semantics.

If behavior is prioritized instead, the next coherent semantic family remains post-move `Game_eventFlagsForMovement` / tile-event / turn-advance behavior.