# Milestone — Native camera interpolation for movement

Status: **REAL-CYD HARDWARE VISUAL PASS — bounded behavior**

Branch: `agent/esp32-camera-rotation-visual`.
Hardware-tested final code SHA:
`e944dc386758900bc55c68d374344c631640e6b4`.
CI normal production `esp32-cyd` at exact code #37705217411:
SUCCESS, RAM static 45056 B, flash 774521 B.
Follow-on docs-only CI #37705286981: SUCCESS.
Prior validated rotation code SHA:
`db0eb476d8070d4a560893718ae79e7570f72fcc`.
Base main after PR #200: `f2a178e1fa0d04e0e163215bef6d745b7ea64da5`.

## Hardware witness for axial midpoint retune (2026-10-08)

User physically tested normal firmware on classic CYD and described
the adjusted axial motion as **"C'est pas mal du tout là !"**;
the prior two-preview forward/back pacing had been perceived as
two distinct steps / a bit slow.

- Eight FORWARD/BACK movements (sequences 1,2,8,9,10,11,12,13)
  each produced exactly **one** `profile=axial-midpoint`
  `FRAME step=1/1 sample=1/2 ... presented=1`, followed by
  `END intermediates=1` and a committed canonical MOVE.
  Samples 132213 us (first render) and 103135..106514 us
  on subsequent axial previews.
- STRAFE_RIGHT and STRAFE_LEFT (sequences 4 and 5) each
  produced **two** `profile=strafe-thirds` previews with
  `sample=1/3` and `sample=2/3`, `intermediates=2`;
  combined preview times 204244 and 204366 us.
- TURN_RIGHT and two TURN_LEFT actions produced two previews
  apiece, all presented. No rotation-induced monster turn.
- Ten `MOVE committed=yes` actions (sequences 1,2,4,5,
  8,9,10,11,12,13) each had exactly one corresponding
  `MONSTERTURN ORDERED-DISPATCH reason=MOVE`, tokens 1..10.
  Active monsters = 0; this verifies cadence, not active AI.
- On forward moves into tiles 839 and 838, two native Armor
  Shard pickups committed once each, armor 0->4->8,
  with expected feedback and no duplicate pickups.
- `[NATIVEFRAME] LEGACY_GUARD / RETRY / RECOVERED` occurred
  on a settled turn frame and recovered correctly.
- ALIVE remained `heap=184212 heap8=118288 largest8=86004`
  at ~19.6, 24.9, 29.9, 34.9, 39.9, 44.9, 49.9 s.
  No preview FALLBACK or render failures in this witness.

The final presented pose is exact and gameplay-visible once;
midpoint and thirds are local render-only camera copies.
No new framebuffer, persistent heap owner, RNG, checkpoint
mutation, automap publication or monster activation in previews.
The previously physically tested ROTATE path is unchanged.

**Limits:** the latest-SHA excerpt does not cover a fresh
MOVE-BLOCKED attempt, visible monsters, automap mode, doors,
save/LOAD, dialogue, SYS EXIT or final MENU_MAIN fingerprint.
Those stay regression tests and are not claimed proven by
this particular firmware excerpt.

## Historical two-preview axial experiment and correction

## Source of truth: real CYD observations before retune

With the first two-preview candidate, physical logs showed these
successful animations:
- FORWARD 838->839, 839->840, 840->808 (two preview paints each,
  ~205–210ms combined preview time);
- BACK 808->840 and 840->872 (~208–210ms preview time);
- STRAFE_RIGHT 840->872 and STRAFE_LEFT 872->840 (repeated,
  ~205ms preview time);
- STRAFE_RIGHT blocked at tile 838->870 with MOVE-BLOCKED
  and no preview.
- Each successful MOVE advanced exactly one regular MONSTERTURN
  token (11 through 19 in the provided excerpt), never on previews.
- ROTATE continued to render two previews with the previously
  hardware-validated behavior, without a monster turn.
- ALIVE showed heap8=117252, largest8=86004 and heap=183176
  consistently at multiple 5-second samples.

The user judged crab/strafe motion enjoyable, but FORWARD/BACK looked
like moving twice and perhaps too slow. That is UX feedback, not an
engine failure or a memory leak. The 2-preview axial presentation should
**not** be described as a successful final UX result. The source of
the pacing overhead is observable: two intermediate 100ms-class renders
plus an additional, required final canonical world render.

## Accepted final behavior: directional visual sampling

Successful WORLD-mode MOVE still performs **exactly one** collision
preflight, one event/transaction commit and a normal canonical final frame.
The visual pose is an ephemeral copy and never becomes gameplay state.

- MOVE_FORWARD / MOVE_BACK: only one preview at source + delta/2.
  Logs: [VIEWANIM] FRAME mode=move profile=axial-midpoint
  step=1/1 sample=1/2, then END intermediates=1.
  Extra preview rendering reduced to one ~103-106ms paint on most
  recent hardware samples; the user prefers the resulting pacing.
- MOVE_LEFT / MOVE_RIGHT (strafe): unchanged two previews at
  source + delta/3 and source + 2*delta/3, profile=strafe-thirds,
  steps 1/2 and 2/2, END intermediates=2.
- TURN_LEFT / TURN_RIGHT: unchanged quarter-turn preview and final
  cardinal image logic. No gameplay turn on rotation.
- Collision-blocked/unsupported/event-deferred MOVE: no animation.
  Automap mode retains original cardinal MOVE/turn presentation.
- RenderVisualMove validates only (step,denominator)=(1,2),(1,3),(2,3),
  as well as the already committed, settled, exact cardinal delta.
  No intermediate logic touches player pose, map data, automap publish,
  monster activation, map events, RNG, checkpoint or input queue.
- Failed optional preview skips to the mandatory canonical render;
  the same post-commit rollback policy remains in force on a final
  render failure. No second framebuffer or permanent animation buffer.

## Remaining optional regression coverage

1. Flash normal esp32-cyd on the latest SHA. In open space try at
   least two FORWARD and two BACK steps. Expect one
   [VIEWANIM] FRAME mode=move profile=axial-midpoint
   step=1/1 sample=1/2 and END intermediates=1 for each.
   Record the new totalUs/previewUs and describe whether the
   double-step feeling is gone without making movement feel snappy.
2. Try both STRAFE directions: two unchanged previews per step with
   profile=strafe-thirds and END intermediates=2.
3. Rotate left/right: keep two camera previews and the same old
   perception; do not trigger a monster turn.
4. Walk into a wall or a closed door: MOVE-BLOCKED and no preview.
   Normal MOVE with open door must not duplicate its event or turn.
5. Watch single MONSTERTURN dispatch per committed MOVE and ALIVE
   heap8/largest8 stability. Prefer one interaction/dialog and
   SYS EXIT to confirm clean menu and DIALOGCHAIN release.
6. If the perspective still feels like two hops, report the visual
   effect. No undocumented changes to presentation timings or
   PlatformVideo_present optimizations without measured evidence.

CI success (if obtained) confirms compilation, not physical UX.
After physical PASS, update milestone and PORTING_STATUS/DOCUMENTATION
with the exact code SHA; post-test closure for the passing code is
docs-only. Never merge main without explicit instruction.
