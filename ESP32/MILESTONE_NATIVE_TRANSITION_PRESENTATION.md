# Milestone — native transition presentation

## Boundary

Hardware-tested code SHA:

`6903431a60700960127be95d95584728698a36c8`

Current main at this validation tail:

`2af4aab025b6fb0094b31eea9403477f892b796a`

Branch:

`agent/esp32-native-transition-polish`

Hardware:

`ESP32-2432S028R classic CYD, no PSRAM`

Validation status:

`REAL-CYD PASS — checkpoint LOAD from MENU_MAIN and from an already-running gameplay HUB`

No new CI/build-size claim is attached to this post-main polish tail; the real-CYD
Serial/visual result is the authority for this boundary.

## Result

The transition/loading presentation is now validated as one shared ownership
pipeline rather than two caller-specific load paths.

The same checkpoint restore entry point is used whether LOAD is requested from
the cold main menu or from the in-game SYS HUB. The in-game case first tears down
the old gameplay/HUB session, then the loading owner acquires the shared logical
framebuffer. No old-session cleanup is allowed to run after loading ownership
starts.

Both hardware paths now complete successfully:

- loading remains full-screen throughout restore and cache/session priming;
- no HUB/gameplay pixels leak into progress frames;
- progress advances through the long priming tail instead of sitting at 100%;
- already-warm gameplay caches do not fail the resume witness;
- the 100% frame is reached before handoff;
- the first visible resumed gameplay frame contains the complete world and both
  HUD bands.

## Reusable component boundary

`EspNativeTransitionPresentation` remains the permanent reusable full-screen
presentation owner.

Current public API:

```c
EspNativeTransitionPresentation_beginLoading(targetMapId)
EspNativeTransitionPresentation_progress(phase, completed, total)
EspNativeTransitionPresentation_checkpointProgress(percent, stage)
EspNativeTransitionPresentation_isLoadingActive()
EspNativeTransitionPresentation_abortLoading(reason)
EspNativeTransitionPresentation_releaseLoading(reason)
EspNativeTransitionPresentation_endLoading()
EspNativeTransitionPresentation_reset()
```

The owner is shared by native transition/loading users including CHANGEMAP,
checkpoint LOAD, and fresh-start/intro handoff. Target identity and progress
semantics are caller-owned; the current visual skin remains encapsulated.

Current visual skin:

- `c.bmp` starfield background;
- compact HUB mini-font;
- amber/steel/ivory palette;
- fixed loading card and progress geometry.

## Shared framebuffer ownership

There is still only one 160x120 RGB565 logical framebuffer. No second framebuffer
and no PSRAM buffer were introduced.

Gameplay presentation calls are suppressed while the transition owner is active,
but gameplay code can still mutate the shared logical framebuffer before reaching
that present gate. Therefore a retained loading image cannot be trusted between
progress updates.

The permanent rule is now:

```text
progress update
 -> reconstruct complete loading frame into logical framebuffer
 -> real physical present
```

This deliberately repaints `c.bmp` and the complete card for each progress
publication. It closes the observed in-game contamination where old HUD/HUB
writers modified the framebuffer behind the loading owner.

Important invariant: do not insert arbitrary loading progress publications after
the final gameplay framebuffer has been prepared unless the handoff sequence also
rebuilds that gameplay framebuffer afterwards.

## Checkpoint progress contract

Checkpoint resume currently publishes the bounded stages:

```text
BEGIN          0
CHECKPOINT    10
BSP           30
RUNTIME       60
STATE         75
RESTORE       85
CACHE-COLD    90
CACHE-WARM    93
CACHE-LEARN   96
READY        100
```

The cache stages are presentation progress, not correctness gates requiring a
specific cache replacement pattern. A resume from an already-running game may
enter LARGE-LEARN with the relevant ranges already hot, so zero new stores or
zero retained large-range entries after the render are not by themselves a load
failure. Functional readiness still requires the resident mode and actual
render/cache operation to succeed.

## Unified checkpoint LOAD ordering

The checkpoint loader now has one lifecycle regardless of caller:

```text
read/validate checkpoint
 -> EspNativeGameplaySession_reset()
 -> beginLoading()
 -> rebuild resident map/runtime
 -> restore player + mutable overlays + V8 monster snapshot
 -> configure checkpoint-resume session
 -> cache/session prime
 -> READY 100%
 -> rebuild final gameplay frame
 -> release loading owner (no present)
 -> repaint retained top + bottom gameplay HUD
 -> final gameplay present
```

Moving session reset before `beginLoading()` is essential for the SYS HUB path:
the old HUB/session may have presentation cleanup to perform. The main-menu path
has no old gameplay session, but it intentionally executes the same loader
ordering.

## Final handoff contract

`READY 100%` reconstructs and physically publishes a complete loading frame.
That necessarily overwrites the logical framebuffer, so the handoff then
reconstructs gameplay before release.

The final sequence is:

```text
[TRANSITIONLOAD] ... stage=READY overall=100
[ENGINECACHE] FINAL-READY ...
[TRANSITIONLOAD] RELEASE ...
[GAMEPLAYHUD] REPAINT ... pixels>0 ...
[ENGINESESSION] FINAL-HUD ... bands=top+bottom ...
... facing/status overlay composition as needed ...
[VIDEO] Present ...
[ENGINESESSION] RESUME-VISIBLE ...
```

`releaseLoading()` does not itself present. This creates a safe window where
HUD reconstruction is allowed again but the physical LCD still shows the 100%
loading frame. The single following gameplay present publishes the complete
handoff atomically.

## Hardware evidence / regressions closed

The real CYD exposed and closed these transition-specific regressions during this
polish tail:

1. in-game HUD/HUB contamination of the loading framebuffer;
2. progress reaching 100% too early and then waiting through long priming;
3. warm-cache resume falsely failing LARGE-WARM/LARGE-LEARN witnesses;
4. final gameplay handoff missing the lower HUD band;
5. caller-context dependence caused by tearing down the old session after
   loading ownership had already begun.

The final user validation explicitly covers both checkpoint entry contexts:

```text
MENU_MAIN -> Load Game : PASS
running gameplay -> SYS HUB -> Load Game : PASS
```

## Permanent constraints retained

- logical framebuffer remains 160x120 RGB565;
- no second framebuffer;
- no PSRAM;
- `shapeData == NULL`;
- `mediaTexels == NULL`;
- runtime checkpoint source remains the native save + `DoomRPG-ESP32.pak`;
- no runtime ZIP fallback;
- transition presentation owns physical publication while active;
- resumed gameplay is published only after explicit ownership release and HUD
  reconstruction.
