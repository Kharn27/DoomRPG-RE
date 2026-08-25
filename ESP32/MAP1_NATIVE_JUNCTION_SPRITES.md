# MAP1 native Junction sprite rendering

## Status

```text
branch = agent/esp32-native-junction-sprites
base = d8da51e5a3b9700d1806110f56f553a422d7d182
hardware-tested firmware = 3fdb2905b1d49ef1112a9e9df7a5db7e278897bd
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

This milestone extends the already hardware-proven native Junction walls+textured-planes frame with the first real native gameplay sprite rasterization while the legacy gameplay/render loop remains parked.

## Permanent renderer boundary

Permanent files:

```text
ESP32/include/esp_native_junction_sprite_renderer.h
ESP32/src/esp_native_junction_sprite_renderer.c
```

Diagnostic/probe support used during the milestone:

```text
ESP32/src/native_junction_sprite_census_probe.c
ESP32/src/native_junction_sprite_overlay_probe.c
ESP32/src/native_junction_sprite_fidelity_probe.c
```

The renderer consumes the compact map runtime, compact sprite topology, sparse native graphics catalog and `/DoomRPG-ESP32.pak` ranges. It does not install the legacy resident graphics pools.

## Hardware-proven predecessor frame

The sprite pass starts from the already canonical Junction frame:

```text
frameBefore = 8910c2ed
viewportBefore = 032ffaed
viewport = 160x80 @ 0,20
```

The same BSP/depth traversal remains exact:

```text
nodes=39
leaves=12
nodeCull=8
lines=62
backface=20
clip=8
occluder=0
spriteSpan=0
```

## Hardware-proven view-sprite admission

A temporary read-only census reproduced legacy `Render_relinkSprite()` leaf ownership plus the stateful BSP depth/cull walk and proved:

```text
mapSprites=48
BSP-visible candidates=21
BSP-rejected=27
hidden=0
candidate modes=0:14 / 7:7
candidateFNV=23ef1895
```

The permanent renderer now uses that same BSP walk for both wall depth and visible-leaf admission. Sprites in rejected leaves never enter sort/raster and never load bitshape/texel payloads.

The hardware-tested permanent pass reports:

```text
objects=48
bspCandidates=21
bspRejected=27
orderFNV=f16737cb
```

The prior map-wide implementation rendered the same final pixels because the 27 rejected objects were already fully clipped/occluded. The BSP-visible filter is therefore an architectural/performance correction, not a framebuffer change.

## Hardware-proven billboard raster

Supported family in this milestone:

```text
standard billboards only
animationTime=0
legacy intrinsic mode 0
legacy intrinsic mode 7 for logical IDs 136/137/144
```

Physical bitshape IDs are resolved with bounded `mappings.bin` reads. Bitshape masks, sprite texels and palettes are read from the native PAK into one temporary bounded workspace. No map-wide sprite texels are retained.

Stable hardware stats:

```text
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

The mode-7 RGB565 additive path follows the legacy saturation rule exactly.

## Stable framebuffer canons

```text
frameAfterFNV = 299506eb
viewportAfterFNV = ae2246eb
BMP path = /junction-sprite-viewport.bmp
BMP bytes = 38454
viewport = 160x80 @ 0,20
```

The BMP is a post-PARK diagnostic export of the logical framebuffer and bypasses panel/gamma/camera effects.

## RAM / ownership / side-effect proof

Real-CYD evidence proved:

```text
heapDelta=0
largestDelta=0
legacyRenderStable=yes
topology=d6e8df7d
catalog=969d5a77
packClosed=yes
```

The renderer uses one bounded temporary workspace and frees it before the probe postcondition. The following invariants remain true:

```text
shapeData=NULL
mediaTexels=NULL
mediaTexelOffsets=NULL
mediaBitShapeOffsets=NULL
mediaTexturesIds=NULL
mediaSpriteIds=NULL
legacy Game.entities=0
legacy Game.monsters=0
legacy DoomCanvas.state=ST_INTRO
legacy Render_render not called
runtime ZIP forbidden
```

No input is consumed, no turn advances and no gameplay dispatch occurs.

## Real-CYD evidence

Final tested firmware produced:

```text
[JUNCTIONSPRITE] DEPTH nodes=39 leaves=12 nodeCull=8 lines=62 backface=20 clip=8 occluder=0 spriteSpan=0 orderFNV=f16737cb parity=firstFrame
[JUNCTIONSPRITE] READY frame=8910c2ed->299506eb objects=48 bspCandidates=21 bspRejected=27 modes=0:14/7:7 mode7Pixels=311 draws=21 nearCull=0 clipCull=0 spans=219 pixels=1828 wallOccludedCols=62 frames=21 uniqueLogical=9 frameBytes=12251 maxFrame=1020 packReads=130 glowDeferred=7 heapDelta=0 largestDelta=0 legacyRenderStable=yes topology=d6e8df7d catalog=969d5a77 packClosed=yes presented=1
[JUNCTIONSPRITE] PARK baseBillboards=yes bspVisibleOnly=yes intrinsicMode7=yes depthBspParity=yes glowPending=yes noLegacyGraphicsPools=yes noWorldMutation=yes
[JUNCTIONSPRITE] BMP READY path=/junction-sprite-viewport.bmp size=38454 viewport=160x80@0,20 viewportFNV=ae2246eb frame=299506eb postParkDiagnostic=yes
```

## Current PARK

```text
nativeFirstFrame=yes
nativeBaseBillboards=yes
bspVisibleOnly=yes
intrinsicMode7=yes
glowPending=yes
hudPending=yes
gameplayDispatchPending=yes
legacyState=ST_INTRO
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Next bounded milestone

Close the implicit sprite dependency family used by legacy glow spawning:

```text
135/140 -> companion logical sprite 136, mode 7
131 -> companion logical sprite 144, mode 7 when encountered
```

For the current Junction view, seven glow companions are pending. Extend the sparse graphics dependency closure so those implicit resources are explicitly owned by the native catalog, then render only those companions. Keep all current memory/world/gameplay invariants unchanged.
