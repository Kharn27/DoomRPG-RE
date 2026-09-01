# ESP32 documentation map

Recovery and development should start from:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — current tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers;
5. the latest relevant milestone/source on the active branch.

Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime truth.

## Current locked branch

```text
main = fe69c2fbcaed874e2cf35828c326d8c8b3465375
branch = agent/esp32-world-render-profile
base main = fe69c2fbcaed874e2cf35828c326d8c8b3465375
code boundary before docs = 9d69272ed8394c230d5eec79a2adc37dd44f3c04
code tree = 181e084bc8abdea1ebc258a5b411e2b60797b0ce
hardware-tested equivalent baseline = efaa067fd9881a1c055b83eed4c80d5de0e33aad
status = renderer/storage profile complete; branch locked
```

`9d69272...` is an explicit revert and has the exact same tree as the real-CYD
validated `efaa067...` baseline. Only documentation commits may follow it on this
branch.

## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

Bring-up diagnostics perturb RAM and are not the production memory canon. Never
claim a local build or hardware pass that did not occur.

## Hardware / permanent memory rules

```text
classic CYD ESP32-2432S028R
ESP32-D0WD-V3 dual core 240 MHz
4 MB flash
no PSRAM
160x120 RGB565 framebuffer = 38400 B
shapeData == NULL
mediaTexels == NULL
native backing store = /DoomRPG-ESP32.pak
```

The permanent architectural rule is bounded explicit ownership, not a specific
cache size. `16 KiB / 256 records` is the current resident-cache implementation,
not a sacred invariant.

Do not recreate map-wide texel ownership or migrate native runtime data back to
ZIP. `/DoomRPG.zip` remains transitional bootstrap/menu compatibility debt only.

## Source layout

```text
ESP32/src/esp_map_*                        map/runtime/event ownership
ESP32/src/esp_player_*                     player/view ownership
ESP32/src/esp_native_gameplay_*            live gameplay/action semantics
ESP32/src/esp_native_first_frame.c         BSP/wall compatibility renderer
ESP32/src/esp_native_plane_renderer.c      native floor/ceiling plane renderer
ESP32/src/esp_native_sprite_renderer.c     native sprite renderer
ESP32/src/esp_asset_pack.cpp               PAK backing + resident cache
ESP32/src/esp_native_dynamic_line_render.c mutable line presentation
ESP32/src/platform_*                       CYD input/video bridges
```

A new BSP must never create another map-specific engine.

## Renderer stack rules

Two permanent lessons are hardware-proven:

```text
1. BSP traversal must remain explicit, bounded and non-recursive.
2. Large renderer workspaces must not casually live on the 9 KiB loopTask stack.
```

The BSP walk already uses a bounded transient iterative stack. During the current
milestone, `PlaneWork` (~1.1 KiB) also proved too large for a deep render path:
a real CYD stack canary occurred after `[NATIVEPLANE]` completed but before its
caller resumed.

Accepted fix:

```text
PlaneWork = temporary heap lease per plane render
six existing 2048 B plane texture buffers = still temporary
new persistent owner = none
new BSS = none
free before return = yes
```

Real-CYD validation subsequently passed the old guard/retry route, regular door
open/close, medkit dialog resume, guard door and extinguisher fire clear without
stack canary.

Do not increase `loopTask` merely to hide renderer stack pressure. Early boot is
fragmentation-sensitive on this no-PSRAM target.

## Linker/include hygiene

A linker wrapper must not call a higher-level API that can indirectly re-enter
its wrapped symbol. Prefer `__real_*`, already-materialized views, or explicit
non-reentrant helpers.

Never shadow Arduino/ESP-IDF framework headers under `ESP32/include`; the removed
`esp_timer.h` shim is the canonical failure mode.

## World/storage profile conclusions

Current real-CYD evidence separates three costs:

```text
VIDEO present          ~= 34-35 ms
plane phase            commonly ~= 80-150 ms, view/read dependent
warm sprite phase      commonly ~= 6-26 ms
cold/thrashing sprites ~= 500-870 ms with tens of physical SD reads
```

Therefore the loaded fire room is not intrinsically beyond the ESP32. It can run
around ~280-330 ms/full frame when the asset working set is warm. The visible
multi-second hitches occur when the resident range cache collapses and must be
relearned from SD.

Current resident owner:

```text
owner = 21160 B
payload = 16384 B
range records = 256
large exact range = 2048 B
```

The cache currently sacrifices large exact ranges when small storage needs
headroom, then performs a global small working-set reset if saturation remains.
Hardware logs show a recurring pattern:

```text
entries approach ~229-254 / 256
 -> new view/dialog/action adds ranges
 -> 40-90+ physical reads
 -> range table/payload drops to a smaller rebuilt set
 -> next frames become fast again
```

This matches subjective gameplay: smooth/playable, sudden hitch, smooth again.

## Rejected experiments — do not repeat blindly

```text
b6bf8f64859612517054bad6b6e24849004f91f2
  incremental oldest-3/8 record eviction
  result: less predictable / subjectively worse
  reverted by efaa067fd9881a1c055b83eed4c80d5de0e33aad

4df43f6fe96f05bf012172f15afe2a069ee93494
  disable/release large 2048 B resident tail before gameplay
  result: regression; small table hits saturation more directly
  reverted by 9d69272ed8394c230d5eec79a2adc37dd44f3c04
```

Large resident ranges currently provide both plane acceleration and sacrificial
headroom. Do not remove them without a replacement policy.

## New performance-development strategy

The next branch should deliberately reverse the previous optimization order:

```text
first create a comfortably playable bounded configuration
 -> measure real RAM/DMA cost and stutter distribution
 -> identify what cache capacity/records are actually useful
 -> optimize downward afterward
```

A 24 KiB-class resident payload and/or additional range records is a reasonable
first A/B candidate, but it is not a predetermined final design.

Before accepting any larger permanent owner, add system-level witnesses:

```text
free heap
MALLOC_CAP_8BIT free + largest block
MALLOC_CAP_DMA free + largest block
lazy gameplay-owner floor
resident-cache owner delta
explicit reserve for future I2S/audio DMA
```

The audio reserve is part of the design budget now, not something to squeeze in
at the end.

## Profiling rule

Hot-path Serial/`printf` instrumentation has measurable cost and worsens the
subjective feel, but it does not fabricate dozens of physical PAK reads or
500-870 ms sprite phases.

Use detailed probes to compare configurations. Once a candidate is stable, run a
production-like firmware with hot-path profiling disabled/reduced before judging
final responsiveness.

Do not optimize `PlatformVideo_present()` first.

## Gameplay correctness frontier

Already hardware owned includes movement/turn/strafe, collision, native event
routing, dialogs/notes/messages, state ops 11/19/20, regular doors, mutable line
presentation, weapon pickup/painting/attack presentation, feedback lifetime and
adjacent extinguisher fire removal.

Still separate/deferred:

```text
generic monster/destructible combat
ammo/XP/sound/turn consequences
remote extinguisher miss/no-effect transaction
player-stat/inventory/ammo pickups
EV_CHANGEMAP / EV_GIVEMAP / EV_PASSWORD / EV_SAVEGAME / EV_CHECK_KEY
```

Do not mix these correctness families into the next cache/memory milestone.

## Development workflow

```text
recover true main + docs
 -> choose one bounded milestone
 -> instrument only what is needed
 -> commit/push agent/*
 -> test normal esp32-cyd on real CYD
 -> Serial is truth
 -> reject/revert regressions explicitly
 -> document only observed hardware results
 -> docs-only tail
 -> merge-ready
```

## After this branch merges

Re-read actual GitHub `main`, record its exact SHA, and create the next `agent/*`
branch from that SHA. Do not continue development from this locked profiling
branch by assumption.
