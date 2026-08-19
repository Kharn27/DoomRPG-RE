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
- small working sets / caches
- no whole-resource graphics inflation
- no monolithic `shapeData`
- no monolithic `mediaTexels`
- shared 160x120 RGB565 framebuffer
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

## Current validated increment

Branch: `agent/esp32-native-sprite-render-consumer`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: prove the first complete ESP32-native graphics path from original game
resources on SD to visible RGB565 pixels on the CYD, without `shapeData` and
without `mediaTexels`.

This increment is the first one where a real Doom RPG sprite is not only
inspected but actually rasterized by an ESP32-native consumer.

## Authoritative pre-render memory boundary

After the real menu structures are resident and all preceding resource probes
have closed:

```text
heap8=30,688 B
largest8=21,492 B
shapeData=0x0
mediaTexels=0x0
```

The previous native resource measurements remain authoritative:

```text
selected unique bitshapes          = 112
menu sprite references             = 284
legacy expanded shapeData          = 55,676 B
ESP32 resident shapeData           = 0 B
selected source masks total        = 30,813 B
selected packed sprite texels      = 143,990 B
selected active pixels total       = 287,848
largest selected sprite payload    = 1,600 B
one packed wall texture            = 2,048 B
```

## Native sprite source used for the first renderer consumer

Hardware-selected worst-case menu sprite:

```text
sprite index        = 172
sourceOffset        = 15749
texelOffset         = 292040
stexelsByteOffset   = 29288
bounds              = 64x64
active pixels       = 3199
packed texels       = 1600 B
paletteOffset       = 1616
```

The bounded frame representation used by the new consumer is:

```text
bitshape mask       = 512 B
packed texels       = 1600 B
logical storage     = 2112 B
allocator cost      = 2128 B
```

No map-wide sprite pool is materialized.

## Hardware-validated source integrity

The independent bitshape and texel probes still reproduce the exact same menu
sprite dataset totals:

```text
[BITSHAPE] Exact selected sprite texels=143990B packed across 284 refs activePixels=287848
[SPRITETEX] Selected unique=112 refs=284 packedTotal=143990B activePixels=287848
```

The selected sprite payload remains byte-for-byte stable:

```text
[SPRITETEX] READ sprite=172 bytes=1600 fnv1a=0c0a7acd first=8e887997 last=77979709
[SPRITERENDER] Texel fnv1a=0c0a7acd expected=0c0a7acd
```

This proves the visible result is using the same validated `stexels.bin` payload
as the previous random-access increment.

## Native palette convention

The existing DoomRPG-RE `Render_loadPalettes()` leaves `mediaPalettes` in the
legacy red/blue ordering expected by the reconstructed renderer path. The
ESP32-native framebuffer contract is canonical RGB565, so native consumers now
perform one explicit palette normalization before rendering.

Hardware log:

```text
[PALETTE] Normalized 3280 entries legacy R/B order -> framebuffer RGB565
[PALETTE] sprite172 offset=1616 first4 before=0000,ffff,c000,07ff after=0000,ffff,0018,ffe0
[PALETTE] READY native consumers now see canonical RGB565
```

The BREW pixel-corruption patch is unrelated to this conversion: that patch
changes the way `wtexels`/`stexels` are read on affected BREW devices. Our
sprite texel hash is already exact and the observed issue was a clean color
channel mismatch, not pixel corruption.

For the ESP32 engine, palette ordering must therefore be normalized at the
native rendering boundary rather than by modifying sprite texel data.

## First real ESP32-native sprite render

Hardware result:

```text
=== Doom RPG ESP32-native sprite render consumer ===
[SPRITERENDER] Begin sprite=172 heap8=30688 largest8=21492 shapeData=0x0 mediaTexels=0x0
[SPRITERENDER] Loaded sprite=172 sourceOffset=15749 paletteOffset=1616 bounds=0..63,0..63 size=64x64
[SPRITERENDER] Source mask=512B active=3199 texelOffset=292040 stexelsByteOffset=29288 packed=1600B storage=2112B
[SPRITERENDER] Texel fnv1a=0c0a7acd expected=0c0a7acd
[SPRITERENDER] Frame resident after pack close heap8=28560 largest8=21492 used=2128B logicalStorage=2112B
[SPRITERENDER] DRAW sprite=172 origin=48,28 drawn=3199 paletteOffset=1616 framebufferFNV=001910a9
[VIDEO] Present 160x120 -> 320x240 exact 2x: 34450 us
[SPRITERENDER] Presented native sprite on shared 160x120 framebuffer -> CYD 320x240
[SPRITERENDER] Released frame heap8=30688 largest8=21492 deltaFromStart=0
[SPRITERENDER] READY real sprite rendered without shapeData or mediaTexels
[SPRITERENDER] READY native frame contract = source mask + packed texels + existing 16-color palette
```

Visual hardware validation confirms that the sprite silhouette, transparency,
masking and packed texel ordering are correct on the physical CYD. After palette
normalization the previously blue-shifted sprite is rendered in the expected
warm/orange palette family.

The deterministic framebuffer signature for this exact diagnostic scene is:

```text
framebufferFNV=001910a9
```

Keep this value as the reference for this particular sprite-172 diagnostic
composition unless the diagnostic background/layout intentionally changes.

## Heap recovery

The native sprite frame is temporary. After presenting the framebuffer, the
2,112-byte logical frame is released and memory returns exactly to the starting
layout:

```text
before       heap8=30688 largest8=21492
resident     heap8=28560 largest8=21492
released     heap8=30688 largest8=21492 deltaFromStart=0
```

The rendered pixels remain in the shared 160x120 framebuffer after the source
frame is released.

Heartbeat/menu state remain healthy after the render:

```text
[MENUBSP] READY menu.bsp plan + real runtime structures validated
[READY] Bring-up remains alive; touch still runs the SDL video test.
```

## Architectural conclusion

A complete original sprite can now travel through our ESP32-specific pipeline:

```text
DoomRPG-ESP32.pak
        |
        +--> bitshapes.bin source header + 512 B mask
        |
        +--> stexels.bin bounded 4-bpp payload (<= 1,600 B for menu)
        |
        +--> existing 16-color palette mapping
        |
        +--> native RGB565 palette normalization
        |
        v
EspNativeSpriteFrame (~2.1 KB worst-case validated frame)
        |
        v
ESP32-native rasterizer
        |
        v
shared 160x120 RGB565 framebuffer
        |
        v
exact 2x presentation to 320x240 CYD
```

Forbidden legacy graphics pools remain absent throughout this path:

```text
shapeData   = NULL
mediaTexels = NULL
```

This is the first hardware proof that the ESP32 engine can consume original Doom
RPG sprite resources and produce correct visible output using a bounded native
working set.

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
- exact selected sprite texel dataset measurement: 143,990 B
- exact largest selected sprite payload measurement: 1,600 B
- direct bounded sprite mask + packed-texel frame load
- explicit native palette RGB565 normalization
- first real native sprite rasterization into the shared framebuffer
- physical CYD presentation of that sprite
- deterministic diagnostic framebuffer hash `001910a9`
- exact frame and pack heap recovery

Still intentionally NOT executed / integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic `mediaTexels`
- native runtime sprite cache / eviction policy
- native wall-texture cache used by the real renderer
- replacement of the real `Render_renderSprite()` call path
- perspective/projected map sprite integration
- completion of map graphics loading
- game entities/player spawning
- main game loop

## Recommended next increment after merge

The next increment should reuse this proven native frame/rasterizer contract in a
more realistic renderer context rather than returning to map-wide resource
loading.

Good next targets are:

- factor the bounded sprite frame load into a reusable resource API rather than
  leaving it as a diagnostic-only consumer
- feed one real map sprite through projection/placement logic while still using
  the ESP32-native source frame
- only then measure actual repeated sprite access and choose a cache slot count
  / eviction policy from observed behaviour

Do not allocate the complete 143,990-byte selected sprite dataset. Do not
reintroduce the 55,676-byte expanded `shapeData` representation. Cache policy
must be justified by runtime access patterns.

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
