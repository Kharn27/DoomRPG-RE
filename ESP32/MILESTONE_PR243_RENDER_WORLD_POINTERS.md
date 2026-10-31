# PR #243 — retire six legacy Render world pointers (CANDIDATE)

## Exact base and boundary

Base GitHub `main=fe3bf6cf005d0f87d534b51daf938c3473b6cdb6` (merged PR #241). Branch `agent/esp32-render-retire-legacy-world-pointers`. This is an independent native-layout retirement after the hardware PASS of #241; it does **not** alter immediate input dispatch, pickup pulses, Malwrath projectile cadence, world mutations, PAK, audio or `PlatformVideo_present()`.

Production `Render_t` no longer carries the six pointer-heavy desktop BSP/world mirrors `nodes`, `lines`, `mapSprites`, `tileEvents`, `mapByteCode` and `mapStringsIDs`. The desktop ABI and explicit `DOOMRPG_ESP32_BRINGUP_PROBES` declarations remain. Normal gameplay already reads immutable `EspMapRuntime`/compact native states and does not allocate those six mirrors. Map counters and native render counters are **not** retired here. The true live mappings `mediaTexelOffsets`, `mediaBitShapeOffsets`, `mediaTexturesIds`, `mediaSpriteIds`, palette, shared RGB565 framebuffer and all bounded world/visibility caches are unchanged.

Native process-root construction continues zeroing `Render_t`, without redundant writes to the absent members. Native START and first-frame boundary guards check the actual retained runtime ownership; bringup retains the historical pointer-null assertions. Normal release no longer attempts to free desktop world mirrors, but retains zeroing the legacy scalar counts and freeing actual mapping arrays; bringup release preserves the historical frees. Host tests exercise partial/full, shared framebuffer, 4096-cycle cleanup, and ABI/size source guards.

**Expected** 32-bit production layout: `Render_t 1332 -> 1308 B` (-24 B), `CORE READY used 2300 -> 2276 B` if all else remains fixed. Actual heap8 delta is allocator-dependent; no timing gain is claimed. `shapeData == mediaTexels == NULL` must remain true and no extra SD/ZIP path is authorized.

## Hardware run — 31 October 2026, normal esp32-cyd

**Functional/memory PASS on the tested routes, subjective rhythm reservation remains open.** Evidence: `device-monitor-261031-201843.log` (1373 lines), user-tested classic CYD firmware corresponding to the candidate layout. Boot verifies `Render=1308 Game=0 Canvas=44 Total=2112`, `[RENDERCORE] ... bytes=1308`, `[CORE] READY ... used=2276`; 4 MB flash and PSRAM=0. Native mapping payload remains 8376 B (textures=152, sprites=252). `shapeData/mediaTexels=NULL` throughout.

Compared with the prior #241 documented boot values: menu heap8 153160 -> **153184** (+24 B free); START cache prime 112880 -> **112904** (+24 B); LOAD slot04 prime 112068 -> **112092** (+24 B); EXIT menu 158004 -> **158028** (+24 B). Largest block unchanged: 86004 gameplay / 110580 menu. This is observed allocator headroom, not evidence of a frame-time improvement.

START /intro.bsp Entrance: native arena=14095, FNV=c3882516, first frame=71ca7465, crates, six pickups, doors/dialog events 88/89/79, EXIT. LOAD slot04 V11 (5604 B): same map arena/FNV, monster state beaeaccc, topology 50e16fe3 and position e2a9d90a, door113, Malwrath322 hits 11->7->3->0, two triple-projectile204 responses cleared before their retaliation commits, death/drop, pickup/secret, EXIT. Two `[RESIDENTRESET] released=18008 empty=1`; two `[SYSEXIT] MENU-READY frame=0f2c9fc3`; `HEADROOM_OK`; no panic/WDT observed. White 500ms pickup blink remains enabled; movement tile738->706 begins before `VIEWFLASH EXPIRE`, so the visual flash does not impose a 500ms input block.

**UX reserve, explicitly reported by the tester:** gameplay feels *slightly less nervous* and less close to traditional Doom than it did immediately after PR #241. Quantitative inspection of this trace (not an A/B timing comparison): 25 `VIEWANIM FRAME mode=move` median ~104.7ms, 20 rotation frames median ~104.3ms each (two frames per 90deg turn), nine `PLAYERRES FRAME` median ~108.6ms, six `MONSTERCOMBAT FRAME` mean ~120.0ms, and 16 `SPRITEORDER` samples median ~21.6ms scanning 344 map sprites with 9–25 candidates. Sprite order scans are an existing hot path, not changed by #243. Full `#243` diff confirms no gameplay animation, input, renderer, combat or presentation changes; no causal performance regression is established. The older #241 log's comparable per-action timings are not available here for controlled statistics.

**Disposition:** freeze tested code. Hardware behavior and ownership validation passed, but do **not** declare performance/UX PASS or merge-ready until the subjective reservation is addressed or an equivalent route A/B comparison rules out regression. Investigate bounded native sprite-order admission as a separate performance milestone; avoid editing `PlatformVideo_present()`, gameplay cadence or the already-proven #241 immediate touch dispatch as a guess.

## Original pre-hardware gate (historical candidate plan)

## Validation gate (not yet run on hardware)

1. Host C/Python tests and normal `esp32-cyd`, bringup and trace builds must succeed in CI. Verify new `_Static_assert(sizeof(Render_t) == 1308U)` and ensure no unguarded desktop pointer use remains; fix compile failures rather than weakening ownership.
2. Build/flash **normal `esp32-cyd`** from the exact agent branch; at boot capture `Engine structs: Render=1308 Game=0 Canvas=44 Total=2112`, `RENDERCORE bytes=1308`, and `CORE READY used=2276` as expected signatures. Record actual heap8/largest8 and PSRAM=0.
3. START intro/Entrance -> crate, pickup pulses, door, dialog -> EXIT. LOAD slot04 V11 -> door/Malwrath triple projectile, death/drop, pickup, secret -> EXIT. Keep responsive taps, exact map and first-frame fingerprints, two clean `RESIDENTRESET empty=1`, menu fingerprint `0f2c9fc3`, `shapeData/mediaTexels=NULL`, and no panic/WDT.
4. Only after hardware PASS: document observed values/log path, freeze code, close with **docs-only** commits and check CI. Merge is reserved to the user.

**Status: software candidate. No local firmware build or CYD test is claimed.** Any logged prior anomaly involving `NATIVEFRAME BOUNDARY-INLINE` or projectile x offsets must remain separate unless a controlled causal regression is established.
