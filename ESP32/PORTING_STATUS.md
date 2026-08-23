# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port.

## Latest merged hardware baseline

```text
PR   = #86 — first native PLAYING service
main = bf1275037fd22504077f6ff2bbf57e14721edf0a
hardware-tested firmware = e9c10c8759588e48478d3d702292628411c5939e
status = REAL-CYD HARDWARE PASS
```

Merged evidence: [`MAP1_NATIVE_PLAYING_SERVICE.md`](MAP1_NATIVE_PLAYING_SERVICE.md).

The port has a hardware-proven native ST_PLAYING owner and first native PLAYING service iteration while the legacy canvas remains parked at ST_INTRO.

## Current merge-ready milestone

```text
branch = agent/esp32-native-graphics-catalog
base   = bf1275037fd22504077f6ff2bbf57e14721edf0a
hardware-tested firmware = 0b40b47eb242ee5ff40b5c4981fe6a8892e95fc5
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

Evidence: [`MAP1_NATIVE_GRAPHICS_CATALOG.md`](MAP1_NATIVE_GRAPHICS_CATALOG.md).

This milestone replaces the need to resurrect global legacy mapping arrays with a compact immutable sparse graphics catalog built directly from the native PAK for resources actually required by Junction.

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
actual heap=10540 B
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

## Hardware-proven transition/player/post-load/PLAYING chain

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
```

Stable canonical fingerprints:

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

The idle-time owner FNV is intentionally not cross-boot canonical because it contains live uptime. The stable idle contract remains `idleTimeAfter-timeBefore=8000`.

Generic `EspMapOpcodeExecutor` remains intentionally only 11/19/20.

## Hardware-proven native ST_PLAYING

```text
EspPostLoadPlayingTransitionState = 12 B
stateFNV=73bc9acd
state=9->3
monstersTurn=0
displaySoftKeys=0
restoreSoftKeys=0->0
skipCheckState=0->1
softKeyIntent=1
softKeyPresentationDeferred=0
targetMap=9
active=1
nativeST_PLAYING=yes
legacyST_PLAYING=no
persistentHeapBytes=0
```

Legacy `DoomCanvas.state` remains parked at `ST_INTRO` so the legacy loop cannot enter `DoomCanvas_playingState()` / `Render_render()`.

## Hardware-proven post-load idle time

Stable contract:

```text
EspPostLoadIdleTimeState = 16 B
idleTimeAfter = timeBefore + 8000
targetMap=9
active=1
persistentHeapBytes=0
```

The owner contains live uptime, so its state FNV is a same-run witness rather than a stable cross-boot canon.

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

Semantic proof:

```text
firstNativePlayingService=yes
nativeST_PLAYING=yes
inputQueueEmpty=yes
inputConsumed=no
gameplayDispatch=no
renderRequested=yes
renderDeferred=yes
hudRequested=yes
presentation=no
legacyDoomCanvas_runCalled=no
legacyDoomCanvas_playingStateCalled=no
legacyDoomCanvas_updateViewCalled=no
legacyDoomCanvas_drawRGBCalled=no
```

## Hardware-proven sparse graphics catalog

Permanent files:

```text
ESP32/include/esp_native_graphics_catalog.h
ESP32/src/esp_native_graphics_catalog.c
```

Record and stable fingerprints:

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

Semantic proof:

```text
sparseCoverage=yes
coverageExact=yes
paletteRgb565Native=yes
legacyPaletteRelation=legacy-rb-swapped
texelPayloadResident=no
mapWideTexels=no
nativeCatalogPersistent=yes
```

The permanent catalog reads only selected mappings/palettes from `mappings.bin` + `palettes.bin` in `/DoomRPG-ESP32.pak`. It does not keep texels resident and does not use the runtime ZIP.

The legacy palette is only a temporary hardware witness and remained unchanged:

```text
legacyPaletteFNV=1e9365e2->1e9365e2
unchanged=yes
```

Legacy graphics mapping/runtime invariants remained:

```text
mediaTexelOffsets=NULL
mediaBitShapeOffsets=NULL
mediaTexturesIds=NULL
mediaSpriteIds=NULL
shapeData=NULL
mediaTexels=NULL
```

Fail-closed proof:

```text
preFindEmpty=1
repeat=1
repeatAtomic=yes
missingTexture=yes
missingSprite=yes
```

Resident / RAM proof:

```text
snapshotFNV=bb714d80->bb714d80
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureStateFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
payload=10410
packClosed=yes
heap8=72476->70620
persistentDelta=1856
largest8=34804->34804
logicalCatalogBytes=1840
allocatorOverhead=16
```

Same-build equality witnesses only:

```text
gameFNV=0b2bb17f->0b2bb17f
playerFNV=96f121f4->96f121f4
hudFNV=b18611d2->b18611d2
canvasFNV=564ff705->564ff705
renderFNV=29f02a1d->29f02a1d
frameFNV=6a0726c1->6a0726c1
GameMutation=no
PlayerMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
FrameMutation=no
```

## Fresh-Junction load tail: complete

Every successful caller statement through `return true` is hardware-proven semantically. There is no remaining post-load statement to recover before native PLAYING/render work.

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
graphicsCatalogPending=no
firstFramePending=yes
gameplayDispatchPending=yes
rendererPending=yes
initialSavePersistencePending=yes
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Still intentionally outside

```text
native durable save storage
cross-map durable SAVEGAME route payload
native queued-event payload ownership for non-empty contexts
native particle payload/runtime ownership for non-empty contexts
first native gameplay framebuffer render/presentation
native input dispatch
turn advancement
full native entity/monster gameplay
sound playback
```

## Probe completion semantics

Historical temporary probes may set `done=1` on terminal failure. `*_isDone()` alone is not a PASS certificate. Downstream probes must revalidate exact predecessor owners/world state. The graphics-catalog probe sets `done=1` only after successful PARK.

## Merge recommendation

```text
MERGE agent/esp32-native-graphics-catalog
```

Hardware-tested firmware:

```text
0b40b47eb242ee5ff40b5c4981fe6a8892e95fc5
```

All commits after that tested SHA must remain documentation-only.

## Next bounded milestone after merge

Recover exact new `main`, then consume the stable native graphics catalog (`stateFNV=969d5a77`) to remove `Render.mediaTexelOffsets` / `Render.mediaBitShapeOffsets` / `Render.mediaPalettes` from the bounded native graphics resource manager path and produce the **first real Junction gameplay framebuffer mutation** using native PAK-backed wall/sprite assets.

Keep it bounded:

```text
input consumed = no
turn advanced = no
legacy Game.entities = 0
legacy Game.monsters = 0
legacy DoomCanvas.state = ST_INTRO
legacy DoomCanvas_playingState = not called
legacy Render_render = not called
shapeData = NULL
mediaTexels = NULL
runtime ZIP = forbidden
```

The next milestone may mutate and present the logical 160x120 RGB565 framebuffer, but must not bundle input, turn logic, entity/monster activation or a return to the legacy renderer architecture.
