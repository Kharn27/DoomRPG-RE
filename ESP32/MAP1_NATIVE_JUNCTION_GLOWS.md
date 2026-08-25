# MAP1 native Junction glow companions

## Status

```text
branch = agent/esp32-native-junction-glows
base = 674b45bbd115cd8f9202f2ce2d7132550c3bb75e
hardware-tested firmware = 338388ee4166115585e2c964aa95e79d5b0313eb
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

This milestone closes the implicit native sprite dependency required by the legacy Junction glow family and renders the seven real glow companions while the legacy gameplay/render loop remains parked.

## Permanent boundary

Permanent files added/extended by this milestone:

```text
ESP32/include/esp_native_bsp_visibility.h
ESP32/src/esp_native_bsp_visibility.c
ESP32/include/esp_native_graphics_catalog.h
ESP32/src/esp_native_graphics_catalog.c
ESP32/include/esp_native_junction_sprite_renderer.h
ESP32/src/esp_native_junction_sprite_renderer.c
```

The stateful compact BSP walk is now a reusable native visibility/depth primitive. It publishes visited leaves plus 160 column-depth values without framebuffer mutation or persistent scratch ownership, and restores borrowed legacy `Render` scratch before return.

## Hardware-proven catalog dependency closure

The previously merged direct sparse catalog remains the validated predecessor:

```text
direct stateFNV=969d5a77
textureCount=30
spriteCount=16
storageBytes=1840
textureFNV=2dd5dfcf
spriteFNV=cfd036cf
```

The new dependency-closure API atomically adds the implicit resource required by current Junction glow spawning:

```text
135/140 -> companion logical sprite 136, mode 7
```

Real-CYD result:

```text
closed stateFNV=257444a5
textureCount=30
spriteCount=17
storageBytes=1880
dependency=136
persistent increment=40 B
largest free block unchanged=34804
repeatAtomic=yes
packClosed=yes
```

The old catalog remains published if allocation or native PAK reads fail. No ad-hoc resource read bypasses catalog ownership.

Logical sprite 144 remains part of the generic dependency rule for `131 -> 144` when a future resident map/view actually requires it; current Junction closure requires only 136.

## Base billboard predecessor remains exact

The shared native BSP visibility path preserves the already hardware-proven Junction billboard pass exactly:

```text
nodes=39
leaves=12
nodeCull=8
lines=62
backface=20
clip=8
occluder=0
spriteSpan=0
orderFNV=f16737cb

objects=48
bspCandidates=21
bspRejected=27
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
```

The hardware probe reported `preserved=yes`; the glow milestone therefore adds only the missing legacy companion family on top of the same proven base sequence.

## Hardware-proven glow rendering

Legacy draws each glow immediately after its base parent rather than as a map-wide final overlay. The native renderer now keeps that ordering.

Current Junction pose:

```text
glow companions=7
glow draws=7
nearCull=0
clipCull=0
spanRuns=59
pixels=1917
wallOccludedCols=32
frameLoads=7
frameBytes=5572
maxFrameBytes=796
packReads=172
render mode=7 additive RGB565 saturation
```

The lamps on the real CYD visibly show the additive glow effect.

## Stable framebuffer canons

Predecessor walls+planes frame remains:

```text
frameBeforeFNV=8910c2ed
```

Complete BSP-visible sprite + glow frame:

```text
frameAfterFNV=b5218f24
viewportAfterFNV=9206eb24
BMP path=/junction-sprite-viewport.bmp
BMP bytes=38454
viewport=160x80 @ 0,20
```

The earlier no-companion sprite framebuffer `299506eb` / viewport `ae2246eb` remains the predecessor canon, not the final glow-frame canon.

## RAM / ownership / side-effect proof

Real-CYD evidence:

```text
catalog persistent increment=40 B
renderer heapDelta=0
renderer largestDelta=0
topology=d6e8df7d
closed catalog=257444a5
packClosed=yes
presented=1
```

Mandatory invariants remain true:

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
no input consumed
no turn advancement
no gameplay dispatch
no world/entity mutation
```

All bitshape/texel/palette reads remain bounded native PAK range reads. The sprite/glow workspace is temporary and released before the postcondition.

## Real-CYD evidence

Hardware-tested firmware printed:

```text
[JUNCTIONGLOWCAT] READY direct=969d5a77 closed=257444a5 textures=30 sprites=17 storage=1880 dependency=136 directTextureFNV=2dd5dfcf directSpriteFNV=cfd036cf heapIncrement=40 largest=34804->34804 repeatAtomic=yes packClosed=yes
[JUNCTIONSPRITE] DEPTH nodes=39 leaves=12 nodeCull=8 lines=62 backface=20 clip=8 occluder=0 spriteSpan=0 orderFNV=f16737cb parity=firstFrame
[JUNCTIONSPRITE] BASE objects=48 bspCandidates=21 bspRejected=27 modes=0:14/7:7 mode7Pixels=311 draws=21 nearCull=0 clipCull=0 spans=219 pixels=1828 wallOccludedCols=62 frames=21 uniqueLogical=9 frameBytes=12251 maxFrame=1020 preserved=yes
[JUNCTIONGLOW] READY frame=8910c2ed->b5218f24 companions=7 draws=7 nearCull=0 clipCull=0 spans=59 pixels=1917 wallOccludedCols=32 frames=7 frameBytes=5572 maxFrame=796 packReads=172 heapDelta=0 largestDelta=0 topology=d6e8df7d catalog=257444a5 packClosed=yes presented=1
[JUNCTIONGLOW] PARK baseBillboards=yes bspVisibleOnly=yes intrinsicMode7=yes glowCompanions=yes glowPending=no depthBspParity=yes noLegacyGraphicsPools=yes noWorldMutation=yes
[JUNCTIONSPRITE] BMP READY path=/junction-sprite-viewport.bmp size=38454 viewport=160x80@0,20 viewportFNV=9206eb24 frame=b5218f24 postParkDiagnostic=yes
```

## Current PARK

```text
nativeFirstFrame=yes
nativeBaseBillboards=yes
bspVisibleOnly=yes
intrinsicMode7=yes
glowCompanions=yes
glowPending=no
hudPending=yes
gameplayDispatchPending=yes
legacyState=ST_INTRO
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Next bounded milestone

The current Junction world view now contains walls, textured planes, BSP-visible base billboards, intrinsic mode-7 sprites and their required glow companions. The next coherent visible boundary is native gameplay HUD painting from the already-owned native player/view/HUD intents, still without input consumption, turn advancement or gameplay dispatch.

Keep the same 160x120 framebuffer/presentation path, native PAK ownership rules, bounded scratch policy and all no-legacy-world/render-pool invariants.