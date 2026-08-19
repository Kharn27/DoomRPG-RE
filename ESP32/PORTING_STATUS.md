# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R (classic Cheap Yellow
Display, no PSRAM) port. Update it as part of every hardware-validated increment,
on the same branch as the code, before that branch is merged.

## Target

- ESP32-2432S028R / classic ESP32 CYD, no PSRAM
- ESP32-D0WD-V3, 240 MHz, 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch on separate software SPI path
- microSD-backed game data
- internal render target: 160x120 RGB565 = 38,400 bytes
- physical output: exact nearest-neighbour 2x to 320x240
- audio disabled/stubbed during bring-up

## Project direction

DoomRPG-RE is treated as an executable specification for behaviour, formats and
rendering semantics, not as an architecture contract. The ESP32 implementation
is progressively becoming its own engine specialized for a classic no-PSRAM
ESP32:

- bounded deterministic RAM use
- SD-backed immutable resources
- small measured working sets / caches
- no whole-resource graphics inflation
- no monolithic `shapeData`
- no monolithic `mediaTexels`
- shared 160x120 RGB565 framebuffer
- canonical RGB565 palette convention for native consumers
- original game data and behaviour preserved where practical

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
17. Native sprite-texel random access: exact 143,990-byte selected sprite dataset
    measured without loading it, largest real menu sprite payload only 1,600 B,
    direct `stexels.bin` read and exact heap recovery
    (`agent/esp32-native-sprite-texel-probe`, PR #16, merge commit
    `6252dcf7f6cdb782bbf554b7ebeae9b296086d25`).
18. First complete ESP32-native sprite render: sprite 172 loaded from
    `bitshapes.bin` + `stexels.bin`, native palette normalization, 3,199 pixels
    rasterized into the shared framebuffer with `shapeData == NULL` and
    `mediaTexels == NULL` (`agent/esp32-native-sprite-render-consumer`, PR #17,
    merge commit `bf59ee37b7242d0917ae0e4b49a83265c0b4f652`).

## Current validated increment

Branch: `agent/esp32-native-wall-render-consumer`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: prove the second complete ESP32-native graphics consumer: read one
real 64x64 wall texture directly from `wtexels.bin`, apply its real 16-color
palette, rasterize all 4,096 texels into the shared framebuffer, present it on
the physical CYD, and recover the exact starting heap layout without ever
creating `mediaTexels`.

## Authoritative memory boundary

Before the wall consumer starts, real menu structures and all previous native
resource work remain valid with the two forbidden legacy graphics pools absent:

```text
heap8=30,688 B
largest8=21,492 B
shapeData=0x0
mediaTexels=0x0
```

Current measured graphics working sets:

```text
legacy expanded shapeData          = 55,676 B          (forbidden)
selected packed sprite texels      = 143,990 B         (not resident)
wall-only legacy mediaTexels       = 172,032 B         (forbidden)
bitshape mask-column scratch       <= 32 B
largest selected sprite payload     = 1,600 B
largest validated sprite frame      = 2,112 B logical
one packed wall texture             = 2,048 B
```

## Native wall source contract

The test texture is the first real texture selected by `menu.bsp`:

```text
textureIndex      = 112
texelOffset       = 65536
wtexels byteOffset= 32768
paletteOffset     = 480
packed texels     = 2048 B
size              = 64x64 / 4 bpp
```

The original renderer's wall semantics are preserved: wall texels are addressed
as 64 columns of 64 texels, i.e. logical texel index `x * 64 + y`, and the
second mapping integer is the 16-color palette offset.

No map-wide texture pool is created. The native frame contains only the 2,048
packed bytes for the requested wall texture.

## Hardware-validated wall render

Authoritative hardware log:

```text
=== Doom RPG ESP32-native wall render consumer ===
[WALLRENDER] Begin texture=112 heap8=30688 largest8=21492 shapeData=0x0 mediaTexels=0x0
[WALLRENDER] Loaded texture=112 texelOffset=65536 byteOffset=32768 paletteOffset=480 packed=2048B
[WALLRENDER] Texel fnv1a=92d40704 expected=92d40704 first=aab544b4 last=e5eeeece
[WALLRENDER] Frame resident after pack close heap8=28624 largest8=21492 used=2064B logicalStorage=2048B
[WALLRENDER] DRAW texture=112 origin=48,28 drawn=4096 paletteOffset=480 framebufferFNV=e39af2c4
[VIDEO] Present 160x120 -> 320x240 exact 2x: 34377 us
[WALLRENDER] Presented native wall texture on shared 160x120 framebuffer -> CYD 320x240
[WALLRENDER] Released frame heap8=30688 largest8=21492 deltaFromStart=0
[WALLRENDER] READY real wall texture rendered without mediaTexels
[WALLRENDER] READY native wall frame contract = 2048B packed texels + existing 16-color palette
[WALLRENDER] Runtime cache/eviction policy still intentionally NOT introduced
[MAPSTRUCT] Native wall render consumer complete; full map texel loading remains blocked
[MENUBSP] READY menu.bsp plan + real runtime structures validated
[READY] Bring-up remains alive; touch still runs the SDL video test.
[ALIVE] uptime=5003 ms heap=96504 heap8=30688 largest8=21492 ... MENUBSP=ready ...
```

The source bytes exactly match the earlier native asset-pack proof:

```text
fnv1a = 92d40704
first = aa b5 44 b4
last  = e5 ee ee ce
```

The deterministic framebuffer signature for the exact diagnostic scene is:

```text
framebufferFNV=e39af2c4
```

Keep this value as a regression marker unless the diagnostic background/layout
or palette convention intentionally changes.

## Heap recovery

The native wall frame requests exactly 2,048 bytes. The ESP32 allocator consumes
2,064 bytes while it is resident:

```text
before       heap8=30688 largest8=21492
resident     heap8=28624 largest8=21492
released     heap8=30688 largest8=21492 deltaFromStart=0
```

The largest free block remains unchanged at 21,492 bytes throughout the bounded
wall-frame allocation. The rendered pixels remain in the shared framebuffer
after the source frame is freed.

## Validated native graphics contracts

### Sprite

```text
DoomRPG-ESP32.pak
  -> bitshapes.bin source header + bounded mask
  -> stexels.bin bounded packed payload (menu max 1,600 B)
  -> 16-color palette
  -> native RGB565
  -> shared framebuffer
```

Validated worst-case sprite frame:

```text
sprite=172
mask=512 B
texels=1600 B
logical frame=2112 B
allocator cost=2128 B
active pixels=3199
texel FNV=0c0a7acd
framebuffer FNV=001910a9
```

### Wall

```text
DoomRPG-ESP32.pak
  -> wtexels.bin direct 2,048 B read
  -> mapped 16-color palette
  -> native RGB565
  -> shared framebuffer
```

Validated wall frame:

```text
texture=112
texels=2048 B
allocator cost=2064 B
pixels=4096
texel FNV=92d40704
framebuffer FNV=e39af2c4
```

Both paths operate with:

```text
shapeData   = NULL
mediaTexels = NULL
```

## Architectural conclusion

We now have two independent complete graphics paths from original Doom RPG data
on SD to visible pixels on the CYD:

```text
                 DoomRPG-ESP32.pak
                         |
             +-----------+-----------+
             |                       |
          sprite                    wall
   bitshape + stexels            wtexels
             |                       |
       ~2.1 KB frame              2 KB frame
             |                       |
             +-----------+-----------+
                         |
                    RGB565 palette
                         |
                 native rasterizers
                         |
                 shared framebuffer
                         |
                    CYD 320x240
```

The next architectural step should therefore stop duplicating pack-open / lookup /
load / release logic in isolated probes and introduce a small reusable ESP32
resource manager/cache boundary shared by native wall and sprite consumers.

That manager must remain bounded and measured. Do **not** choose a large slot
count blindly and do not recreate map-wide resource residency under another
name.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)`
- real structural `Render_beginLoadMapData()` phase
- real nodes, lines, sprites, events, bytecodes and resource reference lists
- full 241-entry native asset pack validation
- zero resident `shapeData`
- zero resident `mediaTexels`
- on-demand bitshape source model
- exact selected sprite dataset measurement: 143,990 B
- worst-case selected sprite payload: 1,600 B
- native sprite frame + physical CYD render
- canonical RGB565 palette normalization for native consumers
- direct random-access read of a real menu wall texture
- native wall frame + physical CYD render
- deterministic sprite framebuffer hash `001910a9`
- deterministic wall framebuffer hash `e39af2c4`
- exact heap recovery after both native graphics consumers

Still intentionally NOT executed / integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic `mediaTexels`
- reusable native resource/cache manager
- cache hit/miss/eviction policy based on measured renderer access
- replacement of the real projected wall-span call path
- replacement of the real `Render_renderSprite()` call path
- full floor/ceiling native texture consumption
- completion of map graphics loading
- game entities/player spawning
- main game loop

## Recommended next increment after merge

Introduce the first reusable **ESP32 native graphics resource manager** around
the already proven contracts, rather than adding another independent probe.

The first version should be intentionally small:

- one API for acquiring/releasing a packed wall texture by texture index
- one API for acquiring/releasing a sprite frame by sprite index
- bounded storage only
- no arbitrary multi-slot cache yet unless access measurements justify it
- counters/timing for requests, SD reads, hits/misses and peak resident bytes
- native consumers depend on this API rather than knowing pack offsets directly

After that contract is hardware-proven, use it inside one existing projected wall
or sprite rendering path and observe real runtime access patterns before deciding
cache size and eviction policy.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- **Documentation is part of the increment:** after hardware success, update all
  relevant `.md` files on the same branch before merge.
- A branch is not considered complete/merge-ready while its code and recovery
  documentation disagree.
- Keep `ESP32/README.md` updated with commands, SD preparation, conventions and
  architecture decisions so operational knowledge is not left only in chat.
