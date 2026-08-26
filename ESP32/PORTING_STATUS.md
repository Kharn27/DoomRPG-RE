# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #93 — native gameplay TURN dispatcher
main = 89f9d5f3feaa40f2e2a0c6e9506d1d8efaf5eeb6
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md).

## Current candidate milestone

```text
branch = agent/esp32-native-move-collision
base   = 89f9d5f3feaa40f2e2a0c6e9506d1d8efaf5eeb6
hardware-tested implementation SHA = becaa1ec5bdd68311fa2e1d626fc238d1a706779
status = REAL-CYD HARDWARE PASS
merge-ready = yes after docs-only closeout audit
```

Evidence: [`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md).

The real CYD executed native cardinal movement, TURN at moved tile centers, wall/entity collision, strafe derived from live orientation, continued movement after collision, and the renderer path that previously failed from a moved position. No `FAILED` marker occurred in the final run.

## Permanent invariants

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
native sparse graphics catalog = reached
native first Junction frame = reached
native textured planes = reached
native BSP-visible billboards = reached
native intrinsic sprite mode 7 = reached
native glow companions = reached
native gameplay HUD = reached
native gameplay touch intent owner = reached
native TURN_LEFT/TURN_RIGHT execution = reached
native FORWARD/BACK/STRAFE execution = reached
native static wall collision = reached
native compact linked-entity collision = reached
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

Absolute allocator cost is a witness, not a semantic fingerprint.

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

Final movement hardware SHA also fixes one permanent renderer bug: cached wall textures now raster in local texel space. `sourceTexelOffset` remains only for PAK range lookup/cache identity, avoiding a legacy map-wide 32-bit fixed-point overflow path while preserving `mediaTexels == NULL`.

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
PAK reads=184 bytes=6344 rows=164 pixels=7538
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

Feedback contract exercised in the final movement run:

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

Every valid tap in the final hardware run restored its current dynamic frame exactly before semantic execution/defer.

## Hardware-proven native TURN family

Permanent dispatcher:

```text
EspNativeGameplayTurnState = 24 B
EspNativeGameplayDispatchResult = 12 B
TURN_LEFT  => +64
TURN_RIGHT => -64
cardinal angles = 0/64/128/192
```

TURN is live at any settled native tile-center position. It does not call `Game_advanceTurn`, `Game_executeTile`, legacy `finishRotation`, entity/monster gameplay or facing refresh.

Canonical spawn 360-degree proof from the TURN milestone remains:

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
EspNativeGameplayFrameStats      = 84 B
movement probe execScratch       = 520 B
```

One successful action changes exactly one tile-center axis by 64 units. Forward derives from current gameplay orientation; BACK negates it; strafes rotate the vector cardinally.

Fresh Junction neighbor proof:

```text
FORWARD      943->911 delta=0,-64 flags=08 CLEAR
BACK         943->975 delta=0,+64 flags=1c CLEAR
STRAFE_LEFT  943->942 delta=-64,0 flags=01 WALL
STRAFE_RIGHT 943->944 delta=+64,0 flags=01 WALL
openLines=0
```

Final real-CYD run at tested SHA `becaa1ec5bdd68311fa2e1d626fc238d1a706779`:

```text
heap8=66708->66708
largest8=29684->29684
stackHighWater=172
FORWARD 943->911 pos 992,1888->992,1824 frame ba3e5182->66da9d16
TURN_LEFT at moved pos: angle64->128 frame 66da9d16->ec232716
blocked FORWARD 911->910: ENTITY blocker=24 type=7 frame exact
STRAFE_RIGHT at angle128: 911->879 pos 992,1824->992,1760
TURN_RIGHT at moved pos: angle128->64 frame 50c26281->fc7a5142
FORWARD 879->847 pos 992,1760->992,1696
TURN_RIGHT at moved pos: angle64->0 frame 3625f7a7->b6a50fb1
FORWARD 847->848 pos 992,1696->1056,1696
SELECT taps remained feedback-only/deferred
legacyStable=yes
residentStable=yes
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

Opened native lines remain deliberately fail-closed for movement collision until dynamic line/entity relinking has its own milestone.

## Performance status

Functionality is hardware-proven, but the user reports MOVE/TURN as **very slow**. This is expected from the current diagnostic bridge and is now an explicit performance debt, not a semantic blocker.

Current successful action hot path still includes:

```text
feedback full present
 -> exact feedback-restore full present
 -> transient wall/plane/sprite cache rebuild + PAK reads
 -> complete world/sprite/glow recomposition
 -> temporary 12.8 KiB HUD band save/restore
 -> final full present
```

Representative physical full presents are ~34 ms each; feedback hold is 120 ms. The renderer also repeats bounded PAK reads and transient cache construction per action.

Do **not** prematurely optimize `PlatformVideo_present()`. Preferred performance work is architectural and bounded:

- persistent bounded wall/plane caches;
- viewport-only native world redraw;
- remove the temporary 12.8 KiB HUD bridge save;
- retain on-demand turn-based redraw and exact frame canons;
- keep all migrated assets PAK-backed and map-wide texel pools forbidden.

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
becaa1ec5bdd68311fa2e1d626fc238d1a706779
```

No code may change after this tested SHA without another hardware run. Documentation commits after it must be `.md` only.

## Next bounded milestone after merge

After the user merges, recover the exact new `main` SHA and reread this file, `DOCUMENTATION.md` and [`MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_MOVE_COLLISION.md) before branching.

Preferred next frontier: a bounded **native gameplay renderer hot-path** milestone. Preserve all current gameplay semantics and exact frame outputs while replacing repeated transient render-cache/HUD-save work with permanent bounded caches and viewport-only redraw. Do not optimize `PlatformVideo_present()` itself as the first move.

If behavior is prioritized instead, the next coherent semantic family is post-move `Game_eventFlagsForMovement` / tile-event / turn-advance behavior, recovered from legacy and introduced fail-closed family by family. Do not combine that with broad entity/monster gameplay.