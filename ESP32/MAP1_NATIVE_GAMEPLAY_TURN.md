# MAP1 native gameplay TURN dispatcher

## Status

```text
branch = agent/esp32-native-turn-dispatch
base   = cdda239f1c884a7d6f6707ba1c30a0a0a3603923
hardware-tested implementation SHA = 66ba643e7650f51d0022cd56e007242902d76c77
status = REAL-CYD HARDWARE PASS
merge-ready = yes after documentation-only closeout
```

Base `cdda239f1c884a7d6f6707ba1c30a0a0a3603923` is merged PR #92 (`agent/esp32-native-touch-input-intent`).

## Objective

Execute the first real native gameplay action family on the classic CYD: `TURN_LEFT` and `TURN_RIGHT` only.

The permanent dispatcher mutates only the settled native player/view orientation plus a compact gameplay-turn owner. The render bridge then recomposes the current Junction frame from the native map/graphics owners. Every other recognized gameplay action remains deferred.

This milestone deliberately does **not** call `Game_advanceTurn`, `Game_executeTile`, the broad legacy `DoomCanvas_finishRotation`, entity/monster gameplay, movement collision, facing refresh, or the legacy PLAYING event loop.

## Legacy behavior recovered

The mobile action IDs remain:

```text
TURN_LEFT  = 3
TURN_RIGHT = 4
```

One cardinal turn is exactly 64 angle units:

```text
TURN_LEFT  => +64
TURN_RIGHT => -64
angle domain = 0,64,128,192 modulo 256
```

The fresh Junction pose begins at angle `64` / North.

## Permanent native ownership

Permanent dispatcher API:

```text
ESP32/include/esp_native_gameplay_dispatch.h
ESP32/src/esp_native_gameplay_dispatch.c
```

Compact owners:

```text
EspNativeGameplayTurnState    = 24 B
EspNativeGameplayDispatchResult = 12 B
EspPlayerViewState            = 44 B
```

`EspNativeGameplayTurnState` owns the cardinal runtime vectors:

```text
angle 0   / E: sin=0      cos=65536  step=64,0
angle 64  / N: sin=65536  cos=0      step=0,-64
angle 128 / W: sin=0      cos=-65536 step=-64,0
angle 192 / S: sin=-65536 cos=0      step=0,64
```

Prepare/commit is stale-checked and rollback-capable. Non-TURN semantic actions return `DEFERRED` without player/view mutation.

The older fresh-map `EspPlayerOrientationState` remains untouched; this milestone adds a distinct gameplay-turn owner instead of rewriting the hardware-proven post-load boundary.

## Input/render scheduling

The hardware-proven gameplay input probe remains the sole touch callback owner. A TURN follows this sequence:

```text
physical tap
 -> calibrated logical hit
 -> compact semantic intent
 -> 120 ms yellow neon/glyph feedback
 -> exact restore of the current dynamic frame
 -> queue TURN
 -> return the complete lifecycle once
 -> commit native cardinal orientation
 -> render on a later service
```

No render occurs from a touch callback. This was required after an earlier prototype triggered the `loopTask` stack canary when the heavy render chain was entered from callback depth.

The final hardware run reports:

```text
stackHighWater = 172
execScratchBytes = 464
heap8 = 67284 -> 67284
largest8 = 34804 -> 34804
```

## Runtime frame bridge

Permanent/bridge modules introduced or extended by this milestone include:

```text
ESP32/include/esp_native_gameplay_frame.h
ESP32/src/esp_native_gameplay_frame.c
ESP32/include/esp_native_gameplay_present_gate.h
ESP32/src/esp_native_gameplay_present_gate.c
ESP32/include/esp_native_gameplay_hud_direction.h
ESP32/src/esp_native_gameplay_hud_direction.c
```

The current TURN compositor reuses the historical first-frame world renderer, but suppresses its intermediate physical presentation. It restores the existing HUD bands from one bounded temporary 12.8 KiB buffer, repaints only the compass dirty rectangle, then performs exactly one final complete-frame presentation.

This is intentionally a bridge toward a future permanent viewport-only gameplay renderer. The 12.8 KiB HUD save is bounded and non-persistent, but remains a later cleanup opportunity rather than a blocker for this milestone.

The sprite renderer's older fixed-North proof required visible mode7/glow witnesses. Runtime cardinal views may legitimately contain no visible mode7/glow. The TURN compositor therefore accepts a view only when all admitted base/glow objects are fully accounted for, `unsupported=0`, `glowDeferred=0`, and renderer scratch is exactly restored. The original fixed-pose renderer contract remains unchanged.

## Compass dirty rectangle fix

An intermediate implementation cleared a fixed bottom-HUD x-range before drawing the compass and accidentally overlapped the ammo text. That caused cumulative HUD drift across turns even though the 3D viewport returned exactly to North.

The final implementation computes the compass dirty rectangle from the actual arrow/glyph footprint only. The final 360-degree run proves the bottom HUD returns bit-exactly to its canonical state.

## Real-CYD hardware proof

Hardware-tested firmware:

```text
66ba643e7650f51d0022cd56e007242902d76c77
normal env = esp32-cyd
```

Initial runtime boundary:

```text
angle = 64 / N
frame = ba3e5182
viewport = 9206eb24
HUD bands = 6c2aa46f
heap8 = 67284
largest8 = 34804
stackHighWater = 172
physicalPresentsPerTurn = 1
```

The user executed four consecutive `TURN_RIGHT` actions, completing a full 360-degree rotation.

### Right turn 1: N -> E

```text
angle 64 -> 0
viewFNV afcdcf74 -> 48ec8b74
orientation FNV c5588d16 -> 47803548
viewport after sprites = 17c48c15
HUD bands after direction = 1d908304
frame = ba3e5182 -> 8cfdfe34
walls = 5 / 11395 pixels
planes = 12800 pixels
sprites = 1 / 531 pixels
glows = 1 / 922 pixels
spriteReads = 22
hudReads = 63
hudPixels = 270
```

### Right turn 2: E -> S

```text
angle 0 -> 192
viewFNV 48ec8b74 -> a4f4a874
orientation FNV 47803548 -> d579a8dc
viewport after sprites = 582c2ad8
HUD bands after direction = a78d0f96
frame = 8cfdfe34 -> da1c4297
walls = 6 / 12562 pixels
planes = 12800 pixels
sprites = 0 / 0 pixels
glows = 0 / 0 pixels
spriteReads = 4
hudReads = 63
hudPixels = 269
```

The S view is important proof that zero actually drawn sprites/glows is valid when BSP-visible candidates are fully accounted for by culling.

### Right turn 3: S -> W

```text
angle 192 -> 128
viewFNV a4f4a874 -> 25568274
orientation FNV d579a8dc -> 5cf32f86
viewport after sprites = de06a408
HUD bands after direction = 9281a6d1
frame = da1c4297 -> 23ee0954
walls = 5 / 10959 pixels
planes = 12800 pixels
sprites = 1 / 402 pixels
glows = 1 / 923 pixels
spriteReads = 28
hudReads = 63
hudPixels = 278
```

### Right turn 4: W -> N exact round trip

```text
angle 128 -> 64
viewFNV 25568274 -> afcdcf74
viewport world = 032ffaed
viewport after sprites = 9206eb24
HUD bands after direction = 6c2aa46f
frame = 23ee0954 -> ba3e5182
roundTrip = exact
walls = 34 / 4341 pixels
planes = 12800 pixels
sprites = 21 / 1828 pixels
glows = 7 / 1917 pixels
spriteReads = 172
hudReads = 63
hudPixels = 280
```

The exact North recovery simultaneously re-proves the established Junction renderer canons:

```text
walls+planes viewport = 032ffaed
complete sprite+glow viewport = 9206eb24
complete gameplay frame = ba3e5182
HUD bands = 6c2aa46f
```

## Side-effect proof

Every successful TURN reported:

```text
heap = 67284 -> 67284
largest = 34804 -> 34804
stackHighWater = 172
legacyStable = yes
residentStable = yes
turnAdvance = no
tileDispatch = no
facingRefresh = deferred
intermediatePresentSuppressed = 1
finalPresent = 1
```

Persistent architecture invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map/graphics access forbidden
legacy Game.entities == 0
legacy Game.monsters == 0
legacy broad PLAYING loop not called
```

The real CYD visibly rotated through all four orientations. The user reported the result working and was satisfied with the milestone.

## Performance notes

Two sources of avoidable latency were removed during bring-up:

1. rendering was moved out of the touch callback to a later lifecycle service;
2. the historical world-only intermediate physical presentation was suppressed.

The invisible-button neon hold was reduced from 250 ms to 120 ms. The current physical display path still costs roughly 34 ms per full 160x120 -> 320x240 x2 present on the tested CYD, and the gameplay TURN issues one final render presentation after the separate input feedback/restore presentations.

Do not prematurely optimize `PlatformVideo_present()`. A later renderer milestone can remove the temporary 12.8 KiB HUD save by making the native world renderer viewport-only.

## Hardware PARK after this milestone

```text
legacyState = 9 / ST_INTRO
page = 3
targetMap = 9
junctionResident = yes
nativeST_PLAYING = yes
nativePlayingService = yes
nativeHud = yes
nativeInput = yes
nativeTurnDispatch = yes
TURN_LEFT = executed native
TURN_RIGHT = executed native
cardinal 360-degree renderer round-trip = exact
Game_advanceTurn = no
Game_executeTile = no
legacy entities = 0
legacy monsters = 0
```

## Still intentionally outside

```text
movement / strafe dispatch
movement collision
SELECT interaction
weapon switching
PASS_TURN execution
MENU/AUTOMAP gameplay execution
actual turn advancement
post-turn tile scripts
facing refresh
entity/monster gameplay
first-person weapon overlay
general animation/update loop
```

## Next bounded milestone

After merge, recover the exact new `main` SHA before branching.

A coherent next gameplay frontier is native cardinal translation (`FORWARD/BACK/STRAFE`) with collision semantics, while keeping entity activation, tile-event execution and turn advancement fail-closed until their own bounded milestones. A smaller renderer-only alternative is the first-person weapon overlay (fresh Junction pistol logical sprite 242), but it should not be mixed into movement dispatch.