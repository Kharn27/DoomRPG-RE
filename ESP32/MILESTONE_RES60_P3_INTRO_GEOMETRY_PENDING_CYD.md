# RES60-P3 — MAP_INTRO geometry at 192x96 (corrected CYD Serial PASS; real walls user-visible)

- Issue: [#263](https://github.com/Kharn27/DoomRPG-RE/issues/263)
- Draft parallel PR: [#364](https://github.com/Kharn27/DoomRPG-RE/pull/364)
- Branch: `agent/esp32-res60-p1` (not merged into main).
- Last rebased integration base: `fe85909babd686bb2041cc74dadf69c514edb519`; main can evolve independently. P1/P2 were verified on physical CYD previously. Corrected P3 now has new physical SD/PAK/geometry/LCD Serial evidence and user-confirmed visible walls (below).
- Status: original P3 [code CI SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44427139506) but **19 Nov real classic-CYD visual FAIL (center uniformly gray)** despite source/PAK/arena/scanout Serial PASS. Root cause: cardinal angle 64 and 192 ray directions reversed vs native Doom RPG camera. **P3 correction + adversarial regression tests: code `21e1859317904e80e4d2d1b7407927462b4b1508`, [CI SUCCESS 216 steps](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44431202764)**. Corrected firmware was **reflashed and tested on a real CYD-inverted panel** with the generic `esp32-cyd-res60` (classic-color) profile; real wall silhouettes visible per user, but photo and calibrated palette proof not supplied.

## Boundary: real BSP geometry, not resampled 50% pixels

`esp32-cyd-res60` additionally defines `DOOMRPG_RES60_P3_WORLD=1`, which is rejected at preprocessing unless P1's VIDEO-only flag AND P2's 192x144 layout flag are both enabled. Normal and inverted profiles continue using the **original 160x120 game framebuffer, 160x80 viewport, 20+20 HUD and 2x presentation**. P3 C source is preprocessor-empty in those release profiles.

The P3 boot now mounts the CYD's real microSD (only for this isolated branch/env) and uses **`/DoomRPG-ESP32.pak`**, never runtime ZIP, to:
1. Inventory the original `/intro.bsp` with `EspBspReader_inventoryPackEntry()` and its bounded 256-byte source window; require validated original CRC, supported cardinal header spawn index/direction and bounded original line count (1..4096).
2. Load the actual compact immutable `EspMapRuntime` arena, preserving source CRC, counts, arena FNV, and allocation-free decoders; **no legacy entity/monster/world mutation**.
3. Compute the original spawn pose from source BSP header: 32x32 64-unit tiles, tile center offset 32; then the legacy first-frame **16-unit camera nudge** based on cardinal 0/64/128/192. Unsupported orientations fail closed (no synthetic fallback).
4. Read real `EspMapRuntime_getLine()` values. A bounded first pass validates each immutable line before touching the output framebuffer; the second pass raycasts the actual world-coordinate segments at **each of 192 native columns**, using signed 64-bit 2D segment intersection and a nearest-hit depth test. Original 64-unit wall height and 36-unit camera eye give perspective spans on **96 physical logical world rows y=24..119**. Top and bottom 24-row HUD *regions* remain P2-style calibration strips.
5. Flat-shade each visible original logical BSP line according to its logical texture ID and distance (not actual Doom RPG wall texels). Floor/ceiling colors are converted directly from the BSP inventory RGB header.
6. Release the immutable map arena, require the PAK lease closed, then present the one **192x144 RGB565 55,296-byte framebuffer** on the 320x240 LCD using the previously hardware-validated **640-byte scanline** and fractional nearest mapping. One frame only; idle loop does not redraw.

No secondary full-screen buffer, mapwide texel payload, scene array, allocation-heavy legacy `Render_t`, runtime ZIP, PSRAM, entity mutation, world sprite, font/HUD art, sound, gameplay action or touch is started. The renderer is a **P3 real-source flat wall geometry preview** rather than the final correct Doom RPG 3D renderer. Its raycasts use all admitted source line segments rather than original BSP render order; two-sided line/door/portal semantics, wall texture sampling, actual floor/ceiling texturing, sprites, shadows, weapon/HUD, combat and animation are intentionally *not* implemented. The inherited native full textured renderer still has strict 160x80 contracts; those must be coherently migrated before the 60% game is playable. The 192-column raycaster is diagnostic, not a performance target; *no fixed-FPS loop should be introduced*.

## Host / CI proof requirements

`python ESP32/test/res60_p3_world_host.py` compiles P3 projective code with strict C11 and checks:
- Exact 192x96 world area and both 24-pixel HUD bands, 55,296B single buffer, 50% contracts untouched.
- Four cardinal orientations in a closed 4-line test map; source-coordinate raycast rather than a 160-pixel rendered image.
- Invalid direction, view height, empty/oversize map, corrupt line/reader failure before painting **without touching framebuffer**.
- Full 192 visible columns for the closed test room, correct source reader counts, near/far perspective and stable deterministic FNV for identical geometry.
- Two guard sentinels outside the framebuffer and rejection of invalid admission P3=1 without P1/P2.
- A build of production-profile P3 source with `DOOMRPG_RES60_P3_WORLD=0` and `nm` verification that the world renderer is **absent** (not linked into production).
- All previous P1, P2, source-parity and PlatformIO firmware checks ran in CI. The original source candidate `2c556d8…` CI passed but **did not catch the direction bug** due to its symmetric four-wall host test. The corrected candidate `21e1859317904e80e4d2d1b7407927462b4b1508` [CI SUCCESS 216 steps](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44431202764) includes four asymmetric front-versus-back wall oracles for all cardinal angles and debug metrics for full-wall coverage. Hardware revalidation remains mandatory.

## Corrected CYD P3 test procedure (performed; no new photo attached)

From `ESP32/`:

```sh
pio run -e esp32-cyd-res60 -t upload
pio device monitor -b 115200
```

Required SD file: `/DoomRPG-ESP32.pak` (the same native backing store used for normal gameplay). P3 deliberately starts SD, BSP inventory and compact map runtime, unlike the P2 VIDEO-only pattern. It **does not start the engine/game, and no audio is expected**.

Expected markers (real numeric counts, RAM and FNV MUST come from the CYD — never invent them):

```text
[RES60P3] BOOT mode=INTRO_GEOMETRY_ONLY target=192x96@y24 source=/intro.bsp ...
[SD] Mounted ...
[BSPREAD] HEADER ...  (real original spawn index/direction)
[MAPRT] READY ...      (real immutable arena footprint/FNV)
[RES60P3] MAP source=PAK map=/intro.bsp ... spawn=... angle=... camera=...
[RES60P3] RASTER source=MAP_INTRO width=192 world=96 hudTop=24 hudBottom=24 lines=... rayTests=... hitColumns=... fnv=... render=FLAT_WALLS textureSamples=0 sprites=0 engine=NOT_STARTED
[RES60P1] PRESENT src=192x144 dst=320x240 method=floor-nearest lineBytes=640 ...
[RES60P3] FRAME ... source=PAK mapRuntime=RELEASED audio=NOT_STARTED legacyWorld=NOT_STARTED
[RES60P1] IDLE ...
```

A framebuffer photo should show **actual flat-shaded Intro map wall silhouettes in perspective**, plus colored calibration bands, rather than P2's checker/grid. Existing classic display inversion may complement authored colors; this is a separate issue, already documented in P2 and must not be "fixed" by altering production's panel profile.

**Gate:** no CI/code test can certify on-device PAK read, compact map memory, projected geometry accuracy, player pose or audio headroom. If PAK is missing, source fails validation or no world columns intersect, P3 must fail closed rather than show a fake image. The P3 log and photograph are required before documenting hardware PASS or advancing to textured BSP.

## Hardware failure evidence and bounded correction — 19 November 2026

**Real CYD first P3 attempt: visual FAIL**, not an incomplete boot. User supplied `platformio-device-monitor-261119-013723.log`, **3,469 bytes, 44 lines, SHA256 `5bd7d07890562b1e3a89a84123614e2d7c32aad9247bdabf7edd2d53f13ac5a9`**, and explicitly described the LCD as the P2 upper/lower colored bands with a **uniform gray central viewport**. The Serial capture has no embedded Git SHA or upload transcript: do not assert a cryptographically established flashed commit. Actual classic ESP32-D0WD-V3, 240 MHz, 4 MiB flash, **0 PSRAM**.

```text
Heap8: 275892 free, largest block 114676
[RES60P1] BOOT mode=VIDEO_ONLY logical=192x144 rgb565=55296 viewport3d=NOT_STARTED HUD=NOT_STARTED MENU=NOT_STARTED AUDIO=NOT_STARTED heap8=220324 largest8=110580 minHeap8=214888
[SD] Mounted, size=240 MiB
[BSPREAD] ENTRY /intro.bsp offset=1945016 size=21823 crc32=623f34e4 window=256B
[BSPREAD] HEADER name='Entrance' loadMapId=1 spawn=904 dir=64 camera=648 floorTex=145 ceilingTex=112
[BSPREAD] STREAM bytes=21823/21823 readCalls=86 window=256B fnv1a=d5cc751f crc32=623f34e4 verified=yes
[MAPRT] READY arenaBytes=14095 populateReadCalls=33 arenaFNV=c3882516 strings=94 sourceCRC=623f34e4
[RES60P3] MAP source=PAK map=/intro.bsp bytes=21823 crc=623f34e4 arena=14095 arenaFNV=c3882516 nodes=223 lines=480 spawn=544,1824 angle=64 camera=544,1840 heapOwner=compact-only
[RES60P3] RASTER source=MAP_INTRO width=192 world=96 hudTop=24 hudBottom=24 lines=480 rayTests=92160 intersections=320 hitColumns=192 maxDepth=48 fnv=243203c5 us=394556 render=FLAT_WALLS textureSamples=0 sprites=0 engine=NOT_STARTED
[RES60P1] PRESENT src=192x144 dst=320x240 method=floor-nearest lineBytes=640 us=36898
[RES60P3] FRAME totalUs=442278 heap8=192512 largest8=110580 minHeap8=168656 source=PAK mapRuntime=RELEASED audio=NOT_STARTED legacyWorld=NOT_STARTED
[RES60P1] IDLE mode=VIDEO_ONLY heap8=192512 largest8=110580 minHeap8=168656 audio=NOT_STARTED map=NOT_LOADED
```

**What passed on the actual CYD:** native PAK SD read, BSP CRC, compact immutable 14,095-byte arena and exact arena FNV, original line decoding, one 55,296-byte 192×144 framebuffer, 640-byte nearest presenter line and orderly map-runtime release/idle. Heap8 changed 275,892 → 220,324 B at video allocation, → **192,512 B** after SD and released map; largest free 8-bit block **110,580 B**, minimum during load/raycast **168,656 B**. Native 192-column 480-line brute-force raycast **394,556 µs** + LCD **36,898 µs**; complete instrumented frame **442,278 µs**. These are P3 one-shot diagnostic costs, **not real textured gameplay FPS or audio headroom**. No panic/WDT observed in captured reset-to-idle log.

**Root cause established by code comparison:** P3 `rayForColumn()` had `angle=64 → rayY=+192` and `angle=192 → rayY=-192`, but native `EspNativeFirstFrame.renderFrame()` sets `viewCos_=cos`, `viewSin_=-sin`, so forward depth = `dx·cos − dy·sin`. Accordingly, `angle=64` faces **negative Y**; the old raycaster looked behind the player. At `maxDepth=48` and `hitColumns=192`, 64-unit wall height and eye at 36 give `top=48−(28×192)/(2×48)=−8`, `wallH=64×192/(2×48)=128`; a wall at **48 units or closer fills every row of the 96px viewport**. This rigorously explains the gray rectangle and why the symmetric square host test falsely passed.

**Correction code SHA** `21e1859317904e80e4d2d1b7407927462b4b1508`: `angle 64 → rayY=-192`, `angle 192 → rayY=+192`, angle 0 (+X) and 128 (−X) unchanged. New asymmetric single-wall C11 tests check **each of the four directions** with visible wall ahead and invisible wall behind (8 directed cases), in addition to prior pixel-canary/FNV/profiles tests. Added bounded statistics `minDepth`, `centerDepth`, `centerLineIndex`, `fullHeightColumns`, `wallPixels` to the Serial raster line for next real diagnosis. No new framebuffer, asset handling, shapeData/mediaTexels or production engine changes. [GitHub Actions 44431202764](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44431202764) **SUCCESS 216 steps** on exactly this code commit, including production/inverted and RES60 firmware builds; **no real CYD second test yet**.

**Historical first-failure retest contract:** reuse `esp32-cyd-res60` and native PAK; first gray screen had `maxDepth=48`. This contract was satisfied by the subsequent user-confirmed visible walls and fresh Serial, with the new diagnostic fields. Photo/color calibration and playable renderer remain outside P3. PR #364 stays DRAFT, parallel to main.

## Corrected P3 real CYD result — 19 November 2026: PASS read/geometry/presentation; 60% game still NOT STARTED

The user reported **"Oui je vois des mur"** after successfully compiling and uploading the corrected `esp32-cyd-res60` and supplied the full upload + reboot/Serial text in the conversation (no distinct attached file or firmware-embedded Git SHA). Hardware was a **CYD-inverted panel** intentionally flashed with the **classic** `esp32-cyd-res60` display settings; the log confirms `classic inversion=on`. The user described the colors as poor, as expected from mismatched panel profile. **No new photograph or colorimetric measurement was supplied**. Treat user-reported wall visibility as bounded qualitative visual PASS, not verified final artwork/colors.

```text
Chip: ESP32-D0WD-V3; CPU=240 MHz; Flash=4194304 bytes; PSRAM=0
Heap8: 275892 free, largest block 114676
[VIDEO] RES60-P1 VIDEO-ONLY framebuffer 192x144 RGB565, 55296 bytes
[RES60P1] BOOT ... heap8=220324 largest8=110580 minHeap8=214888
[RES60P3] BOOT mode=INTRO_GEOMETRY_ONLY target=192x96@y24 source=/intro.bsp backing=PAK ...
[SD] Mounted, size=240 MiB
[BSPREAD] ENTRY /intro.bsp offset=1945016 size=21823 crc32=623f34e4 window=256B
[BSPREAD] HEADER name='Entrance' loadMapId=1 spawn=904 dir=64 camera=648 floorTex=145 ceilingTex=112
[BSPREAD] STREAM bytes=21823/21823 readCalls=86 window=256B fnv1a=d5cc751f crc32=623f34e4 verified=yes
[MAPRT] READY arenaBytes=14095 populateReadCalls=33 arenaFNV=c3882516 strings=94 sourceCRC=623f34e4
[RES60P3] MAP source=PAK map=/intro.bsp bytes=21823 crc=623f34e4 arena=14095 arenaFNV=c3882516 nodes=223 lines=480 spawn=544,1824 angle=64 camera=544,1840 heapOwner=compact-only
[RES60P3] RASTER source=MAP_INTRO width=192 world=96 hudTop=24 hudBottom=24 lines=480 rayTests=92160 intersections=1663 hitColumns=192 minDepth=96 maxDepth=240 centerDepth=240 centerLine=255 fullHeightColumns=0 wallPixels=6349 fnv=ed9b68c4 us=393074 render=FLAT_WALLS textureSamples=0 sprites=0 engine=NOT_STARTED
[RES60P1] PRESENT src=192x144 dst=320x240 method=floor-nearest lineBytes=640 us=36925
[RES60P3] FRAME totalUs=451177 heap8=192512 largest8=110580 minHeap8=168616 source=PAK mapRuntime=RELEASED audio=NOT_STARTED legacyWorld=NOT_STARTED
[RES60P1] IDLE mode=VIDEO_ONLY heap8=192512 largest8=110580 minHeap8=168616 audio=NOT_STARTED map=NOT_LOADED
```

Compare with the earlier failing P3: `hitColumns=192` unchanged, but `minDepth=96` and `maxDepth=240` instead of all depths ≤48, `intersections=1663` instead of 320, `fullHeightColumns=0`, and **6,349 true wall pixels** (of 18,432 native world pixels). The center wall is at distance 240, source line 255; raster changed `fnv=243203c5` → **`ed9b68c4`**. This backs the corrected cardinal sign with real geometry. P3 raster took **393,074 µs**, LCD **36,925 µs**, total instrumented frame **451,177 µs**; the difference is additional setup/reporting, not gameplay rendering. Heap8 minimum **168,616 B** during PAK/arena work and **192,512 B** at idle, no panic/WDT visible up to IDLE.

**Verdict:** P3 source PAK/CRC + immutable arena + one-shot genuine **192×96 flat-wall** projection + nearest LCD presentation **CYD Serial PASS**, and **user verbally confirms visible original walls** after correction. The 60%-playable renderer is **not built**: original raycaster cost ~393 ms for 92,160 line tests, no textured BSP visibility traversal, sprite/planes/HUD/input/audio residency, no gameplay budget claim. The lack of photo means no exact image composition/color PASS. Display palette issue is tied to **testing a CYD-inverted screen using classic panel mode**, not proof of a rendering bug; an eventual 60% inverted env must mirror production inverted screen settings without changing pixel geometry.

## Scope of eventual full 60% integration (19 November audit)

The pilot currently **deliberately bypasses** the production 160x120 game renderer; switching the framebuffer constant alone cannot enable gameplay. Source audit on this branch identified the renderers with 160x80 guards: `esp_native_bsp_visibility.c`, `esp_native_plane_renderer.c`, `esp_native_sprite_renderer.c`, `esp_native_gameplay_frame.c`, and output geometry ownership in `esp_native_first_frame.c`. User-interface ownership also embeds 160x120/20px layout: `esp_native_gameplay_hud.c`, `esp_native_gameplay_projection_overlay.c`, `native_main_menu_touch_layout.c`, `esp_native_gameplay_input.c`, and HUB touch UI. Rework these **bounded surface/coordinate owners** one coherent submodule at a time behind an explicit dimensioned profile. Do not blindly replace constants globally, do not break production 50%-size tests, and do not rewrite unrelated map formats, scripts, save logic or asset decode. Real 192x144 RGB565 uses **55,296 B vs 38,400 B at 50%** (+16,896 B / 44% pixels); world 192x96 vs 160x80 is likewise +44% pixels. 5/3 nearest LCD presentation is not an integer 2× clone. Budget RAM and redraw separately from the diagnostic 393ms all-lines raycaster before declaring gameplay feasible.
