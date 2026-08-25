# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #89 — native Junction BSP-visible billboard sprites
main = 674b45bbd115cd8f9202f2ce2d7132550c3bb75e
status = REAL-CYD HARDWARE PASS / MERGED
```

Merged evidence: [`MAP1_NATIVE_JUNCTION_SPRITES.md`](MAP1_NATIVE_JUNCTION_SPRITES.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-junction-glows
base   = 674b45bbd115cd8f9202f2ce2d7132550c3bb75e
hardware-tested firmware = 338388ee4166115585e2c964aa95e79d5b0313eb
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_JUNCTION_GLOWS.md`](MAP1_NATIVE_JUNCTION_GLOWS.md).

This milestone preserves the hardware-proven Junction wall/plane/base-billboard frame, closes the sparse sprite dependency `135/140 -> 136`, and renders all seven current glow companions in exact legacy parent order.

## Permanent invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
runtime ZIP = forbidden
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

Later diagnostic layout changes can shift allocator bookkeeping without changing semantic payload/snapshots. Absolute heap cost is a witness, not a semantic fingerprint.

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

## Native transition / player / post-load chain

Hardware-proven sequence through the current renderer boundary:

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

## Hardware-proven direct sparse graphics catalog

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

The catalog reads only selected mappings/palettes from `/DoomRPG-ESP32.pak`, retains no map-wide texel payload, and does not resurrect legacy graphics arrays.

## Hardware-proven glow dependency closure

The direct catalog remains the predecessor canon. The current milestone atomically adds the implicit current-view dependency:

```text
135/140 -> logical 136 / mode 7
```

Real-CYD closure canon:

```text
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

Generic dependency semantics also retain `131 -> 144` for future maps/views that require it; current Junction requires only 136.

## Hardware-proven native Junction walls + planes frame

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

`frameBeforeFNV` and the whole first-frame state FNV are not cross-boot canons.

## Hardware-proven BSP-visible base billboard pass

The shared stateful BSP visibility/depth walk publishes visited leaves and 160 column depths, then only sprites relinked to those visited leaves enter sorting/rasterization.

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
```

Pre-glow predecessor framebuffer:

```text
frameAfterFNV=299506eb
viewportFNV=ae2246eb
```

Filtering the 27 non-visible-leaf sprites reduced work but did not change this framebuffer because those sprites already contributed no final pixels.

## Hardware-proven native glow companions

Legacy draws the glow immediately after each parent. Current Junction contains seven visible parents requiring companion sprite 136.

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
```

Stable complete sprite+glow framebuffer:

```text
frameAfterFNV=b5218f24
viewportFNV=9206eb24
BMP path=/junction-sprite-viewport.bmp
BMP size=38454
viewport=160x80 @ 0,20
```

Real-CYD visual inspection clearly showed the additive lamp glow effect.

Memory/world proof:

```text
catalog persistent increment=40 B
renderer heapDelta=0
renderer largestDelta=0
topology=d6e8df7d
closedCatalog=257444a5
packClosed=yes
shapeData=NULL
mediaTexels=NULL
legacy Game.entities=0
legacy Game.monsters=0
no world/entity mutation
no input consumed
no turn advancement
no gameplay dispatch
```

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

Presentation remains raw framebuffer x2 duplication with no per-pixel software colour transform.

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
glowPending=no
hudPending=yes
gameplayDispatchPending=yes
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Still intentionally outside

```text
native gameplay HUD painting
other sprite families/render modes not required by current Junction pose
native input dispatch
turn advancement
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
MERGE agent/esp32-native-junction-glows
```

Hardware-tested firmware:

```text
338388ee4166115585e2c964aa95e79d5b0313eb
```

All commits after that tested SHA must remain documentation-only before merge-ready declaration.

## Next bounded milestone after merge

After merge, recover the exact new `main` SHA and branch from it. The next coherent visible boundary is **native gameplay HUD painting** from the already-owned native player/view/HUD intents.

Keep it bounded:

```text
same 160x120 framebuffer
same Junction pose/world PARK
no input consumption
no turn advancement
no gameplay dispatch
no legacy DoomCanvas_playingState / Render_render
shapeData=NULL
mediaTexels=NULL
runtime ZIP forbidden
bounded native PAK reads/caches only
```

Do not fold input/gameplay dispatch into the HUD milestone.