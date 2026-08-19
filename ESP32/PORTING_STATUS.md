# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R (classic Cheap Yellow
Display, no PSRAM) port. Update it after every hardware-validated increment.

## Target

- ESP32-2432S028R / classic ESP32 CYD, no PSRAM
- ESP32-D0WD-V3, 240 MHz, 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch on separate software SPI path
- microSD-backed game data
- internal render target: 160x120 RGB565 = 38,400 bytes
- physical output: exact nearest-neighbour 2x to 320x240
- audio disabled/stubbed during bring-up

## Hardware-validated milestones already merged to main

1. TFT / SD / engine link bring-up.
2. Touch calibration/orientation (`agent/esp32-touch-input-layer`, PR #1).
3. Native 160x120 render target + exact 2x output (PR #2).
4. SDL compatibility renderer sharing the platform framebuffer (PR #3).
5. Real 12-object Doom RPG core graph (PR #4).
6. Real `DoomCanvas_startup()` + HUD/font/legal resources + 160x120 layout
   (`agent/esp32-engine-layout-160x120`, PR #5).
7. `ParticleSystem_startup()`, `MenuSystem_startup()` and `EntityDef_startup()`
   with packed indexed BMP storage (`agent/esp32-pre-render-startup`, PR #6).
8. Real `Render_startup()` using the shared platform framebuffer, plus
   `sintable.bin` and `palettes.bin` (`agent/esp32-render-startup-shared-framebuffer`, PR #7).
9. `Game_loadConfig()` first-boot path + real `Render_loadMappings()`
   (`agent/esp32-config-mappings-startup`, PR #8).
10. Real `/menu.bsp` ZIP load/inflate + exact fixed 33-byte header parse
    (`agent/esp32-menu-bsp-preflight`, PR #9).
11. Complete byte-for-byte parse of `menu.bsp` and exact ESP32 structural
    allocation plan (`agent/esp32-menu-bsp-structure-plan`, PR #10).
12. Real `Render_beginLoadMap(MAP_MENU)` + real structural portion of
    `Render_beginLoadMapData()`, stopped exactly before graphics resources
    (`agent/esp32-menu-map-runtime-structures`, PR #11).
13. Graphics-resource memory plan proving the original whole-file inflate and
    monolithic `mediaTexels` strategy cannot fit on the no-PSRAM CYD
    (`agent/esp32-menu-resource-memory-plan`, PR #12).
14. First ESP32-native asset-pack primitive: direct random access to a real
    2,048-byte menu wall texture with exact heap recovery
    (`agent/esp32-native-asset-pack`, PR #13).
15. Full ESP32-native asset pack v2: all 241 ZIP resources mirrored uncompressed
    behind a hash-sorted on-disk index, with 241/241 hardware cross-check and
    exact heap recovery (`agent/esp32-full-native-asset-pack`, PR #14).
16. Native on-demand bitshape source model: 112 unique menu shapes across 284
    sprite references, zero resident `shapeData`, <=32-byte mask-column scratch,
    exact heap recovery (`agent/esp32-native-bitshape-loader`, PR #15, merge
    commit `76898e7990481cf630b293477958fe0c478f7f1a`).

## Current validated increment

Branch: `agent/esp32-native-sprite-texel-probe`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: prove that real sprite texel payloads can be addressed and read
directly from `stexels.bin` inside `DoomRPG-ESP32.pak`, measure the actual
maximum single-sprite working set for the menu, and keep both legacy
`shapeData` and monolithic `mediaTexels` absent.

## Hardware-validated sprite texel source model

Authoritative starting point after the native bitshape walk:

```text
[BITSHAPE] Begin refs=284 heap8=30688 largest8=21492 shapeData=0x0
[BITSHAPE] Selected unique=112 refs=284 max=64x64 pitch=8B
[BITSHAPE] Legacy expanded shapeData=55676B (27838 words) -> ESP32 resident=0B
[BITSHAPE] Exact selected sprite texels=143990B packed across 284 refs activePixels=287848
[BITSHAPE] Pack closed heap8=30688 largest8=21492 deltaFromStart=0
```

The sprite probe starts with the two forbidden legacy pools absent:

```text
[SPRITETEX] Begin refs=284 heap8=30688 largest8=21492 shapeData=0x0 mediaTexels=0x0
```

Opening the native pack still costs 4,376 bytes temporarily:

```text
[SPRITETEX] Pack open heap8=26312 largest8=21492 cost=4376B
```

The source layout is now hardware-proven:

```text
[SPRITETEX] Source bases wallData=116736B spriteBaseTexel=233472 stexelsHeader=126614 stexelsSize=126618B
```

`bitshapes.bin` stores sprite texel offsets in the same global logical texel
space used by the original loader. The sprite region begins after the wall
logical texel region, so the ESP32-native reader converts a sprite texel offset
to a byte range inside `stexels.bin` without constructing `mediaTexels`.

## Cross-check against the bitshape measurement

The independent sprite-texel probe reproduces the exact dataset totals measured
by the previous bitshape increment:

```text
[SPRITETEX] Selected unique=112 refs=284 packedTotal=143990B activePixels=287848
```

This matches:

```text
[BITSHAPE] Exact selected sprite texels=143990B packed across 284 refs activePixels=287848
```

The agreement between the two independent walks is the current consistency
check for source bitshape decoding and sprite texel addressing.

## Largest real menu sprite working set

The largest selected sprite payload discovered on hardware is:

```text
[SPRITETEX] Largest sprite=172 sourceOffset=15749 texelOffset=292040 stexelsByteOffset=29288 bounds=64x64 active=3199 packed=1600B refs=1
```

Therefore the real menu worst-case **single sprite payload** is only:

```text
1,600 B packed
```

This is the important working-set result. The complete selected sprite dataset
is 143,990 bytes, but no map-wide sprite texel pool is required merely to access
one real sprite.

## Bounded real payload read

The 1,600-byte worst-case payload fits comfortably inside the validated menu
heap boundary:

```text
[SPRITETEX] Payload preflight heap8=26312 largest8=21492 required=1600B fitAggregate=yes fitContiguous=yes
```

The actual allocator cost is 1,616 bytes:

```text
[SPRITETEX] Largest payload resident heap8=24696 largest8=21492 used=1616B
```

A real direct read from `stexels.bin` succeeds:

```text
[SPRITETEX] READ sprite=172 bytes=1600 fnv1a=0c0a7acd first=8e887997 last=77979709
```

After freeing the bounded payload, both free heap and largest block return
exactly to the pre-payload values:

```text
[SPRITETEX] Released payload heap8=26312 largest8=21492 delta=0
```

After closing the pack, the complete probe returns to the exact starting heap
layout:

```text
[SPRITETEX] Pack closed heap8=30688 largest8=21492 deltaFromStart=0
```

Final hardware result:

```text
[SPRITETEX] READY largest selected sprite payload read directly from stexels.bin
[SPRITETEX] READY sprite working-set ceiling measured without shapeData or mediaTexels
[SPRITETEX] Cache size intentionally NOT chosen until hardware result is known
[MAPSTRUCT] Native sprite texel random-access probe complete; full texel loading remains blocked
[MENUBSP] READY menu.bsp plan + real runtime structures validated
```

Heartbeat remains stable afterwards:

```text
[ALIVE] uptime=5000 ms heap=96504 heap8=30688 largest8=21492 ... MENUBSP=ready ...
[ALIVE] uptime=10001 ms heap=96504 heap8=30688 largest8=21492 ... MENUBSP=ready ...
```

Touch and shared-framebuffer presentation also remain operational.

## Architectural conclusion

The graphics dataset sizes remain far too large for map-wide resident pools:

```text
legacy selected shapeData          = 55,676 B
selected source bitshape masks     = 30,813 B
selected packed sprite texels      = 143,990 B
wall-only legacy mediaTexels       = 172,032 B
```

But the newly measured menu working sets are small:

```text
bitshape mask scratch              <= 32 B
largest selected sprite payload     = 1,600 B
one packed wall texture             = 2,048 B
```

This strongly supports an ESP32-native renderer architecture based on bounded
on-demand resource consumers rather than original map-wide expanded graphics
pools.

Validated source path for sprites:

```text
mediaBitShapeOffsets source offset
        |
        v
DoomRPG-ESP32.pak / bitshapes.bin
        |
        +--> 12-byte shape header
        +--> bounded mask decode
        +--> global sprite texel offset
        |
        v
convert against wall logical base
        |
        v
DoomRPG-ESP32.pak / stexels.bin
        |
        v
bounded packed sprite payload (<= 1,600 B for menu)
```

No cache slot count or eviction policy has been chosen yet. The cache design
must follow measured renderer access behaviour, not simply the maximum dataset
size.

## Current authoritative memory boundary

After real menu structures are resident and all current native resource probes
are closed:

```text
heap8=30,688 B
largest8=21,492 B
```

Temporary native-pack open cost:

```text
4,376 B
```

Worst-case measured single-sprite payload allocation:

```text
requested payload                  = 1,600 B
allocator-observed cost            = 1,616 B
```

All temporary memory is fully recovered after release/close.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)`
- real structural `Render_beginLoadMapData()` phase
- real nodes, lines, sprites, events, bytecodes and resource reference lists
- full 241-entry native asset pack validation
- direct random-access read of a real menu wall texture
- exact enumeration of 112 unique bitshapes used by 284 menu sprite refs
- zero resident `shapeData`
- direct source-header and source-mask reads from `bitshapes.bin`
- exact selected sprite texel dataset measurement: 143,990 B
- independent sprite texel address cross-check against the same 143,990 B total
- exact largest selected sprite payload measurement: 1,600 B
- real direct 1,600-byte read from `stexels.bin`
- exact payload and pack heap recovery

Still intentionally NOT executed / integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic `mediaTexels`
- native sprite cache used by the real renderer
- native wall-texture cache used by the real renderer
- direct bitshape source consumption inside sprite rasterization
- completion of map graphics loading
- game entities/player spawning
- main game loop

## Recommended next increment after merge

The next increment can now stop measuring file format semantics and begin
building the first reusable **ESP32-native sprite resource consumer** for the
renderer.

Do not allocate 143,990 bytes and do not choose a large fixed cache blindly.
Use the proven 1,600-byte maximum selected payload plus actual renderer access
patterns to design a small bounded cache or decode buffer. The first integration
should remain narrow: one existing renderer sprite path should consume native
bitshape metadata and packed sprite texels without `shapeData` or `mediaTexels`.

A similarly bounded wall path has already been proven with a 2,048-byte packed
wall texture and should eventually share the same resource-management
philosophy.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
- Keep `ESP32/README.md` updated with commands, SD preparation and tooling so
  operational knowledge is not left only in chat history.
