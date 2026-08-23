# ESP32 native Junction sparse graphics catalog milestone

Branch: `agent/esp32-native-graphics-catalog`

Base merged `main`:

```text
PR   = #86 — first native PLAYING service
main = bf1275037fd22504077f6ff2bbf57e14721edf0a
```

Hardware-tested firmware:

```text
0b40b47eb242ee5ff40b5c4981fe6a8892e95fc5
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Remove the last requirement to resurrect the large legacy mapping tables before the first native Junction gameplay frame.

The existing bounded native wall/sprite resource loaders still historically resolve source offsets and palettes through `Render.mediaTexelOffsets`, `Render.mediaBitShapeOffsets` and `Render.mediaPalettes`. After the native map handoff those mapping arrays are deliberately freed and remain `NULL`.

This milestone introduces a compact permanent sparse catalog built directly from `mappings.bin` and `palettes.bin` inside `/DoomRPG-ESP32.pak`. It contains only resource IDs required by the resident Junction runtime. No texel payload is made resident and no runtime ZIP dependency is introduced.

## Permanent owner

Files:

```text
ESP32/include/esp_native_graphics_catalog.h
ESP32/src/esp_native_graphics_catalog.c
```

Record ABI:

```text
EspNativeGraphicsCatalogRecord = 40 B
resourceId
paletteSourceOffset
sourceOffset
paletteRgb565[16]
```

Hardware-proven catalog:

```text
textureCount=30
spriteCount=16
totalRecords=46
logicalCatalogBytes=1840
allocatorHeapCost=1856
allocatorOverhead=16
stateFNV=969d5a77
textureFNV=2dd5dfcf
spriteFNV=cfd036cf
textureRange=0..151
spriteRange=134..162
```

The catalog uses one compact persistent allocation. It stores mappings and sixteen RGB565 palette colors per selected resource, but **no texel payload**.

## Native palette source proof

`palettes.bin` is consumed directly from the native PAK. Hardware established:

```text
paletteRgb565Native=yes
legacyPaletteRelation=legacy-rb-swapped
legacyPaletteFNV=1e9365e2->1e9365e2
unchanged=yes
```

This matches the recovered startup behavior: the historical legacy palette loader swaps red/blue for its backend, while the native RGB565 source words in `palettes.bin` are already the permanent representation needed by the CYD framebuffer. The catalog therefore does not mutate or depend permanently on the legacy palette table.

## Sparse coverage proof

The resident runtime resource bitsets are the authority for required catalog entries.

Real CYD proved:

```text
sparseCoverage=yes
coverageExact=yes
missingTexture=yes
missingSprite=yes
texelPayloadResident=no
mapWideTexels=no
nativeCatalogPersistent=yes
```

Every Junction-required texture/sprite has exactly one matching catalog record and non-required lookup IDs remain absent.

## Strict predecessor and fail-closed proof

The catalog is routed only after the hardware-proven first native PLAYING service:

```text
playingServiceBytes=12
playingServiceFNV=4c50b853
unchanged=yes
residentSnapshot=bb714d80
```

Fail-closed/repeat behavior:

```text
preFindEmpty=1
repeat=1
repeatAtomic=yes
missingTexture=yes
missingSprite=yes
```

The PAK must be closed before catalog construction begins and is closed again before publication/PARK.

## Resident integrity

```text
snapshotFNV=bb714d80->bb714d80
unchanged=yes
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureStateFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

## RAM proof

Normal `esp32-cyd` environment:

```text
heap8=72476->70620
persistentDelta=1856
largest8=34804->34804
logicalCatalogBytes=1840
allocatorOverhead=16
```

Stable heartbeat after PARK:

```text
heap=136384
heap8=70620
largest8=34804
SD=ready
ZIP=ready
VIDEO=ready
CORE=ready
LAYOUT=ready
PRERENDER=ready
RENDER=ready
MAPPINGS=ready
MENUBSP=ready
touchIRQ=idle
```

## Legacy/framebuffer equality witnesses

Same-build witnesses only:

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

The unchanged framebuffer is intentional: this is a graphics-resource ownership milestone, not yet the first frame milestone.

Legacy mapping/runtime invariants remained exactly:

```text
mediaTexelOffsets=NULL
mediaBitShapeOffsets=NULL
mediaTexturesIds=NULL
mediaSpriteIds=NULL
shapeData=NULL
mediaTexels=NULL
```

## Hardware-proven PARK

```text
legacyState=9
page=3
targetMap=9
junctionResident=yes
nativeST_PLAYING=yes
nativePlayingService=yes
nativeGraphicsCatalog=yes
graphicsCatalogPending=no
firstFramePending=yes
gameplayDispatchPending=yes
rendererPending=yes
initialSavePersistencePending=yes
entities=0
monsters=0
noGameplay=yes
```

## Architectural consequence

The first native Junction frame no longer needs to recreate the global legacy mapping arrays. The permanent renderer path can resolve the resident map's selected resource IDs through a bounded immutable catalog backed by `/DoomRPG-ESP32.pak`.

The next bounded milestone should refactor the existing bounded native graphics resource manager to consume `EspNativeGraphicsCatalog` instead of `Render.mediaTexelOffsets` / `Render.mediaBitShapeOffsets` / `Render.mediaPalettes`, then produce the first real Junction framebuffer mutation using the existing native wall/sprite rasterization pieces.

Keep the next boundary strict:

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
