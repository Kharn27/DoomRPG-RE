# Milestone — Native camera interpolation for movement

Status: CODE CANDIDATE — REAL-CYD TEST PENDING

Same active branch: agent/esp32-camera-rotation-visual.
Main at start of this extension: f2a178e1fa0d04e0e163215bef6d745b7ea64da5
(PR #200 merged). No new branch and no main merge.
Previous rotation hardware PASS remains pinned to
db0eb476d8070d4a560893718ae79e7570f72fcc.
New MOVE candidate code SHA:
529e7abb177060743ffb8495e97b1be14606fd20.

## Design and strict scope

- For successful, settled 64-unit cardinal translations from normal
  gameplay WORLD mode only, display two intermediate camera frames at
  1/3 and 2/3 of source-to-destination displacement.
- A single real gameplay MOVE is already prepared and committed before
  any preview. No interpolation modifies EspPlayerView's published pose.
  A render-local EspPlayerViewState has its viewX/viewY/destX/destY set
  to intermediate coordinates; cardinal angle and logical HUD direction
  remain exactly equal to the committed view.
- Frame preview verifies the active canonical view still equals the
  committed destination, that before/after differ only in position,
  that both endpoints are settled, and that displacement is exactly
  one +/-64 cardinal-axis step. Every other case fails closed.
- Reuse the proven native BSP wall/plane/sprite composition with
  pure read-only preview visibility, no automap publication, no gameplay
  monster activation, no action-engine FreshFrame mark. Existing
  framebuffer (160x120 RGB565) reused for every physical present.
- The final canonical frame is mandatory, exactly as before. Preview
  errors emit [VIEWANIM] FALLBACK and skip remaining intermediate
  frames; they never alter the original failed-final-frame rollback.
- No animation when collision is blocked, event preflight defers or
  move commit fails. Automap-mode MOVE preserves original single
  settled presentation; TURN path stays exactly as previously tested.
- World/tile event continuation, status messages, dialogs and monster
  turn ordering are unchanged. A normal committed MOVE still advances
  its legitimate monster turn exactly once, never once per preview.
- Each intermediate native frame invokes bounded transient renderer
  scratch and PAK reads, so wall-clock render costs need real-hardware
  confirmation. No heap-based persistent animator or frame queue.

## Real-CYD acceptance on normal esp32-cyd

1. Boot and enter intro.bsp. Make at least two FORWARD steps in open
   space. Each step must display [VIEWANIM] FRAME mode=move step=1/2
   and step=2/2, with presented=1, then END intermediates=2,
   then the unchanged [RESIDENTGAMEPLAY] MOVE ... committed=yes.
2. Step BACK and, if controls support it, STRAFE left and right.
   Verify camera positions increase/decrease smoothly and the world
   finishes aligned at the original exact cardinal tile center.
3. Walk into a blocked wall/closed door: MOVE-BLOCKED with no preview.
   Open a door and move through it: check textures, clipping and that
   game events are applied exactly once (not once per frame).
4. Ensure monster turn token increments only once for each legitimate
   committed MOVE; rotate in place and verify zero extra monster turns.
5. Exercise a dialog trigger and SYS->EXIT to menu. Watch for
   [TURNFRAME] DIAG errors, [VIEWANIM] FALLBACK, heap8/largest8 drift,
   [RESIDENTRESET] empty=1 and MENU_MAIN 522dc605.
6. In AUTOMAP mode MOVE and TURN should use the original nonanimated
   path. Verify LOAD/save compatibility if practical.
7. Human visual criterion: movement must feel smooth and responsive.
   Record [VIEWANIM] totalUs and whether two intermediate frames are
   enough; do not optimize PlatformVideo_present prematurely.

No hardware PASS, RAM delta, elapsed time or acceptance is fabricated.
Only once the code SHA passes CI and the physical CYD should docs be
changed to hardware PASS (docs-only post-test).
