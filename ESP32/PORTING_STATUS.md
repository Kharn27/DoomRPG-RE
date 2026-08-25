# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #88 — first native Junction gameplay frame + permanent CYD panel profile
main = d8da51e5a3b9700d1806110f56f553a422d7d182
status = REAL-CYD HARDWARE PASS / MERGED
```

Merged evidence: [`MAP1_NATIVE_FIRST_JUNCTION_FRAME.md`](MAP1_NATIVE_FIRST_JUNCTION_FRAME.md).

## Current merge-ready milestone

```text
branch = agent/esp32-native-junction-sprites
base   = d8da51e5a3b9700d1806110f56f553a422d7d182
hardware-tested firmware = 3fdb2905b1d49ef1112a9e9df7a5db7e278897bd
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_JUNCTION_SPRITES.md`](MAP1_NATIVE_JUNCTION_SPRITES.md).

This milestone extends the deterministic Junction walls+textured-planes frame with BSP-visible native billboard rasterization, intrinsic legacy render modes 0/7 and bounded PAK-backed bitshape/texel reads while the legacy gameplay/render loop remains parked.

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
native base billboard sprites = reached
native BSP-visible sprite admission = reached
native intrinsic sprite mode 7 = reached
```

## Hardware-proven map canons

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
actual heap=18008 B
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

Later diagnostic BSS changes shifted allocator bookkeeping by 8 B on hardware (`10548 B` observed) without changing Junction payload, snapshots or largest free block. Absolute heap cost is therefore an allocator-layout witness, not a semantic fingerprint; the historical 10540 B value remains recorded for recovery.

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

## Hardware-proven transition/player/post-load/PLAYING/render chain

```text
CHANGEMAP pending intent
 -> level-exit stats
 -> native player exit-state
 -> LEVEL stats-menu semantic intent
 -> immutable 13-map catalog
 -> Junction transition preflight
 -> resident lifecycle / committed swap
 -> fresh-map spawn projection
 -> active player/view owner
 -> post-spawn HUD dirty owner
 -> Player_setup session owner
 -> initial tile owner
 -> finishRotation orientation owner
 -> finishRotation second-tile owner
 -> durable facing owner
 -> post-load HUD-clear owner
 -> direct Junction GIVEMAP owner
 -> current-weapon self-select owner
 -> initial-save semantic intent owner
 -> post-load flag cleanup owner
 -> event/particle cleanup owner
 -> post-load view-invalidation owner
 -> native ST_PLAYING transition owner
 -> post-load idle-time owner
 -> first native PLAYING service owner
 -> native sparse graphics catalog
 -> first native Junction wall frame
 -> native textured floor/ceiling planes
 -> raw RGB565 CYD presentation
 -> native BSP-visible billboard sprite pass
```

Stable canonical fingerprints through the pre-render chain:

```text
levelExitStatsFNV                 = bd41bcfa
playerExitAppliedFNV              = 298eaaa4
statsMenuIntentFNV                = 96afe901
catalogFNV                        = ce322e3f
transitionPreflightFNV            = 108e5c7b
committed WAIT_STATS FNV          = 66fe636a
committed READY FNV               = 0ef58ea8
committed ROLLBACK FNV            = 2dec1442
committed COMMITTED FNV           = 2c595a62
Junction spawn FNV                = ba6af4a7
packed override FNV               = e0a5110b
Junction player/view FNV          = d1131d18
packed override view FNV          = 9ed47d08
post-HUD player/view FNV          = d17fa0d1
Junction HUD refresh FNV          = 6965ee06
Player_setup semantic FNV         = 3b27c6a1
post-setup player/view FNV        = c21fba3c
Junction initial-tile FNV         = f73e28b2
post-initial-tile player FNV      = 1bd0f09b
Junction orientation FNV          = acc754a6
Junction second-tile FNV          = 09e58e0d
Junction durable-facing FNV       = 95aa1108
post-facing player/view FNV       = afcdcf74
Junction post-load HUD clear      = b7383e18
Junction post-load GIVEMAP        = 448e587d
Junction weapon self-select       = 699f3cf3
Junction initial-save intent      = 0bf1a911
Junction post-load flag cleanup   = 46cb2547
Junction event/particle cleanup   = 8bc79e2b
Junction view invalidation        = 4561c3c1
Junction native ST_PLAYING        = 73bc9acd
Junction native PLAYING service   = 4c50b853
Junction graphics catalog         = 969d5a77
Junction graphics texture records = 2dd5dfcf
Junction graphics sprite records  = cfd036cf
```

The idle-time owner FNV is intentionally not cross-boot canonical because it contains live uptime. Its stable contract remains `idleTimeAfter-timeBefore=8000`.

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20.

## Hardware-proven first native PLAYING service

```text
EspNativePlayingServiceState = 12 B
stateFNV=4c50b853
persistentHeapBytes=0
nativeState=3
serviceOrdinal=1
inputCountBefore=0
inputConsumed=0
gameplayDispatched=0
renderIntent=1
renderDeferred=1
presentationDeferred=1
hudIntent=1
targetMapId=9
active=1
```

Legacy `DoomCanvas.state` remains parked at `ST_INTRO` so the legacy loop cannot enter `DoomCanvas_playingState()` / `Render_render()`.

## Hardware-proven sparse graphics catalog

Permanent files:

```text
ESP32/include/esp_native_graphics_catalog.h
ESP32/src/esp_native_graphics_catalog.c
```

Catalog canon:

```text
EspNativeGraphicsCatalogRecord=40 B
textureCount=30
spriteCount=16
totalRecords=46
storageBytes=1840
heapCost=1856
allocatorOverhead=16
stateFNV=969d5a77
textureFNV=2dd5dfcf
spriteFNV=cfd036cf
textureRange=0..151
spriteRange=134..162
```

The catalog reads selected mappings/palettes directly from `/DoomRPG-ESP32.pak`, keeps no texel payload resident, and does not resurrect the global legacy mapping arrays.

Legacy graphics invariants remain:

```text
mediaTexelOffsets=NULL
mediaBitShapeOffsets=NULL
mediaTexturesIds=NULL
mediaSpriteIds=NULL
shapeData=NULL
mediaTexels=NULL
```

## Hardware-proven first native Junction frame

Permanent renderer files:

```text
ESP32/include/esp_native_first_frame.h
ESP32/src/esp_native_first_frame.c
ESP32/include/esp_native_plane_renderer.h
ESP32/src/esp_native_plane_renderer.c
```

Published first-frame owner:

```text
EspNativeFirstFrameState = 48 B
```

Important fingerprint rule:

```text
frameBeforeFNV = NOT cross-boot canonical
whole stateFNV = NOT cross-boot canonical
frameAfterFNV  = 8910c2ed  [stable canon]
viewportFNV    = 032ffaed  [stable canon, 160x80 @ 0,20]
```

Wall/BSP proof:

```text
nodes=39
leaves=12
nodeCull=8
lineCandidates=62
backfaceCull=20
clipCull=8
occluderOnly=0
spriteSpanDeferred=0
wallRequests=34
wallDraws=34
spanCalls=166
wallPixels=4341
wallCache=17H/17M/14E
resolvedTextures=30
animationTime=0
```

Textured plane proof:

```text
rows=80
pixels=12800
uniqueLogicalTextures=6
cache=12795H/5M/0E
texelReads=10240 B
```

Integrity/RAM proof:

```text
frameAfterFNV=8910c2ed
residentSnapshot=bb714d80->bb714d80
catalog=969d5a77->969d5a77
heapDelta=0
largestDelta=0
legacyRenderStable=yes
packClosed=yes
```

Post-PARK diagnostic:

```text
BMP path=/junction-viewport.bmp
BMP size=38454
viewport=160x80@0,20
viewportFNV=032ffaed
```

## Hardware-proven native Junction billboard pass

Permanent renderer files:

```text
ESP32/include/esp_native_junction_sprite_renderer.h
ESP32/src/esp_native_junction_sprite_renderer.c
```

The pass begins from `frame=8910c2ed`, repeats the validated stateful BSP depth walk, and uses visited leaves as the permanent equivalent of legacy `Render_relinkSprite()` / `viewSprites` admission.

View-sprite canon:

```text
mapSprites=48
bspCandidates=21
bspRejected=27
hidden=0
candidate modes=0:14 / 7:7
hardware census candidateFNV=23ef1895
permanent orderFNV=f16737cb
```

The 27 BSP-rejected sprites produce no pixels in this pose. Filtering them reduced work but deliberately left the final framebuffer unchanged versus the earlier map-wide implementation.

Raster/storage canon:

```text
depth nodes=39 leaves=12 nodeCull=8 lines=62 backface=20 clip=8
modes=0:14 / 7:7
mode7Pixels=311
draws=21
nearCull=0
clipCull=0
spanRuns=219
pixels=1828
wallOccludedCols=62
frameLoads=21
uniqueLogical=9
frameBytes=12251
maxFrameBytes=1020
packReads=130
glowDeferred=7
```

Stable post-sprite framebuffer:

```text
frameAfterFNV=299506eb
viewportFNV=ae2246eb
BMP path=/junction-sprite-viewport.bmp
BMP size=38454
viewport=160x80@0,20
```

Memory/world proof:

```text
heapDelta=0
largestDelta=0
legacyRenderStable=yes
topology=d6e8df7d
catalog=969d5a77
packClosed=yes
shapeData=NULL
mediaTexels=NULL
legacy Game.entities=0
legacy Game.monsters=0
```

The renderer owns one temporary bounded workspace only for the call. It resolves logical BSP IDs through bounded native PAK ranges, reads compact bitshape mask/texels/palette payloads, restores borrowed legacy projection scratch exactly and closes the pack before returning.

The intrinsic legacy mode-7 family is active for logical IDs 136/137/144. Implicit glow companions spawned by base IDs 135/140/131 are **not** yet rendered; seven are pending in the current Junction view.

## Hardware-selected classic CYD presentation

The logical framebuffer is standard raw RGB565. Dedicated real-CYD primary testing proved that software red/blue swapping is wrong.

Permanent presentation policy:

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

The stock `ILI9341_2_DRIVER` power/VCOM/frame-rate settings are retained. The real CYD selected panel calibration profile **B / INV-GAMMA-WA**. `PlatformVideo_begin()` programs this table into both ILI9341 gamma registers `0xE0` and `0xE1`:

```text
00 15 17 07 11 06 2b 56 3c 05 10 0f 3f 3f 0f
```

This correction is panel-side only. `PlatformVideo_present()` still sends the raw framebuffer with exact x2 duplication and performs no per-pixel colour correction.

## Current hardware PARK

```text
legacyState=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeST_PLAYING=yes
nativeIdleTime=yes
postLoadTailComplete=yes
nativePlayingService=yes
nativeGraphicsCatalog=yes
nativeFirstFrame=yes
texturedPlanes=yes
nativeBaseBillboards=yes
bspVisibleOnly=yes
intrinsicMode7=yes
glowPending=yes
hudPending=yes
gameplayDispatchPending=yes
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Still intentionally outside

```text
implicit glow companion sprites (7 pending in current Junction view)
other unsupported sprite families/render modes
native HUD gameplay painting
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

Historical temporary probes may set `done=1` on terminal failure. `*_isDone()` alone is not a PASS certificate. Downstream probes must revalidate exact predecessor owners/world state. Hardware serial evidence remains the final truth.

## Merge recommendation

```text
MERGE agent/esp32-native-junction-sprites
```

Hardware-tested firmware:

```text
3fdb2905b1d49ef1112a9e9df7a5db7e278897bd
```

All commits after that tested SHA must remain documentation-only before merge-ready declaration.

## Next bounded milestone after merge

After merge, recover the exact new `main` SHA and branch from it. Close the native sprite dependency graph for legacy implicit glow spawning, then render only that bounded family:

```text
135/140 -> companion logical 136, mode 7
131 -> companion logical 144, mode 7 when encountered
current Junction view = 7 pending companions
```

The sparse catalog must explicitly own these implicit dependencies; do not bypass ownership with ad-hoc PAK reads. Keep the existing BSP-visible set, input/turn/gameplay PARK and all no-legacy-graphics-pool invariants unchanged.
