# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #91 — native gameplay HUD
main = 7686f7fb5c93d375f51a34ec0dd0b5cb127017e3
status = REAL-CYD HARDWARE PASS / MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_HUD.md`](MAP1_NATIVE_GAMEPLAY_HUD.md).

## Current candidate milestone

```text
branch = agent/esp32-native-touch-input-intent
base   = 7686f7fb5c93d375f51a34ec0dd0b5cb127017e3
implementation HEAD = 8c620093650ac4efd5d470343466ce9f5c441e4e
status = REAL-CYD VISUAL PASS / FINAL SERIAL ARCHIVE PENDING
merge-ready = no
```

Evidence: [`MAP1_NATIVE_GAMEPLAY_INPUT.md`](MAP1_NATIVE_GAMEPLAY_INPUT.md).

The final CYD layout has been visually confirmed by the user as perfect. The semantic input owner and exact-restoring feedback path were already exercised on real hardware before the final layout/color polish, but the repository deliberately does not label the final HEAD hardware-tested until a Serial block from that exact firmware is archived.

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
native glow dependency closure = reached
native glow companions = reached
native gameplay HUD = reached
native gameplay touch intent owner = reached on candidate branch
real gameplay action dispatch = not reached
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
entities=30
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

Later diagnostic layout changes can shift allocator bookkeeping without changing semantic payload/snapshots. Absolute heap cost is a witness, not a semantic fingerprint.

## Native transition / player / post-load chain

Hardware-proven sequence through the merged HUD boundary:

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
 -> first native walls + textured planes frame
 -> raw CYD presentation
 -> BSP-visible billboard pass
 -> implicit glow dependency closure
 -> seven native glow companions
 -> native gameplay HUD paint
 -> consume native HUD dirty intent
```

Candidate branch adds, without action execution:

```text
 -> calibrated CYD touch
 -> logical 160x120 hit classification
 -> compact EspNativeGameplayInputState
 -> probe consumes semantic intent
 -> transient exact-restoring feedback
```

Stable pre-render fingerprints retained for recovery:

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

The idle-time owner FNV is intentionally not cross-boot canonical because it contains live uptime; stable contract is `idleTimeAfter-timeBefore=8000`.

Generic `EspMapOpcodeExecutor` remains intentionally limited to opcodes 11/19/20 and fail-closes all others.

## Hardware-proven graphics catalog / glow closure

Direct sparse catalog predecessor:

```text
EspNativeGraphicsCatalogRecord=40 B
textureCount=30
spriteCount=16
totalRecords=46
storageBytes=1840
historical heapCost=1856
stateFNV=969d5a77
textureFNV=2dd5dfcf
spriteFNV=cfd036cf
```

Current Junction dependency closure:

```text
135/140 -> logical 136 / mode 7
direct stateFNV=969d5a77
closed stateFNV=257444a5
textureCount=30
spriteCount=17
storageBytes=1880
persistent increment=40 B
dependency=136
directTextureFNV=2dd5dfcf
directSpriteFNV=cfd036cf
largest8=34804->34804
repeatAtomic=yes
packClosed=yes
```

Generic dependency semantics retain `131 -> 144` for future maps/views that require it; current Junction requires only 136.

## Hardware-proven native Junction world frame

Walls + textured planes predecessor:

```text
frameAfterFNV=8910c2ed
viewportFNV=032ffaed
viewport=160x80 @ 0,20
nodes=39
leaves=12
nodeCull=8
lines=62
backface=20
clip=8
occluder=0
spriteSpan=0
wallRequests=34
wallDraws=34
wallSpans=166
wallPixels=4341
planes=12800
planeTextures=6
planeCache=12795H/5M/0E
planeReads=10240 B
heapDelta=0
largestDelta=0
legacyRenderStable=yes
packClosed=yes
```

BSP-visible base billboards:

```text
mapSprites=48
bspCandidates=21
bspRejected=27
hidden=0
candidateFNV=23ef1895
orderFNV=f16737cb
modes=0:14 / 7:7
mode7Pixels=311
draws=21
nearCull=0
clipCull=0
spans=219
pixels=1828
wallOccludedCols=62
frameLoads=21
uniqueLogical=9
frameBytes=12251
maxFrameBytes=1020
preGlowFrameFNV=299506eb
preGlowViewportFNV=ae2246eb
```

Glow companions:

```text
companions=7
draws=7
nearCull=0
clipCull=0
spans=59
pixels=1917
wallOccludedCols=32
frameLoads=7
frameBytes=5572
maxFrameBytes=796
packReads=172
renderMode=7 additive RGB565
completeFrameFNV=b5218f24
completeViewportFNV=9206eb24
```

Real-CYD visual inspection clearly showed the additive lamp glow effect.

## Hardware-proven native gameplay HUD

Current-pose HUD contract:

```text
top HUD band = y 0..19
world viewport = y 20..99 / 160x80
bottom HUD band = y 100..119
health=30/30
armor=0/20
weapon=2 / pistol
ammoType=1
ammo=8
face=0 / normal
direction=N
```

Permanent native painter / indexed-BMP path:

```text
EspNativeGameplayHudState=22 B
stateFNV=4756db9c
assets=a.bmp,k.bmp,l.bmp,m.bmp,o.bmp
assetsValidated=5
bar=20x20
icon=13x13
face=18x20
PAK reads=184
PAK bytes=6344
source rows=164
painted pixels=7538
packClosed=yes
```

Stable framebuffer canons:

```text
frameBeforeFNV=b5218f24
frameAfterFNV=ba3e5182
worldViewportFNV=9206eb24
worldViewportPreserved=yes
HUD bands FNV=9cf0c5c5->6c2aa46f
```

HUD dirty owner:

```text
before: bytes=8 FNV=6965ee06 refreshPending=1
after :         FNV=40c66f99 refreshPending=0
consume only after successful paint=yes
```

Real-CYD RAM / side-effect proof:

```text
heap8=70196->70196
largest8=34804->34804
heapDelta=0
largestDelta=0
legacyHudStable=yes
playerStable=yes
gameStable=yes
canvasStable=yes
renderStable=yes
residentStable=yes
topology=d6e8df7d
closedCatalog=257444a5
shapeData=NULL
mediaTexels=NULL
legacy Game.entities=0
legacy Game.monsters=0
no input consumed
no turn advancement
no gameplay dispatch
```

The user explicitly reported the HUD as visually correct on the physical CYD.

## Candidate native gameplay touch boundary

Permanent semantic owner:

```text
EspNativeGameplayInputState = 12 B
EspNativeGameplayTouchHit   = 6 B
one pending intent maximum
unsupported/busy = fail closed
```

Final touch layout at implementation HEAD:

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

Final transient feedback contract:

```text
hold=250 ms
maxEdits=512
bounded static saved-pixel storage ~= 2 KiB + metadata
runtime allocation=0
style=neon double ring + vector glyph
palette top/move/turn/weapon = BLUE/GREEN/YELLOW/RED
restore=reverse-order exact pixel restore
no PAK/SD reads
```

The first real-CYD input run before final polish proved:

```text
baseline frame=ba3e5182
stateBytes=12
hitBytes=6
heap8=69828->69828
largest8=34804->34804
all exercised valid taps => semantic INTENT
all transient restores => frame ba3e5182 exact=yes
no gameplay dispatch
```

The user subsequently visually confirmed the final 12-zone neon layout as perfect. Exact final-HEAD runtime values are intentionally not promoted until its Serial block is archived.

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
```

Selected hardware gamma profile B / INV-GAMMA-WA, written to ILI9341 registers `0xE0` and `0xE1`:

```text
00 15 17 07 11 06 2b 56 3c 05 10 0f 3f 3f 0f
```

## Current hardware PARK

Merged/main hardware PARK:

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
glowPending=no
nativeHud=yes
hudPending=no
gameplayDispatchPending=yes
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

Candidate branch additionally arms native touch-intent classification after this PARK, but still executes no gameplay action.

## Still intentionally outside

```text
real native gameplay action dispatch
player/view mutation from touch
turn advancement
movement collision
full menu navigation from gameplay
real automap activation from gameplay
other sprite families/render modes not required by current Junction pose
full native entity/monster gameplay
native durable save storage
cross-map durable SAVEGAME route payload
native queued-event payload ownership for non-empty contexts
native particle payload/runtime ownership for non-empty contexts
sound playback
```

## Probe completion semantics

Historical temporary probes may set `done=1` on terminal failure. `*_isDone()` alone is not a PASS certificate. Downstream code must revalidate exact predecessor owners/world state. Serial logs from the real CYD remain the final truth.

## Merge recommendation

```text
DO NOT MERGE YET
```

Implementation HEAD awaiting final Serial archive:

```text
8c620093650ac4efd5d470343466ce9f5c441e4e
```

Once the final Serial block from that exact firmware proves the 12-zone contract, representative BLUE/GREEN/YELLOW/RED feedback, PREV/NEXT weapon, PASS_TURN, exact `ba3e5182` restore and unchanged heap/largest with no FAILED/ERROR, update the three documentation files only and declare merge-ready.

## Next bounded milestone after merge

After merge, recover the exact new `main` SHA and branch from it. The next coherent frontier is the first **real native gameplay action family**, preferably `TURN_LEFT` / `TURN_RIGHT` only.

All other recognized actions must remain deferred/fail-closed. Do not combine first dispatch with movement collision, full turn advancement, monster gameplay or general entity activation.