# Milestone — Native camera interpolation for movement

Status: **MOVE mechanics observed on real CYD; axial pacing retune awaiting test**

Active branch: agent/esp32-camera-rotation-visual (same branch as rotation).
Main after PR #200 merge: f2a178e1fa0d04e0e163215bef6d745b7ea64da5.
Rotation visual hardware-PASS code remains
db0eb476d8070d4a560893718ae79e7570f72fcc.
Initial MOVE code: 529e7abb177060743ffb8495e97b1be14606fd20.
Revised MOVE pacing candidate: e944dc386758900bc55c68d374344c631640e6b4.

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

## Current revised candidate: directional visual sampling

Successful WORLD-mode MOVE still performs **exactly one** collision
preflight, one event/transaction commit and a normal canonical final frame.
The visual pose is an ephemeral copy and never becomes gameplay state.

- MOVE_FORWARD / MOVE_BACK: only one preview at source + delta/2.
  Logs: [VIEWANIM] FRAME mode=move profile=axial-midpoint
  step=1/1 sample=1/2, then END intermediates=1.
  Expected extra preview render count reduced from 2 to 1;
  actual new wall-clock time and human perception require remeasurement.
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

## Required next physical CYD acceptance

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
