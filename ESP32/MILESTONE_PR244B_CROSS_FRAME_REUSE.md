# PR #244B — one immutable frame decode for four legacy CROSS billboards (CANDIDATE)

## Baseline and A/B separation

Continuing **on the existing user-selected branch** `agent/esp32-sprite-bsp-early-admission`. `main` read from GitHub at start: `fe3bf6cf005d0f87d534b51daf938c3473b6cdb6`; prior hardware PASS code #244A `ea6e8fa12c376bd36fa099e6ebbaaa2de698aae1`, docs HEAD `4e64fe81b13da99fcaedacd814e2b09806bc0d5f` before B changes. **#244A remains the last hardware-confirmed code** (user log `device-monitor-261101-101936.log`), with `Render=1332`, no PSRAM, `shapeData/mediaTexels=NULL`. #243 is a distinct unmerged draft with `Render=1308`; its UX reservation and +24 B saved must not be confused with #244.

## Measured remaining hotspot and exact behavior

On #244A after admission, 16 early `SPRITEORDER.scanUs` passes fell to median ~3.08 ms (cold 16.67 ms), while `SPRITEPHASE.drawUs` ranges ~6–22 ms in the inspected samples; source I/O is separately visible in `SPRITEFLASH.flashReadUs`. `[NATIVESPRITE] CROSS index=...` appears repeatedly during Entrée and Malwrath action/render frames. Real `drawLegacyCross()` draws **four billboard copies** for one immutable source `logical in {82,84,86,88,90}` with offsets `(+16,0),(-16,0),(0,+16),(0,-16)`. Previously **every copy called `loadFrame()`**, repeating mappings, palette, bitshape mask and packed sprite-texel PAK range accesses for the same `resourceLogical` and `animation`. No cross-copy edits of `Frame` occur in `spans()`. The same ambient clock sample is used throughout one `renderImpl()`.

## Change B: bounded, permanent, fail-closed

`drawLegacyCross()` passes a *single stack `uint32_t crossAnimation` sentinel*, initially `UINT32_MAX`, into its four `drawAt()` calls. The first call performs the exact original `loadFrame()` into the **existing caller-owned `SpriteWorkspace.frame`**. Copies 2–4 recompute the same animation and **reuse the already decoded frame**, after checking `crossAnimation` matches and `frame->logical` is unchanged; mismatch fails closed. The pointer is only live during one four-copy draw call. Non-CROSS billboards and additive glow still pass `NULL` and perform their original loads. **No new persistent frame/cache/heap owner, no changed SD ZIP vs PAK source, no extra framebuffer, gameplay mutation or RNG, no changed offsets/masks/depth/culling or number of pixels intended.** Packed source/geometry remains immutable. Per-frame `frameLoads`, `packReads` and `frameBytes` represent physical frame decodes, so *these counters intentionally decrease for CROSS*. They are not historical semantic object counts; `draws` remains collapsed to one object.

`ESP32/test/native_cross_frame_reuse_host.py` extracts **production** `drawAt` and `drawLegacyCross` C bodies, reconstructs pre-change load order, and compiles the two paths for both normal and sprite-trace profile. It checks four calls in same order, identical deterministic raster input hash, exact object/pixel counters, **4→1** `loadFrame` and 28→7 PAK read mocks, first-load failures and animation-drift mismatch **fail-closed**. The raster stub is a witness, **not** literal hardware pixel parity. Full production normal/bringup/trace compilation and existing tests are required to accept a candidate.

## Validation gate

1. CI host tests, both C-equivalent frame reuse cases, compilation `esp32-cyd` normal/bringup/trace, all existing source guards (including sprite trace) green. No build success asserted until CI shows it.
2. User clean-builds **normal `esp32-cyd` on the same branch** and reflashes classic CYD. Boot must still say `Render=1332 Game=0 Canvas=44 Total=2136`, PSRAM zero, `shapeData/mediaTexels=NULL`.
3. First repeat START intro/Entrance; rotate/move through scenes including CROSS resources 81/83, crates, pickups and dialog; verify `SPRITEORDER`, `SPRITEPHASE.drawUs`, `SPRITEFLASH.flashReadUs`, `NATIVESPRITE CROSS ... copies=4`, stable frame FNV, correct hit/clip/near patterns and visual scene. Then LOAD slot04 V11 with Malwrath322, cross index324 resource83 and two three-shot projectile204 sequences; drop/pickup/secret and EXIT. **Compare identical scenes and warmed-cache conditions**, not only globally aggregated samples.
4. Check save restoration, monster activation, door animations, preview read-only, HUD, no new unsupported/fail-closed error, RNG/heap invariants, two exact menu FNV `0f2c9fc3` and two `RESIDENTRESET released=18008 empty=1`; note subjectively whether frames are smoother. No audio or all-map coverage implied.
5. Only after hardware PASS document the results, freeze B code, docs-only closing commits and mark PR #244 merge-ready again. **User merges; agent never merges main.**

## Software gate — PASS (candidate for CYD)

**GitHub Actions [#41302501979](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41302501979): SUCCESS 78/78 steps** on source+tests HEAD `e211246de7d76d3b47d83508a8588d9687d22cb7`. The new actual-C dual-path test passed in **normal and sprite-trace**: deterministic draw witness FNV `93af917d`, four physically drawn copies collapsed to one logical draw, `frameLoads 4→1`, and harness `packReads 28→7` (mock values, **not** a measurement of physical PAK traffic). Both initial-load failure and synthetic mid-group animation drift stayed fail-closed. Previous sprite BSP admission host parity, native gameplay tests, normal `esp32-cyd`, bringup and trace firmware builds, and all source audits also passed. The CI job uploaded compiled firmware artifacts; no claim of a local build or CYD test.

**Change audit:** from A docs HEAD `4e64fe81b13da99fcaedacd814e2b09806bc0d5f` to B code+test HEAD, only four paths differ: `ESP32/src/esp_native_sprite_renderer.c` (+24/−8), new `ESP32/test/native_cross_frame_reuse_host.py`, workflow CI registration, and this milestone. No `PlatformVideo_present()`, gameplay or input file changes.

**Status: #244B SOFTWARE PASS, real CYD hardware NOT tested, PR remains DRAFT**. Prior A PASS remains valid only for the old code. Do not label B merge-ready until fresh user Serial and visual feedback.
