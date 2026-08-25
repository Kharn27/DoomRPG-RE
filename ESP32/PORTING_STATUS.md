# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #92 — native gameplay touch intent
main = cdda239f1c884a7d6f6707ba1c30a0a0a3603923
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_INPUT.md`](MAP1_NATIVE_GAMEPLAY_INPUT.md).

PR #92 was merged from the input branch even though that archive still recorded the historical final-input-SHA Serial archive as pending. The current TURN milestone re-exercises the evolved input path on real hardware, including dynamic frame restore and 120 ms neon feedback.

## Current candidate milestone

```text
branch = agent/esp32-native-turn-dispatch
base   = cdda239f1c884a7d6f6707ba1c30a0a0a3603923
hardware-tested implementation SHA = 66ba643e7650f51d0022cd56e007242902d76c77
status = REAL-CYD HARDWARE PASS
merge-ready = yes after docs-only closeout audit
```

Evidence: [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md).

The real CYD executed four consecutive `TURN_RIGHT` actions, traversed all cardinal orientations, and returned exactly to the canonical North viewport, HUD bands and complete framebuffer.

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

## Native transition / player / post-load chain

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
 -> first native gameplay TURN dispatcher
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

Dependency-closed current Junction catalog:

```text
135/140 -> 136 mode7
131 -> 144 remains generic future dependency
closed stateFNV=257444a5
textures=30
sprites=17
storage=1880 B
persistent increment=40 B
largest8=34804 stable
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

Current feedback contract exercised during TURN hardware validation:

```text
hold=120 ms
maxEdits=512
feedbackBytes=2068 in predecessor 250 ms build; bounded static storage remains ~2 KiB
style=neon double ring + vector glyph
palette=BLUE/GREEN/YELLOW/RED
current-frame FNV captured per tap
reverse exact restore before dispatch
runtime allocation=0 for feedback
PAK/SD reads=0 for feedback
```

Every TURN tap in the final hardware run restored its current dynamic frame exactly before dispatch.

## Hardware-proven first real gameplay action family

Permanent dispatcher:

```text
EspNativeGameplayTurnState = 24 B
EspNativeGameplayDispatchResult = 12 B
TURN_LEFT  => +64
TURN_RIGHT => -64
only cardinal angles 0/64/128/192
all other recognized actions = DEFERRED
```

The input callback never renders. TURN waits for exact neon restore, queues an intent, returns the lifecycle, then commits and renders on a later service.

Final real-CYD runtime witnesses at tested SHA `66ba643e7650f51d0022cd56e007242902d76c77`:

```text
EspPlayerViewState=44 B
EspNativeGameplayFrameStats=84 B
probe execScratch=464 B
heap8=67284->67284
largest8=34804->34804
stackHighWater=172
intermediate world present suppressed=yes
final complete-frame present=1
Game_advanceTurn=no
Game_executeTile=no
legacyStable=yes
residentStable=yes
facingRefresh=deferred
```

Four consecutive `TURN_RIGHT` actions proved all cardinal views and exact 360-degree recovery.

Turn frames:

```text
N / angle64: frame ba3e5182 viewport 9206eb24 HUD 6c2aa46f
E / angle0 : frame 8cfdfe34 viewport 17c48c15 HUD 1d908304
S / angle192: frame da1c4297 viewport 582c2ad8 HUD a78d0f96
W / angle128: frame 23ee0954 viewport de06a408 HUD 9281a6d1
N / angle64 round-trip: frame ba3e5182 viewport 9206eb24 HUD 6c2aa46f exact
```

Final North render details re-match the established renderer canons:

```text
world viewport=032ffaed
sprites+glows viewport=9206eb24
walls=34 / 4341 pixels
planes=12800
sprites=21 / 1828 pixels
glows=7 / 1917 pixels
spriteReads=172
```

The runtime sprite compositor is view-agnostic: a cardinal view may legitimately draw zero sprites/glows when all BSP candidates are accounted for. Unsupported sprites and deferred glow dependencies still fail closed.

Current TURN frame bridge still uses one bounded temporary HUD save:

```text
temporaryHudBytes=12800
```

This is not persistent allocation and showed no heap/largest drift. Future cleanup should move the world renderer to a viewport-only redraw and remove this bridge buffer; do not mix that cleanup into already-proven TURN semantics.

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

Do not prematurely optimize `PlatformVideo_present()`.

## Current hardware PARK

Candidate hardware PARK after the TURN PASS:

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
TURN_LEFT=yes
TURN_RIGHT=yes
360-degree roundTrip=exact
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
Game_advanceTurn=no
Game_executeTile=no
```

## Still intentionally outside

```text
FORWARD/BACK/STRAFE execution
movement collision
SELECT interaction
weapon switching execution
PASS_TURN execution
MENU/AUTOMAP execution from gameplay
actual turn advancement
post-move/turn tile event dispatch
facing refresh after gameplay actions
full native entity/monster gameplay
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
66ba643e7650f51d0022cd56e007242902d76c77
```

No code may change after this tested SHA without another hardware run. Documentation commits after it must be `.md` only.

## Next bounded milestone after merge

After the user merges, recover the exact new `main` SHA and reread this file, `DOCUMENTATION.md` and [`MAP1_NATIVE_GAMEPLAY_TURN.md`](MAP1_NATIVE_GAMEPLAY_TURN.md) before branching.

Preferred next gameplay frontier: native cardinal translation (`FORWARD/BACK/STRAFE`) with collision semantics, while keeping actual turn advancement, tile-event execution and entity/monster activation fail-closed for later milestones.

A smaller renderer-only alternative is the first-person weapon overlay (fresh Junction pistol logical sprite 242). Do not mix both frontiers in one milestone.