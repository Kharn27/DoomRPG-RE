# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R (classic Cheap Yellow
Display, no PSRAM) port. Update it after every hardware-validated increment.

## Target

- ESP32-2432S028R / classic ESP32 CYD, no PSRAM
- ESP32-D0WD-V3, 240 MHz, 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch on separate software SPI path
- microSD game data at `/DoomRPG.zip`
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
7. `ParticleSystem_startup()`, `MenuSystem_startup()` and
   `EntityDef_startup()` with packed indexed BMP storage
   (`agent/esp32-pre-render-startup`, PR #6).
8. Real `Render_startup()` using the platform-owned shared framebuffer, plus
   `sintable.bin` and `palettes.bin`
   (`agent/esp32-render-startup-shared-framebuffer`, PR #7, merge commit
   `72fde9307fc9de4c0a8bcbb63f3a73c5b7bf11bf`).

The real core graph costs 56,152 bytes of MALLOC_CAP_8BIT heap. `Game_t` is the
largest individual core allocation at 36,484 bytes.

Validated DoomCanvas geometry:

- clip: 160x120
- display: 160x120
- top HUD: 20 px
- bottom HUD: 20 px
- gameplay viewport: 160x80
- `Render_t` viewport: 160x80
- `Render_setup()` arrays: 1,280 bytes total

## Current validated increment

Branch: `agent/esp32-config-mappings-startup`

Status: **HARDWARE VALIDATED, READY TO MERGE**.

Objective: cross the startup boundary immediately after `Render_startup()` by
exercising the config path and loading the persistent render mappings, while
still stopping before any BSP/map data is opened.

Executed and validated in this increment:

1. `Game_loadConfig()`
2. preflight / inspect `/mappings.bin`
3. compute mapping allocation sizes before entering the original loader
4. execute the real `Render_loadMappings()`
5. validate all four persistent mapping tables
6. keep `Render_beginLoadMap()` and BSP loading blocked

No engine source file is patched by this increment; the probe calls the real
`Game_loadConfig()` and `Render_loadMappings()` implementations.

## Config path result

On the tested CYD there is no `Config` save file yet, which is a valid first-boot
state:

```text
[CONFIG] Config file present=no (missing is valid on first boot)
[CONFIG] -> Game_loadConfig()
loadConfig
loadConfig: (unable to open file)
[CONFIG] DONE heap delta=0 heap8=53388 largest8=36852
```

`Game_loadConfig()` therefore exits safely without persistent memory cost.

## mappings.bin preflight and allocation plan

The tested archive entry is:

```text
[MAPPINGS] mappings.bin c=2156 u=8392
```

The probe first loads the decompressed data only for header inspection and
calculates exactly what the original `Render_loadMappings()` will allocate:

```text
[ZIP] read mappings.bin method=8 c=2156 u=8392
[ZIP] inflate mappings.bin c=2156 u=8392 state=10992
[MAPPINGS] Header texelOffsets=592 bitShapeOffsets=1300 textures=152 sprites=252
[MAPPINGS] Plan payload=8376B largestAlloc=5200B whileData heap8=44980 largest8=28660
```

Persistent tables implied by the header:

- `mediaTexelOffsets`: 592 ints = 2,368 bytes
- `mediaBitShapeOffsets`: 1,300 ints = 5,200 bytes
- `mediaTexturesIds`: 152 shorts = 304 bytes
- `mediaSpriteIds`: 252 shorts = 504 bytes
- total payload: 8,376 bytes
- largest individual mapping allocation: 5,200 bytes

While the full 8,392-byte decompressed file is resident, the largest free block
is still 28,660 bytes, so the original loader's allocation sequence is safe on
this hardware.

## Final hardware mappings result

The real engine loader succeeds:

```text
[MAPPINGS] -> Render_loadMappings() heap8=53388 largest8=36852
---Render_loadMappings---
[ZIP] read mappings.bin method=8 c=2156 u=8392
[ZIP] inflate mappings.bin c=2156 u=8392 state=10992
[MAPPINGS] Render_loadMappings result=1 used=8440 heap8=44948 largest8=23540
[CONFIGMAP] READY config path exercised and mappings resident
[CONFIGMAP] mappingPayload=8376 heap8=44948 largest8=23540
[CONFIGMAP] Render_beginLoadMap / BSP still NOT executed
```

Measured persistent cost of this stage:

- config path: 0 bytes persistent on first boot
- mapping payload expected from header: 8,376 bytes
- measured mapping-stage heap delta: 8,440 bytes
- allocator / bookkeeping overhead above payload: 64 bytes
- remaining MALLOC_CAP_8BIT heap: 44,948 bytes
- largest contiguous block: 23,540 bytes

All four mapping pointers are non-null and the final texture/sprite counts match
the inspected header.

## Regression checks

The board remains alive after config + mappings startup. Heartbeat is stable:

```text
[ALIVE] ... heap8=44948 largest8=23540 ... CORE=ready LAYOUT=ready PRERENDER=ready RENDER=ready MAPPINGS=ready ...
```

Touch and the shared framebuffer diagnostic still work:

```text
[TOUCH] raw=2059,2300 pressure=1827 screen=180,117
[SDL] Sharing platform framebuffer: 38400 bytes
[VIDEO] Present 160x120 -> 320x240 exact 2x: 34441 us
[SDL] Touch-triggered shared-framebuffer test is on screen
```

This branch therefore passes its hardware merge gate.

## Shared Render framebuffer already validated

The ESP32 build wraps `Render_startup()` / `Render_free()` without modifying the
upstream `src/Render.c`. `Render_t::framebuffer` aliases the platform-owned
160x120 RGB565 framebuffer, while desktop `piDIB` stays absent.

Hardware proved framebuffer identity:

```text
platformFB=0x3ffc4224
[RENDER] Shared framebuffer 160x120 pitch=320 bytes=38400 ptr=0x3ffc4224
```

Render startup then loads:

- `sintable.bin`: 1,024 bytes temporary
- palette table: 3,280 entries / 6,560 bytes persistent

Post-render startup memory before this increment was approximately:

```text
heap8=53840
largest8=36852
```

## Packed indexed BMP storage already validated

The ESP32 image path keeps original indexed assets in native packed form:

- 1-bpp: 8 pixels per byte
- 4-bpp: 2 pixels per byte
- 8-bpp: 1 pixel per byte
- BMP row padding removed
- SDL texture adopts pixels and palette zero-copy
- palette index is unpacked only when sampled during `SDL_RenderCopy()`

This recovered exactly 52,000 bytes at the DoomCanvas layout boundary:

```text
before: heap8=34408 largest8=14324
after:  heap8=86408 largest8=47092
```

## Other memory work already validated

- correct generated resource ZIP: 241 entries
- miniz `tinfl_decompressor` state (10,992 bytes) moved from `loopTask` stack to heap
- indexed BMP 1/4/8-bpp decoder
- zero-copy ownership transfer from BMP surface to SDL texture
- native packed indexed texture storage
- desktop DoomCanvas 128-pixel minimum specialized to 120 only for ESP32
- desktop `mediaPlanes[24][64*64]` removed from ESP32 `Render_t`
- SDL, platform video and `Render_t` all share the same 38,400-byte logical framebuffer

## Current safe stop boundary

Validated and executed:

- core object graph
- `DoomCanvas_startup()` / `Hud_startup()` / `Render_setup()`
- `ParticleSystem_startup()`
- `MenuSystem_startup()`
- `EntityDef_startup()`
- `Render_startup()` through sintable, shared framebuffer and palettes
- `Game_loadConfig()` first-boot path
- `Render_loadMappings()`

Still intentionally NOT executed:

- `Render_beginLoadMap()`
- `menu.bsp` / gameplay BSP loading
- map runtime allocations
- main game loop

## Recommended next increment after merge

Start a new branch from the newly merged `main` and enter the first BSP boundary
piecewise rather than running the complete map loader blindly.

Recommended order:

1. inspect `/menu.bsp` archive compressed/uncompressed sizes
2. inspect the first fixed BSP header fields and the allocation counts they imply
3. measure peak heap while the decompressed BSP buffer is resident
4. call only the earliest safe `Render_beginLoadMap()` / map-data stage needed to
   validate those structures
5. stop before large texel/bitshape/sprite payloads if their allocation plan does
   not fit the current contiguous heap

The new starting budget is:

```text
heap8=44948
largest8=23540
```

This is now the authoritative hardware budget for the first BSP increment.

## Increment discipline

- Start each new increment from the latest hardware-validated `main`.
- One branch = one small testable objective.
- Fix failures on the same branch; do not create the next branch early.
- Hardware validation on the real CYD is the merge gate.
- Record measured hardware values here after every successful increment.
