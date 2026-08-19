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
    exact heap recovery (`agent/esp32-full-native-asset-pack`, PR #14, merge
    commit `fc2012b1a0b9e8e74058d9396ded8fceea68e168`).

## Current validated increment

Branch: `agent/esp32-native-bitshape-loader`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: determine and validate the ESP32-native bitshape representation for
the real 284 menu sprite references without materializing `bitshapes.bin` and
without inheriting the original renderer's large resident `shapeData` format.

## Important discovery: legacy shapeData does not fit

The first safe hardware attempt measured the exact legacy representation before
allocating it:

```text
[BITSHAPE] Begin refs=284 heap8=30688 largest8=21492
[BITSHAPE] Pack open heap8=26312 largest8=21492 cost=4376B entrySize=62273
[BITSHAPE] Preflight unique=112 shapeWords=27838 shapeData=55676B max=64x64 pitch=8B
[BITSHAPE] Allocation preflight heap8=26312 largest8=21492 fitAggregate=NO fitContiguous=NO
[BITSHAPE] REFUSED exact selected shapeData does not fit while pack is open
```

This refusal was intentional and did not crash the CYD. It proved that simply
rebuilding the reverse-engineered `shapeData` structure from streamed source
data would still preserve a PC/mobile-oriented memory model that cannot fit the
target.

The ESP32 design therefore changed on the same branch: `shapeData` is not part
of the native bitshape runtime model.

## Hardware-validated native bitshape source model

The second hardware run keeps the original mapping offsets as **source offsets**
into `bitshapes.bin` and reads only the current bitshape header/mask data from
`DoomRPG-ESP32.pak`.

Authoritative baseline:

```text
[BITSHAPE] Begin refs=284 heap8=30688 largest8=21492 shapeData=0x0
[BITSHAPE] Pack open heap8=26312 largest8=21492 cost=4376B entrySize=62273
```

Representative real sprite:

```text
[BITSHAPE] Sample sprite=0 sourceOffset=0 texelOffset=233472 bounds=1..63,44..63 mask=189B active=584 packed=292B refs=4
```

Complete real menu result:

```text
[BITSHAPE] Selected unique=112 refs=284 max=64x64 pitch=8B
[BITSHAPE] Legacy expanded shapeData=55676B (27838 words) -> ESP32 resident=0B
[BITSHAPE] Selected source masks=30813B, decoded one column at a time scratch<=32B
[BITSHAPE] Exact selected sprite texels=143990B packed across 284 refs activePixels=287848
[BITSHAPE] Source walk fnv1a=ed9c1179 mappings remain source offsets shapeData=0x0
```

The total 30,813 bytes of source masks and 143,990 bytes of selected packed
sprite texels are **dataset sizes**, not resident allocations. The validated
bitshape probe decodes one source column at a time with at most 32 bytes of mask
scratch.

## Heap recovery proof

After the complete 112-unique-shape / 284-reference walk, the pack closes back
to the exact starting heap and largest block:

```text
[BITSHAPE] Pack closed heap8=30688 largest8=21492 deltaFromStart=0
```

Final hardware result:

```text
[BITSHAPE] READY on-demand bitshape source model validated; legacy shapeData eliminated
[BITSHAPE] Renderer integration will consume source header/mask directly from bounded cache
[BITSHAPE] Render_loadTexels / monolithic mediaTexels still intentionally NOT executed
[MAPSTRUCT] Native on-demand bitshape model validated; texel loading remains blocked
[MENUBSP] READY menu.bsp plan + real runtime structures validated
```

Heartbeat remains stable afterwards:

```text
[ALIVE] uptime=5004 ms heap=96504 heap8=30688 largest8=21492 ... MENUBSP=ready ...
[ALIVE] uptime=10005 ms heap=96504 heap8=30688 largest8=21492 ... MENUBSP=ready ...
```

Touch and shared-framebuffer presentation also remain operational.

## Architectural conclusion

The original bitshape pipeline is now explicitly rejected on ESP32:

```text
bitshapes.bin (62,273 B)
        |
        v
whole-file inflate
        |
        v
shapeData for selected menu shapes (55,676 B resident)
```

The validated ESP32-native model is:

```text
mappings.bin source offsets
        |
        v
DoomRPG-ESP32.pak / bitshapes.bin
        |
        +--> read 12-byte shape header
        +--> read one mask column at a time (<= 32 B scratch)
        +--> derive bounds / active pixels / sprite texel range
        |
        v
future bounded renderer/cache consumer
```

`mediaBitShapeOffsets` therefore remain source-resource offsets on ESP32 rather
than being rewritten into offsets inside a resident `shapeData` array.

This is a deliberate platform architecture divergence from DoomRPG-RE. Game
behaviour and data compatibility remain the reference; the reverse-engineered
resident representation is not sacred.

## Current authoritative memory boundary

After real menu structures are resident and all current native resource probes
are closed:

```text
heap8=30,688 B
largest8=21,492 B
```

Temporary cost while the Arduino SD `File` for the pack is open:

```text
4,376 B
```

This temporary cost is fully recovered on close.

Important measured datasets that must **not** become monolithic RAM pools:

```text
legacy selected shapeData          = 55,676 B
selected source bitshape masks     = 30,813 B
selected packed sprite texels      = 143,990 B
wall-only legacy mediaTexels       = 172,032 B
```

These measurements strongly require bounded caches / on-demand consumers rather
than map-wide resident graphics pools.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real `Render_beginLoadMap(MAP_MENU)`
- real structural `Render_beginLoadMapData()` phase
- real nodes, lines, sprites, events, bytecodes and resource reference lists
- full 241-entry native asset pack validation
- direct random-access read of a real menu wall texture
- exact enumeration of the 112 unique bitshapes used by 284 menu sprite refs
- direct source-header and source-mask reads from `bitshapes.bin` inside the pack
- exact selected sprite texel requirement measurement
- 32-byte maximum mask-column scratch model
- zero resident `shapeData`
- exact heap recovery after complete native bitshape source walk

Still intentionally NOT executed / integrated:

- original `Render_loadBitShapes()`
- original `Render_loadTexels()`
- monolithic `shapeData`
- monolithic `mediaTexels`
- native sprite texel cache / source reader used by the real renderer
- native wall-texture cache used by the real renderer
- direct bitshape source consumption inside sprite rasterization
- completion of map graphics loading
- game entities/player spawning
- main game loop

## Recommended next increment after merge

Do **not** try another map-wide graphics allocation.

The next graphics increment should use the newly proven source model to build a
small reusable native sprite/bitshape consumer boundary. A good first target is
one real sprite: resolve its source bitshape metadata, its `stexels.bin` source
range, read only that bounded sprite payload, and validate the bytes/heap without
modifying the full renderer yet.

That measurement should establish the practical maximum single-sprite working
set before choosing cache slot count and eviction policy. Cache capacity must be
chosen from measured real access/size data, not guessed in advance.

Walls should follow the same philosophy already proven by the 2,048-byte wall
texture random-access test.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
- Keep `ESP32/README.md` updated with commands, SD preparation and tooling so
  operational knowledge is not left only in chat history.
