# Milestone — Native camera interpolation for quarter-turns

Status: **CODE CANDIDATE — production CI and real-CYD tests pending**

Branch `agent/esp32-camera-rotation-visual`, stacked temporarily on
`agent/esp32-dialogchain-owner-lifecycle` docs-only PASS closure
`ea2ded7995aa24858947b2b5c07c484069fcd4d9`.
Main at branch creation: `91e1d8412c98fc10ff7b6a2cff6c97323496ecaf`.
The dependency must be merged first; never merge main automatically.

Code candidate: `db0eb476d8070d4a560893718ae79e7570f72fcc`.

## Boundary

- Rotation only. MOVE and automap turn presentation remain byte-for-byte
  through the original settled gameplay compositor.
- A TURN commits once using the existing exact cardinal native dispatch.
  Synchronously paint up to two interpolated visual angles
  (1/3 and 2/3 of a quarter-turn, 256-angle circle).
  Always paint the final cardinal frame through the original route.
- The render-only pose is copied from the committed player view; no edit to
  `EspPlayerView`, movement state, event queues, turns, scripts, checkpoint,
  RNG, tiles, monsters or immutable runtime.
- The preview camera uses native wall/plane projection and native sprites
  with the same framebuffer, BSS compositor, PAK read leases and existing
  complete-frame presentation. No second framebuffer or animation heap owner.
- Preview BSP visibility is driven by a supplied camera pose; production
  `EspNativeBspVisibility_build` and sprite renderer retain their old API.
  Preview sprite admission uses the read-only visibility query rather than
  the wrapped gameplay activation observer, and skips
  `EspNativeBspVisibility_publishAutomap`. The final canonical render
  performs the usual activation and automap discovery only once.
- Preview paints do not publish `EspNativeGameplayActionEngine_markFreshFrame`.
  If an intermediate compose fails, record `[VIEWANIM] FALLBACK` and
  attempt the mandatory final cardinal frame. If that final frame fails,
  the original commit/rollback/fatal path remains unchanged.
- Non-cardinal camera poses are never passed to gameplay collision,
  monster turn producers, the HUD direction glyph or save data.
- The existing world compositor allocates bounded *transient* scratch
  per render, just as before. Animations multiply the number of presents
  and PAK leases; real device timing and heap margins must be measured
  before declaring performance acceptable.

## Hardware acceptance — normal `esp32-cyd`

1. Boot and enter MAP_INTRO. Verify untouched first-map frame,
   initial/menu FNV, `shapeData==NULL` and `mediaTexels==NULL`.
2. In world mode, TURN_RIGHT and TURN_LEFT at least twice each, including
   wrap 0<->192. Each turn should show two `[VIEWANIM] FRAME` events
   with `presented=1`, then `[VIEWANIM] END intermediates=2`
   and the unchanged `[RESIDENTGAMEPLAY] TURN ... committed=yes`.
   Record `totalUs`, visual appearance and perceived latency.
3. Confirm no extra `[MONSTERTURN] SCHEDULE` on rotation.
   With visible monsters, verify they do not move/attack on rotation alone.
4. A collision-free MOVE still advances the monster turn precisely once,
   rendering and facing-label behavior remain unchanged.
5. Exercise HUD, a chained dialogue, HUB, SYS EXIT and optional LOAD:
   no stuck PAK, memory growth or leaked visual camera; final MENU_MAIN
   fingerprint `522dc605`.
6. Test automap turn: previews must be skipped, original automap render
   retained. A failed preview should be optional and log FALLBACK;
   the cardinal final render must always succeed.

No hardware PASS is claimed until Serial logs and a human visual check
confirm both behavior and acceptable latency. This is **not** the MOVE
interpolation milestone. Any corrective code changes require a fresh
real-device check on the final code SHA.
