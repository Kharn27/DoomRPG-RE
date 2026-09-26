# Milestone — native transition presentation

## Boundary

Hardware-tested code SHA:

`a81dd38a6875154b37b9006a145eef5d73e85a66`

Base main at validation time:

`8dd660ce1017540c364591cad54514c9c788acf5`

Branch:

`agent/esp32-native-level-stats-loading`

CI:

`esp32-cyd #867 SUCCESS`

Memory/build witness:

```text
RAM static = 45224 B / 327680 B (13.8%)
Flash      = 795461 B / 1310720 B (60.7%)
```

## Result

The native transition presentation is now hardware-validated on the real classic
CYD for both level-transition loading and checkpoint LOAD presentation.

The user explicitly accepted the final LOAD Game result as clean/professional.
The previously observed regressions are closed:

- no gameplay top bar appears over the loading frame;
- no gameplay bottom HUD appears over the loading frame;
- no stale restored GIB/blood burst is replayed during checkpoint resume;
- loading remains the visible owner through runtime/cache/session priming;
- the first visible gameplay image is published only after the loading owner is
  released.

## Reusable component boundary

`EspNativeTransitionPresentation` is a permanent reusable presentation owner,
not a map-specific screen.

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

The same loading owner is used by:

1. native CHANGEMAP handoff / raw-map-flash rebuild;
2. native checkpoint LOAD / session reconstruction.

Parameters already owned by callers include target map identity, generic progress,
checkpoint stage text, and lifecycle/release reason. The target label is resolved
from the native map catalog.

The current visual skin is deliberately fixed inside the component:

- `c.bmp` fixed first-frame background;
- compact HUB mini-font;
- amber/steel/ivory palette;
- fixed card/progress geometry.

That styling is not yet a public theme/config object. A later UI-polish milestone
may expose a small immutable style/config descriptor without changing the
loading callers or ownership contract.

## Presentation ownership fix

The final hardware failure exposed an ownership bug rather than a renderer bug.

The checkpoint path correctly called `beginLoading()`, but session reset later
requested `PlatformInput_setTapCallback(NULL)`. The generic WAIT_STATS bridge
interpreted the absence of a level transition as stale transition state and
called `EspNativeTransitionPresentation_reset()`, silently destroying the
checkpoint loading owner.

The final code preserves an already-active checkpoint loading owner across this
unrelated NULL callback.

Additionally, the loading gate now lives at
`esp_native_gameplay_present_gate.c`, the sole permanent
`--wrap=Esp32PlatformVideo_present` boundary. While loading owns presentation,
gameplay presents return acknowledged-but-suppressed before Action/GIB/HIT
decorators can modify or publish the logical framebuffer.

TransitionPresentation's own progress frames use the real physical-present leaf,
so progress remains visible.

## Restored GIB ownership fix

The hardware log also identified the apparent "damage splash" as:

```text
[GIBFX] PAINT sprite=10 ...
```

A monster already dead in the V8 checkpoint had been interpreted as a newly
observed death during the first resume present.

Checkpoint resume now adopts the restored monster state into the bounded
presentation-only GIB owner before any visible gameplay present. Restored dead
monsters are marked historical; live monsters remain eligible for future genuine
death bursts.

Expected witness:

```text
[GIBFX] CHECKPOINT-ADOPT ... replay=no mutation=presentation-owner-only rng=untouched
```

No gameplay state, monster state, topology, renderer state, or gameplay RNG is
mutated by this adoption.

## Permanent constraints retained

- logical framebuffer remains 160x120 RGB565;
- no second framebuffer;
- no PSRAM;
- `shapeData == NULL`;
- `mediaTexels == NULL`;
- loading background is rendered once and kept fixed;
- progress updates do not re-read loading assets;
- gameplay presentation is blocked while the loading owner is active;
- first resumed gameplay publication happens only after explicit release.
