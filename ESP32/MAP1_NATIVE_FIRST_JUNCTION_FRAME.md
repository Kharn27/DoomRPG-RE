# ESP32 native Junction first gameplay frame milestone

Branch: `agent/esp32-native-first-junction-frame`

Base merged `main`:

```text
PR   = #87 — native sparse graphics catalog
main = 91a17414859fa12a0553e5b011956b6f95165780
```

Hardware-tested firmware:

```text
09f670a2f11e1cfce065c55aef8a4d3a5711a9a3
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Produce and present the first real native Junction gameplay framebuffer on the classic ESP32-2432S028R without reactivating the legacy gameplay loop or global graphics runtime.

The milestone consumes the hardware-proven compact Junction resident owners, player/view state, native PLAYING service and sparse graphics catalog, then renders exactly one deterministic 160x120 RGB565 frame using native PAK-backed wall and floor/ceiling texture access.

Input, turn advancement, legacy entities/monsters, sprites, HUD painting and gameplay dispatch remain outside this boundary.

## Permanent implementation

New native renderer owners:

```text
ESP32/include/esp_native_first_frame.h
ESP32/src/esp_native_first_frame.c
ESP32/include/esp_native_plane_renderer.h
ESP32/src/esp_native_plane_renderer.c
```

The first-frame owner is fixed at:

```text
EspNativeFirstFrameState = 48 B
```

`frameBeforeFNV` is intentionally not a stable cross-boot canon because the previously displayed logical framebuffer can differ at entry. Therefore the whole 48-byte state FNV is also only a same-run witness.

The stable rendered framebuffer canon is:

```text
frameAfterFNV = 8910c2ed
```

The post-PARK diagnostic viewport export independently proves the gameplay viewport bytes:

```text
viewport = 160x80 @ 0,20
viewportFNV = 032ffaed
BMP = /junction-viewport.bmp
BMP bytes = 38454
```

## Hardware-proven geometry and wall render

Real CYD final run:

```text
BSP nodes=39
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
resolvedTextures=30
wallCache=17H/17M/14E
animationTime=0
```

Wall texels are loaded through bounded cache slots from `/DoomRPG-ESP32.pak`; no map-wide texel payload is retained.

## Hardware-proven textured planes

The same first frame includes native textured floor/ceiling rendering:

```text
rows=80
pixels=12800
uniqueLogicalTextures=6
cache=12795H/5M/0E
texelReads=10240 B
```

The plane renderer is native and PAK-backed. Its cache is bounded and leaves no persistent heap delta after the frame.

## Stable graphics/resident predecessors

The renderer revalidates the exact predecessor owners before it can run:

```text
residentSnapshotFNV = bb714d80
runtimeFNV          = bc432a0f
mapFNV              = 8dba0bb4
scriptFNV           = bc9b18ff
lineFNV             = 3658710d
textureStateFNV     = 537319ad
automapFNV          = b699bd75
topologyFNV         = d6e8df7d
playerViewFNV       = afcdcf74
playingServiceFNV   = 4c50b853
graphicsCatalogFNV  = 969d5a77
textureRecordsFNV   = 2dd5dfcf
spriteRecordsFNV    = cfd036cf
```

The resident snapshot remains bit-identical across the render.

## Zero-persistent-RAM and side-effect proof

Final real-CYD run proved:

```text
heapDelta=0
largestDelta=0
legacyRenderStable=yes
packClosed=yes
```

The strict probe also verifies equality across the frame for:

```text
Game
Player
Hud
DoomCanvas
Render
Render.columnScale
legacy mediaPalettes
native graphics catalog
resident snapshot
```

Fail-closed behavior is verified for null Render, null player/view and wrong target map. A second call returns `ESP_NATIVE_FIRST_FRAME_ALREADY_ACTIVE` without mutating the published owner.

Legacy graphics/runtime invariants remain:

```text
Render.lines=NULL
Render.nodes=NULL
Render.mapSprites=NULL
mediaTexelOffsets=NULL
mediaBitShapeOffsets=NULL
mediaTexturesIds=NULL
mediaSpriteIds=NULL
shapeData=NULL
mediaTexels=NULL
mapTextureTexels=NULL
mapSpriteTexels=NULL
```

And gameplay remains deliberately inactive:

```text
legacy DoomCanvas.state = 9 / ST_INTRO
legacy Game.entities = 0
legacy Game.monsters = 0
inputConsumed = no
turnAdvanced = no
gameplayDispatch = no
legacy DoomCanvas_playingState = not called
legacy Render_render = not called
```

## Hardware-selected CYD presentation profile

The logical framebuffer itself is standard raw RGB565. A dedicated primary-color hardware probe proved that an additional software red/blue swap is incorrect.

The permanent presentation path is:

```text
logical framebuffer = 160x120 RGB565 / 38400 B
physical output      = 320x240
resampling           = exact nearest-neighbour x2
software saturation = none
software R/B swap   = none
TFT byte swap        = ON
panel inversion      = ON
TFT_RGB_ORDER        = TFT_BGR
ILI9341 driver       = ILI9341_2_DRIVER
SPI frequency        = 55 MHz
```

The stock `ILI9341_2_DRIVER` power, VCOM and frame-rate settings were retained. Hardware comparison of the exact canonical Junction framebuffer isolated the panel gamma as the source of the poor colour/contrast reproduction.

The real CYD selected profile **B / INV-GAMMA-WA** without contest. The permanent platform initialization programs the following 15-byte table into both ILI9341 gamma registers `0xE0` and `0xE1` while inversion remains enabled:

```text
00 15 17 07 11 06 2b 56 3c 05 10 0f 3f 3f 0f
```

This is a panel-side correction only. `PlatformVideo_present()` does not reinterpret framebuffer pixels; it duplicates each raw RGB565 logical pixel into an exact 2x2 physical block and sends it directly through TFT_eSPI.

The final hardware-tested firmware no longer contains the temporary `VIDEOCAL`, `VIDEOBOUNDARY`, `VIDEOPRIMARY` or `PANELCAL` presentation carousels.

## Final real-CYD evidence

Final hardware run on `09f670a2f11e1cfce065c55aef8a4d3a5711a9a3`:

```text
[NATIVEBOOT] READY validated predecessors silent passes=46 catalog=969d5a77

[NATIVEPLANE] rows=80 pixels=12800 textures=6 cache=12795H/5M/0E reads=10240B
[NATIVEFRAME] BSP nodes=39 leaves=12 nodeCull=8 lines=62 backface=20 clip=8 occluder=0 spriteSpanDeferred=0
[NATIVEFRAME] WALL requests=34 draws=34 spans=166 pixels=4341 cache=17H/17M/14E resolvedTextures=30 animationTime=0
[VIDEO] Present 160x120 -> 320x240 exact 2x raw RGB565: 34464 us
[JUNCTIONFRAME] COLORSTATS viewport=160x80@0,20 meanRGB=61/74/78 meanY=70 minY=0 maxY=255 binsY=0-63:4982 64-127:7613 128-191:199 192-255:6 neutral=497/12800 framebufferUntouched=yes
[JUNCTIONFRAME] READY stateBytes=48 stateFNV=f81692b2 frame=b366be13->8910c2ed walls=34 spans=166 wallPixels=4341 planes=12800 planeTex=6 cache=12795H/5M/0E presented=1
[JUNCTIONFRAME] GFX catalog=969d5a77 texture=2dd5dfcf sprite=cfd036cf planeReads=10240B resident=bb714d80 heapDelta=0 largestDelta=0 legacyRenderStable=yes packClosed=yes
[JUNCTIONFRAME] PARK nativeFirstFrame=yes texturedPlanes=yes firstFramePending=no spritesPending=yes hudPending=yes gameplayDispatchPending=yes legacyState=9 entities=0 monsters=0 noGameplay=yes
[JUNCTIONFRAME] BMP READY path=/junction-viewport.bmp size=38454 viewport=160x80@0,20 viewportFNV=032ffaed postParkDiagnostic=yes
```

The displayed frame was visually accepted on the real classic CYD after applying the permanent inverted-gamma panel profile. The title screen was also observed to be closer in colour fidelity, consistent with a panel-wide correction rather than a renderer-specific compensation.

## Hardware-proven PARK

```text
legacyState=9 / ST_INTRO
targetMap=9
junctionResident=yes
nativeST_PLAYING=yes
nativePlayingService=yes
nativeGraphicsCatalog=yes
nativeFirstFrame=yes
texturedPlanes=yes
firstFramePending=no
spritesPending=yes
hudPending=yes
gameplayDispatchPending=yes
initialSavePersistencePending=yes
entities=0
monsters=0
noGameplay=yes
```

## Architectural consequence

The ESP32 port now owns a real gameplay-visible Junction framebuffer independently of the legacy `DoomCanvas_playingState()` / `Render_render()` path. The rendering data path is:

```text
/DoomRPG-ESP32.pak
 -> compact native graphics catalog
 -> bounded native wall/plane texture caches
 -> native BSP traversal + rasterization
 -> 160x120 RGB565 framebuffer
 -> raw nearest-neighbour x2 CYD presentation
```

`shapeData` and `mediaTexels` remain `NULL`, and no global map-wide texel arrays are recreated.

## Next bounded milestone after merge

Recover the exact merged `main`, then extend the same deterministic first-frame boundary with **native sprite rendering** using the already hardware-proven 16 sparse sprite catalog records.

Keep that milestone strict:

```text
same Junction pose/frame boundary
input consumed = no
turn advanced = no
gameplay dispatch = no
legacy Game.entities = 0
legacy Game.monsters = 0
legacy DoomCanvas.state = ST_INTRO
legacy DoomCanvas_playingState = not called
legacy Render_render = not called
shapeData = NULL
mediaTexels = NULL
runtime ZIP = forbidden
bounded sprite cache only
HUD painting still deferred unless separately scoped
```
