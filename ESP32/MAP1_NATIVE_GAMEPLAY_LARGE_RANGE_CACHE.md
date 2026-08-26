# MAP1 native gameplay shared-payload large range cache

## Status

```text
branch = agent/esp32-native-gameplay-large-range-cache
base main = 377fce3de5381373750a7fba29d0c83b8142c583
base PR = #96 — persistent gameplay render resource cache
hardware-tested implementation SHA = 1273f0205c0ba060972500bedd76effc974077bf
status = REAL-CYD HARDWARE PASS
merge-ready = yes after docs-only closeout audit
```

This milestone bounds part of the remaining exact `2048 B` world-texture traffic inside the already-paid `16 KiB` resident render-resource payload, then fixes the first renderer boundary exposed by unrestricted native roaming without restoring legacy `mediaTexels`.

The final real-CYD run successfully traversed the previously failing region and the user reports being able to roam throughout the level without renderer failure or reboot.

## Permanent cache boundary

The predecessor resident owner remains the one permanent allocation:

```text
ResidentCache owner struct = 21160 B
payload capacity            = 16384 B
range record capacity       = 256
entry descriptor slots      = 24
small exact range max       = 1024 B
```

This milestone adds an opt-in exact `2048 B` class using only free bytes at the high end of the same payload:

```text
small ranges grow upward
large 2048 B ranges grow downward
small ranges keep priority
small-range pressure may evict the lowest large range
no second permanent heap owner
exact key = nameHash + relativeOffset + length
```

The public opt-in boundary is:

```text
EspAssetPack_residentLargeRangeBegin()
EspAssetPack_residentLargeRangeEnd()
EspAssetPack_isResidentLargeRangeEnabled()
```

The existing `<=1024 B` cache behavior remains active and unchanged before large-range activation.

## Real-CYD canonical large-cache proof

The predecessor cache was first revalidated unchanged in the same firmware:

```text
owner struct=21160 B
cache=9225/16384 B
entries=195/256
large=off
```

The free tail was therefore `7159 B`, enough for exactly three `2048 B` ranges while leaving `1015 B` separation from the small-range frontier.

Hardware observed exactly that topology:

```text
[LARGECACHE] BASE
sdReads=22
sdBytes=45056
cache=9225/16384 B
entries=195/256
large=off
owner=21160 B

[LARGECACHE] LEARN
slots=3
sdReads=22
sdBytes=45056
range=350H/22M/3S/19B
cache=15369/16384 B
entries=198/256
large=3
exact=yes

[LARGECACHE] WARM
slots=3
sdReads=19
sdBytes=38912
range=353H/19M/0S/19B
cache=15369/16384 B
entries=198/256
large=3
exact=yes

[LARGECACHE] READY
savedReads=3
savedBytes=6144
ownerDelta=0
heapStable=yes
frame=ba3e5182
viewport=9206eb24
hud=6c2aa46f
shapeData=NULL
mediaTexels=NULL
```

The warm canonical North frame improved from roughly `260.8 ms` during the learn pass to `232.9 ms` once all three retained `2048 B` ranges were hot, saving exactly three physical SD reads / `6144 B`.

This is a useful but intentionally modest gain. The user reported that movement was playable but did not perceive a dramatic difference from this cache alone, which is consistent with the fixed touch feedback/restore presents and other renderer costs still dominating perceived action latency.

## Renderer failure exposed by wider roaming

With movement now usable enough to leave the original fixed-pose neighborhood, one North movement from:

```text
position = 992,1632
candidate = 992,1568
angle = 64 / North
```

failed closed after the plane pass and before completed walls/sprites/HUD.

The minimal BSS-only diagnostic later proved the exact failure:

```text
[NATIVEFRAME] FAILED route=gameplay
code=3/SPAN_OOB
view=992,1568,36
angle=64
line=90
logical=66
actual=140
flags=00002000
source=98304
packedIndex=2048
```

This is decisive:

- the bounded native wall block has legal packed indices `0..2047`;
- the first illegal access is exactly `2048`, not an arbitrary runaway index;
- `flags=0x00002000`, so this witness is **not** the legacy `0x00010000` double-height wall case;
- the original renderer's map-wide compact `mediaTexels` layout allowed index `2048` to fall into the first byte of the next admitted packed wall block.

The earlier speculative double-height continuation patch was therefore the wrong abstraction. It also caused a real loopTask stack-canary panic on hardware and was fully reverted before this final solution.

## Permanent legacy compact guard recovery

The final implementation preserves the native bounded architecture and reproduces only the one legacy byte actually required at the block boundary.

A tiny static owner records:

```text
LegacyWallGuard = 16 B BSS
logicalId
actualId
successorActualId
packedByte
valid
sourceOffset
successorSourceOffset
```

Important stack rule:

```text
no new 2048 B buffer
no field added to FirstFrameWork
no PAK read from sampleWallSpan()
no nested successor-search helper in the raster recursion
```

Recovery flow:

```text
bounded sampler detects exact packedIndex=2048
 -> renderer fails closed normally
 -> BSP/raster stack fully unwinds
 -> outside renderFrame(), resolve legacy compact successor from mappings.bin
 -> read one packed byte from wtexels.bin
 -> store 16 B guard in BSS
 -> retry the viewport frame
 -> sampler accepts only exact index 2048 for the matching source
```

Any unrelated OOB, `2049+`, missing successor, bad mapping, or failed read remains fail closed.

The successor is recovered by the same legacy compact ordering principle used by the desktop loader: actual wall blocks admitted for the map are ordered by source texel offset and packed contiguously. The native implementation does not recreate a map-wide `mediaTexels` allocation.

## Real-CYD recovery proof

The final roaming run contains an explicit second real boundary witness:

```text
[NATIVEFRAME] LEGACY_GUARD
logical=15
actual=40
source=20480
successorActual=108
successorSource=61440
byte=aa
owner=BSS
bytes=16

[NATIVEFRAME] RETRY legacy compact guard after unwound SPAN_OOB
line=33
actual=40

[NATIVEFRAME] BSP ...
[NATIVEFRAME] WALL requests=12 draws=12 spans=162 pixels=6373 ...

[NATIVEFRAME] RECOVERED legacy compact guard
actual=40
successorActual=108
source=20480->61440
```

The movement then committed normally:

```text
position 992,1696 -> 992,1760
tile 847 -> 879
frame ece49fb2 -> ea36efee
legacyStable=yes
residentStable=yes
orientationStable=yes
turnAdvance=no
tileDispatch=no
facingRefresh=deferred
```

The user then continued moving and rotating throughout the level, including traversing the previously problematic corridor in both directions, with no `FAILED`, Guru Meditation, reboot, heap drift, or resident-state loss reported.

The Serial excerpt is necessarily partial because the full roaming run produced more output than was retained, so the milestone does **not** invent missing per-position fingerprints. The hardware claim is limited to the explicit witnesses plus the user's real-device statement that the whole level became roamable.

## RAM and stack proof

Final stable real-CYD witness:

```text
heap8=39756
largest8=14836
stackHighWater=924
execScratch MOVE=540 B
execScratch TURN=484 B
```

Immediately before adding the guard, the corresponding stable heap witness was `39772 B`. The exact `16 B` reduction matches `sizeof(LegacyWallGuard)` and confirms the final repair uses static DRAM rather than another heap allocation.

Across the long roaming sequence:

```text
heap=105424 stable
heap8=39756 stable
largest8=14836 stable
legacyStable=yes
residentStable=yes
shapeData=NULL
mediaTexels=NULL
```

This is also safely separated from the earlier failed speculative patch, whose real hardware stack-canary panic proved that the renderer stack could not tolerate extra deep automatic state.

## Orientation / start-door visual anomaly remains open

The user's roaming run still reports a visual anomaly around the start/arrival door:

- backing away while facing away from the door can visually look like an unexpected half-turn;
- advancing while facing the door can produce a similarly confusing view.

The current semantic orientation owner remains internally coherent in Serial:

```text
0   = East
64  = North
128 = West
192 = South
```

MOVE preserves orientation and TURN changes it by exactly `+/-64`; repeated round trips still recover canonical fingerprints. Therefore this milestone does **not** mutate orientation semantics to mask the visual issue.

Treat the door anomaly as a separate renderer/geometry/culling investigation. It is explicitly outside this cache/guard milestone and should get its own hardware probe before any correction.

## Remaining performance caveat

The one-byte guard resolver performs small immutable PAK reads only after an exact boundary failure has unwound. Small resident ranges retain priority, so those reads are memory-safe even if they evict one or more large tail ranges.

The final roaming run proves functional stability after recovery, but it does not separately re-measure whether all three large `2048 B` slots remain resident after every guard resolution. If this matters in profiling, a later optimization may add a deliberate uncached metadata-read path rather than changing the now hardware-proven renderer semantics.

Do not change this in the closeout commit: code after the tested SHA would require another hardware run.

## Side-effect boundary

Still intentionally absent:

```text
Game_advanceTurn
Game_executeTile
Game_eventFlagsForMovement
post-move tile-event execution
dynamic opened-line relinking
entity/monster activation and AI
facing refresh after actions
weapon overlay
runtime ZIP map/graphics access
legacy Game.entities population
legacy Game.monsters population
```

Mandatory invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
/DoomRPG-ESP32.pak = native backing store
```

## Merge boundary

Hardware-tested code SHA:

```text
1273f0205c0ba060972500bedd76effc974077bf
```

All commits after this SHA must be documentation-only for this PASS to remain valid.

After merge, recover the exact new `main` SHA before creating another `agent/*` branch.

Recommended next investigation: a small renderer/view witness around the start-door visual anomaly, keeping semantic orientation untouched until the hardware logs prove exactly where renderer and gameplay ownership diverge.
