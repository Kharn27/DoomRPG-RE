# PR #243 — retire six legacy Render world pointers (CANDIDATE)

## Exact base and boundary

Base GitHub `main=fe3bf6cf005d0f87d534b51daf938c3473b6cdb6` (merged PR #241). Branch `agent/esp32-render-retire-legacy-world-pointers`. This is an independent native-layout retirement after the hardware PASS of #241; it does **not** alter immediate input dispatch, pickup pulses, Malwrath projectile cadence, world mutations, PAK, audio or `PlatformVideo_present()`.

Production `Render_t` no longer carries the six pointer-heavy desktop BSP/world mirrors `nodes`, `lines`, `mapSprites`, `tileEvents`, `mapByteCode` and `mapStringsIDs`. The desktop ABI and explicit `DOOMRPG_ESP32_BRINGUP_PROBES` declarations remain. Normal gameplay already reads immutable `EspMapRuntime`/compact native states and does not allocate those six mirrors. Map counters and native render counters are **not** retired here. The true live mappings `mediaTexelOffsets`, `mediaBitShapeOffsets`, `mediaTexturesIds`, `mediaSpriteIds`, palette, shared RGB565 framebuffer and all bounded world/visibility caches are unchanged.

Native process-root construction continues zeroing `Render_t`, without redundant writes to the absent members. Native START and first-frame boundary guards check the actual retained runtime ownership; bringup retains the historical pointer-null assertions. Normal release no longer attempts to free desktop world mirrors, but retains zeroing the legacy scalar counts and freeing actual mapping arrays; bringup release preserves the historical frees. Host tests exercise partial/full, shared framebuffer, 4096-cycle cleanup, and ABI/size source guards.

**Expected** 32-bit production layout: `Render_t 1332 -> 1308 B` (-24 B), `CORE READY used 2300 -> 2276 B` if all else remains fixed. Actual heap8 delta is allocator-dependent; no timing gain is claimed. `shapeData == mediaTexels == NULL` must remain true and no extra SD/ZIP path is authorized.

## Validation gate (not yet run on hardware)

1. Host C/Python tests and normal `esp32-cyd`, bringup and trace builds must succeed in CI. Verify new `_Static_assert(sizeof(Render_t) == 1308U)` and ensure no unguarded desktop pointer use remains; fix compile failures rather than weakening ownership.
2. Build/flash **normal `esp32-cyd`** from the exact agent branch; at boot capture `Engine structs: Render=1308 Game=0 Canvas=44 Total=2112`, `RENDERCORE bytes=1308`, and `CORE READY used=2276` as expected signatures. Record actual heap8/largest8 and PSRAM=0.
3. START intro/Entrance -> crate, pickup pulses, door, dialog -> EXIT. LOAD slot04 V11 -> door/Malwrath triple projectile, death/drop, pickup, secret -> EXIT. Keep responsive taps, exact map and first-frame fingerprints, two clean `RESIDENTRESET empty=1`, menu fingerprint `0f2c9fc3`, `shapeData/mediaTexels=NULL`, and no panic/WDT.
4. Only after hardware PASS: document observed values/log path, freeze code, close with **docs-only** commits and check CI. Merge is reserved to the user.

**Status: software candidate. No local firmware build or CYD test is claimed.** Any logged prior anomaly involving `NATIVEFRAME BOUNDARY-INLINE` or projectile x offsets must remain separate unless a controlled causal regression is established.
