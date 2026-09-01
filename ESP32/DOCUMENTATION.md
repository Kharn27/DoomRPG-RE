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
main = 8a0b74b51a55ea12fa8b35e444e69b972ef1fccb
branch = agent/esp32-resident-cache-ram-baseline
base main = 8a0b74b51a55ea12fa8b35e444e69b972ef1fccb
code boundary before docs = 684b2f52608f4a44cd28e0448f518f43bdb71012
code tree = 6b0f32cb287001175f75d9423d6f091e9cb0ea5f
status = resident-cache RAM baseline complete; branch locked
```

`684b2f5...` is the real-CYD validated code boundary. Only documentation commits
may follow it on this branch.

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
cache size. The selected implementation is currently `19 KiB / 288 records`, but
that is not a sacred invariant.

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

`PlaneWork` (~1.1 KiB) proved too large for a deep render path: a real CYD stack
canary occurred after `[NATIVEPLANE]` completed but before its caller resumed.

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

## Selected resident-cache baseline

Current hardware-selected implementation:

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

`ResidentRangeRecord` is compact:

```text
uint32_t nameHash
uint32_t relativeOffset
uint16_t length
uint16_t dataOffset
```

Compile-time assertions keep payload offsets and supported range lengths inside
16-bit bounds. This recovered 1152 B of metadata relative to 16-byte records;
1024 B was reinvested into payload while total owner shrank 128 B relative to the
hardware-stable 18 KiB / 288 candidate.

## Resident-cache A/B history

### 24 KiB / 384 — rejected

```text
owner = 31400 B
CACHE_PRE  heap8=55100 largest8=36852
CACHE_POST heap8=19328 largest8=6900
observed heap8 delta=35772
```

The first cold frame passed, but the next warm plane reconstruction failed after
lazy gameplay ownership. Gameplay controls never armed although raw touch still
worked. This is a renderer headroom failure, not an input failure.

### 20 KiB / 320 — rejected

General gameplay was noticeably smoother, but the soldier continuation exposed
lazy rollback ownership:

```text
before snapshot heap8=21096
TOPOLOGY-SNAPSHOT ownerBytes=2408
after snapshot heap8=18672
NATIVEPLANE FAILED
rollback exact=yes
second plane render FAILED
session FAILED dialog-resume-render-rollback
```

The topology snapshot is required for exact SHOW/HIDE rollback. Do not delete or
weaken it to make a larger cache fit.

### 18 KiB / 288 — first full stable corpus

Owner = 23720 B. Real CYD passed regular movement/doors, medkit/tutorial resume,
repeated soldier dialogue, 2408 B topology snapshot, guard-unlocked door, loaded
fire room and two fire clears. Post-snapshot steady heap8 was about 21232 B with
largest8 14324 B.

### 19 KiB / 288 compact — selected

Owner = 23592 B. Real CYD passed the same discriminating corpus again and kept:

```text
early gameplay heap8 ~= 24416, largest8=14324
after dialog-chain owner heap8 ~= 23784, largest8=14324
after topology snapshot heap8 ~= 21360, largest8=14324
```

No renderer failure, rollback failure, stack canary, reboot, or control loss.
`shapeData` and `mediaTexels` remained NULL.

Subjectively this is the best global result so far. The loaded room is notably
fluid, including adjacent extinguisher actions.

## World/storage profile conclusions

Current real-CYD evidence separates three costs:

```text
VIDEO present          ~= 34-35 ms
plane phase            commonly ~= 70-150 ms, view/read dependent
warm sprite phase      commonly ~= 6-26 ms
cold/thrashing sprites ~= 500-870 ms with tens of physical SD reads
```

The loaded fire room is not intrinsically beyond the ESP32. In the selected
19 KiB compact run, representative warm work included:

```text
loaded-room movement ~= 246-323 ms
first fire attack/settle ~= 302 / 294 ms
second fire attack/settle ~= 294 / 293 ms
```

The remaining visible multi-second hitches occur when the resident exact-range
working set globally resets and must be relearned from SD.

Two independent saturation modes are now hardware-visible:

```text
payload pressure:
  cache ~= 19096/19456 B with only ~=214/288 records
  first turn triggers 62 small stores / 62 misses
  sprite phase ~= 547 ms, full frame ~= 2.0 s

record pressure:
  guard-door visibility reaches 286/288 records
  next frame triggers 84 small stores / 90 physical reads
  rebuilt set drops to ~=82 records
```

So the next policy work must distinguish payload retention from record retention.
Do not assume one scalar capacity solves both.

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

Large resident ranges provide both plane acceleration and sacrificial headroom.
Do not remove them without a replacement policy.

## RAM / audio planning

Selected startup witness:

```text
CACHE_PRE  heap8=55100 largest8=36852
CACHE_POST heap8=27136 largest8=14324
structural owner = 23592 B
configured owner delta vs old main = +2432 B
observed heap8 delta = 27964 B
```

The structural owner does not include all File/FS/allocator live overhead. Use
system-level heap witnesses, not only `sizeof(owner)`.

The existing future audio/I2S/general reserves are advisory and intentionally
conservative. `MALLOC_CAP_DMA` overlaps ordinary 8-bit-capable memory; do not add
those pools as though they were independent.

Future cache-policy work should first improve reuse inside the selected owner
rather than increasing permanent RAM again.

## Profiling rule

Hot-path Serial/`printf` instrumentation has measurable cost. In particular,
`totalUs` spans diagnostic work and is not a production-frame benchmark.

However `[SPRITEPROFILE] us=...` is captured immediately after
`EspNativeSpriteRenderer_render()` and before that profile line is printed. The
observed 500-800 ms sprite phases that coincide with 50-90 physical reads are
therefore genuine renderer/storage stalls, not fabricated by the profile print.

Use detailed probes to compare configurations. A future production-like A/B
should reduce hot diagnostics while keeping the exact selected cache/renderer
semantics before judging final responsiveness.

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

Do not mix these correctness families into cache-policy work.

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
branch from that SHA.

Recommended next sequence:

```text
1. production-like profiling A/B, same 19 KiB / 288 / 23592 B owner
2. quantify subjective response with hot diagnostics reduced
3. separate bounded milestone for the global-reset cliff
4. distinguish payload-pressure and record-pressure cases
5. preserve large-range sacrificial behavior unless replaced deliberately
6. never reintroduce the rejected FIFO experiment by default
```

Do not continue development from this locked RAM-baseline branch by assumption.
