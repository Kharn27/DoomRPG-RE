# PR #244 — bounded BSP sprite admission before immutable decode (CANDIDATE)

## Source of truth and separation

GitHub `main=fe3bf6cf005d0f87d534b51daf938c3473b6cdb6` (merged #241); branch `agent/esp32-sprite-bsp-early-admission`, new independent draft PR #244. The tested draft PR #243 remains unchanged and must not be silently merged or folded into this branch. PR #243 reduced `Render_t` by 24 B without modifying sprite rendering; its user reported a slight loss of subjective Doom-like responsiveness. That perception is a genuine **UX reservation**, not a demonstrated #243 timing regression.

The full `device-monitor-261031-201843.log` on #243 measured 344 map sprites, 11 hidden, 333 BSP leaf queries and 9–25 visible candidates; the first 16 instrumented `SPRITEORDER` passes took median 21.6 ms (source record portion ~18–21 ms). Separate presentation frames: MOVE preview median 104.651 ms (25 samples), ROTATE preview median 104.288 ms (20), pickup redraw median 108.607 ms (9), monster combat frame mean 119.980 ms (6). The first 16 scan samples are instrumented; they must not be treated as a representative overall frame rate. No paired #241 timing log is currently available for controlled A/B.

## Precise permanent boundary

`ESP32/src/esp_native_sprite_renderer.c::buildOrder()` first reads the mutable visual byte (including HIDDEN bit 0x80 -> info 0x10000) and runs exactly the original BSP leaf acceptance callback, `mapSpriteVisibleReadOnly()` for previews or `mapSpriteVisible()` for committed renders. A hidden sprite is still skipped before any BSP query, and an invalid leaf (`UINT32_MAX`) still fails closed. **Only when BSP-admitted** does production call `EspMapRuntime_getMapSprite()` and `EspMapSpriteTopology_getEntity()`, merge the original immutable info and visual mask, do resource checks, and insert into the same sorted `Order[64]` with the same sort depth, stats and FNV.

The `DOOMRPG_ESP32_TRACE_SPRITE` profile retains original per-record immutable/entity reads and original state/acceptance logging. No allocation, expanded sprite array, map-wide dynamic pointer graph, extra PAK/SD path, RNG call, gameplay step, projectile cadence, number of animation samples, `PlatformVideo_present()` or shared 38,400 B framebuffer change. Renderer/source compile is covered by normal/bringup/trace CI.

`ESP32/test/native_sprite_bsp_early_admission_host.py` extracts the **actual C function** from the source tree, builds an old-call-order reference from the same function (not a separately written algorithm) and checks normal/preview/trace paths with 344 synthetic sprites, hidden and BSP rejects, identical candidate order/sort/hash/stats, and fail-closed malformed leaf. For healthy frames, expected source/entity decode requests drop from 344 to the BSP candidate count; real duration/subjective improvement **must be confirmed by hardware**, not estimated from host calls.

## Gate and hardware itinerary

1. GitHub Actions host parity (normal+trace), original host suite, normal `esp32-cyd`, bringup, trace and audits pass before asking for CYD run. Build locally only if actually available; don't pretend a host build occurred otherwise.
2. On the classic CYD, clean upload firmware normal `esp32-cyd` from the exact head of this branch. Boot should reflect **main/#241 layout `Render=1332`**, not the unmerged #243 layout 1308. PSRAM=0 and `shapeData=mediaTexels=NULL` must remain. Explicitly avoid conflating #243 and #244.
3. Compare a same-route Entrance traversal with quick FORWARD/STRAFE/TURN inputs plus pickups and dialogues. Then LOAD V11 slot04: door113, Malwrath sprite322, three projectiles 204 per counterattack, kill/drop/pickup/secret and EXIT. Include `SPRITEORDER`, `SPRITEPHASE`, `VIEWANIM`, `PLAYERRES`, `MONSTERATKVIS`, `MALWRATHPROJECTILE`, `MONSTERRETAL`, heap8/largest8 and exact reset/fingerprint witnesses. Confirm image continuity and subjective fluidity.
4. Look for non-regression of monster activation, automap reveal, preview-only world mutation, native input immediacy, FNV, cache/PAK, two `RESIDENTRESET released=18008 empty=1` and two `SYSEXIT MENU-READY frame=0f2c9fc3`. Runtime no crash/watchdog. **Record subjective response separately** from numeric timings.
5. No hardware PASS, measured speedup or merge-ready declaration before actual Serial + user feedback. If software/hardware fails, correct code on this branch and rerun. After hardware PASS freeze code, docs-only closing commits, then user decides merges.

Status: **candidate, software CI pending, real CYD pending, PR draft**.
