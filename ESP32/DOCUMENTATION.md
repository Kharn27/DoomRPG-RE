# ESP32 documentation map

Recovery and development should start from:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — current tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers.

Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime truth.

## Current branch

```text
main = bb95082917d2bb3504243015de358d40a8d11788
branch = agent/esp32-sprite-storage-profile
base main = bb95082917d2bb3504243015de358d40a8d11788
hardware-tested final code HEAD = abb66f4a44c196b01511df66d1230c72040546e2
status = REAL-CYD HARDWARE PASS
merge-ready = YES after docs-only tail verification
```

This branch profiles the resident sprite-storage path and fixes a renderer stack
failure exposed while reproducing the performance corpus. It does not introduce
a second large cache or a new renderer architecture.

## Build environments

Normal hardware reference:

```text
pio run -e esp32-cyd
```

Bring-up diagnostics can perturb RAM and are not the production memory canon.
Never claim a build or hardware pass that did not occur.

## Hardware / memory invariants

```text
classic CYD ESP32-2432S028R
ESP32-D0WD-V3, dual core 240 MHz
4 MB flash
no PSRAM
160x120 RGB565 framebuffer = 38400 B
shapeData == NULL
mediaTexels == NULL
native backing store = /DoomRPG-ESP32.pak
```

Do not solve performance by recreating map-wide legacy texel ownership.

## Native data path

Migrated map/runtime data uses:

```text
/DoomRPG-ESP32.pak
 -> native reader/catalog
 -> compact immutable map/runtime owners
 -> bounded mutable overlays/caches
```

`/DoomRPG.zip` remains transitional compatibility debt for menu/HUD/bootstrap
paths only. Do not move migrated runtime data back to ZIP ownership.

## Source layout

```text
ESP32/src/esp_map_*                        map/runtime/event ownership
ESP32/src/esp_player_*                     player/view ownership
ESP32/src/esp_native_gameplay_*            live gameplay/action semantics
ESP32/src/esp_native_first_frame.c         world/BSP wall renderer compatibility layer
ESP32/src/esp_native_sprite_renderer.c     reusable sprite renderer
ESP32/src/esp_asset_pack.cpp               PAK backing store + resident cache
ESP32/src/esp_native_dynamic_line_render.c mutable line presentation overlay
ESP32/src/platform_*                       CYD input/video bridges
```

A new BSP must not create another map-specific engine.

## Header/include hygiene

The legacy C headers contain historical declarations that do not mix safely with
arbitrary C++/framework include chains. Rules:

```text
prefer existing project clocks/APIs when sufficient
keep legacy/SDL include order explicit
never shadow ESP-IDF/Arduino headers in ESP32/include
never inject DoomRPG.h through a framework-shadow header
use a narrowly project-named adapter if a native compatibility boundary is needed
```

The removed `ESP32/include/esp_timer.h` shim is the canonical example of what not
to reintroduce.

## Linker-wrapper hygiene

Permanent rule:

```text
A linker wrapper must not call a higher-level API that can indirectly re-enter
that wrapped symbol.
```

Prefer `__real_*`, already-materialized owner views, or explicitly non-reentrant
helpers. The dynamic-line texture recursion failure is the hardware witness for
this rule.

## Framebuffer owner arbitration

Temporary visual owners with exact framebuffer snapshots must not overlap writes
without arbitration. Current example:

```text
touch control flash owns exact pixels for 120 ms
Action feedback expiry waits while that owner is active
```

Do not weaken exact restore/FNV checks to hide owner collisions.

## BSP traversal rule

The native world renderer must not rely on recursive BSP traversal on the classic
CYD. A real gameplay turn exposed a `loopTask` stack-canary failure after the
legacy wall guard retry progressed deeper than the first failed render pass.

Final hardware-tested traversal:

```text
front-to-back DFS order preserved
recursion = none
node stack = 65 x uint16_t
level stack = 65 x uint8_t
transient workspace = 195 B
new heap allocation = none
new permanent BSS owner = none
```

Do not repeatedly increase Arduino `loopTask` stack to mask renderer recursion.
Keep traversal bounded, explicit and fail-closed.

A first iterative version used a 520 B BSS owner and changed early boot heap
topology enough to make the transitional `menu.bsp` ZIP inflate allocation fail.
This is an important no-PSRAM lesson: even a small permanent owner can move a
fragmentation-sensitive compatibility path.

## Legacy wall guard recovery

A bounded one-byte guard currently recovers a specific compact legacy wall
sampling boundary. Hardware-tested route:

```text
SPAN_OOB
 -> unwind renderer
 -> resolve one successor packed byte
 -> retry world render
 -> RECOVERED
```

At the reproduced angle 64 -> 128 turn, final hardware logs show:

```text
[NATIVEFRAME] LEGACY_GUARD logical=15 actual=40 source=20480 successorActual=68 successorSource=32768 byte=11
[NATIVEFRAME] RETRY legacy compact guard after unwound SPAN_OOB line=277 actual=40
[NATIVEFRAME] WALL requests=19 draws=19 spans=160 pixels=6328 ...
[NATIVEFRAME] RECOVERED legacy compact guard actual=40 successorActual=68 source=20480->32768
[RESIDENTGAMEPLAY] TURN ... angle=64->128 committed=yes
```

The following forward move also commits, proving the resident gameplay service
stays alive after recovery.

## Sprite storage profile

`[SPRITEPROFILE]` measures logical sprite loading and deltas from the existing
resident PAK cache. The distinction between logical reads and physical reads is
critical.

Cold witness:

```text
spriteUs=641298
logicalReads=148
frameLoads=23
unique=12
physicalReads=81
physicalBytes=12467
range=76H/69M/69S/3B
```

Warm steady-state examples:

```text
spriteUs ~= 19-23 ms, physicalReads=3
spriteUs = 10598 us, physicalReads=0
spriteUs = 9249 us, physicalReads=0
```

Therefore do not infer SD cost from `logicalReads` alone. The existing 16 KB
resident owner can satisfy large logical workloads entirely from cache.

## Performance direction

The old working hypothesis that steady-state sprites were the dominant hotspot
is superseded by the hardware profile.

Current reproduced route:

```text
warm sprite phase          = ~9-23 ms
raw VIDEO present          = ~34-35 ms
normal full gameplay frame = ~208-267 ms
legacy-guard retry TURN    = 453208 us
following MOVE             = 283531 us
```

The next bounded performance audit should therefore target:

```text
world / plane / wall renderer
```

Measure separately:

```text
plane time
BSP traversal/projection time
wall texture acquisition / physical PAK traffic
wall raster time
legacy-guard failed pass + retry cost
```

Preserve exact framebuffer output and compact ownership. Do not optimize
`PlatformVideo_present()` first and do not add a large map-wide cache without
hardware evidence.

## Action / Combat recovery notes

Current native Action remains bounded. Important legacy specification nuance:
remote extinguisher attempts can still enter attack presentation; outside the
one-tile hit range they miss and legacy reports `No effect!`.

Until generic combat/miss/ammo/turn ownership exists:

```text
adjacent fire -> native FIRE_CLEARED transaction
remote fire   -> FIRE_RANGE_DEFERRED, mutation=no
```

This is a temporary safe divergence, not final gameplay behavior.

## Pickup frontier

Owned:

```text
eType=5 weapon pickup
consumed-sprite overlay
native weapon mask/select
rollback on redraw failure
```

Deferred:

```text
eType=3 player-stat/world item
eType=4 inventory
eType=6 ammo
eType=16 alternate ammo
weapon ammo/message/sound consequences
```

Do not implement helmets, health or ammo as subtype-specific one-offs.

## Cleanup rule for probes

Do not delete a file simply because its name contains `probe`. Classify whether
it is production implementation, useful instrumentation, or obsolete temporary
surface first. Performance probes should remain while they materially improve
hardware diagnosis.

## Development workflow

```text
recover exact current/legacy behavior
 -> choose one bounded permanent owner/API
 -> add strict observability
 -> fail closed outside supported cases
 -> commit/push agent/*
 -> build normal esp32-cyd
 -> test real CYD
 -> trust Serial
 -> document only observed results
 -> verify all post-test commits are docs-only
 -> declare merge-ready
```

## After this branch merges

Re-read actual GitHub `main`, record its exact SHA, and create the next `agent/*`
branch from that SHA. Do not continue development from this branch by assumption.

Current strongest next milestone: bounded world/plane/wall renderer profiling and
optimization. Player-stat pickups and generic combat remain separate correctness
families.
