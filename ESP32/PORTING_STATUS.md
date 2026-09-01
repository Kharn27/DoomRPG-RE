# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = 8a0b74b51a55ea12fa8b35e444e69b972ef1fccb
branch = agent/esp32-resident-cache-ram-baseline
base main = 8a0b74b51a55ea12fa8b35e444e69b972ef1fccb
current code HEAD before docs = 684b2f52608f4a44cd28e0448f518f43bdb71012
current code tree = 6b0f32cb287001175f75d9423d6f091e9cb0ea5f
status = REAL-CYD RESIDENT CACHE RAM BASELINE COMPLETE
branch policy = LOCKED; docs-only tail only
```

`684b2f5...` is the hardware-tested code boundary selected after the RAM/cache
A/B sequence documented below. Do not add another cache-policy experiment to
this branch.

After merge, read the real GitHub `main` SHA again before creating the next
`agent/*` branch.

## Permanent architecture and hard invariants

```text
A NEW BSP IS NOT A NEW ENGINE.
```

Production path:

```text
/DoomRPG-ESP32.pak
 -> native parsers/catalog
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> native event/action/gameplay
 -> native renderer
```

Hard invariants:

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
backing     = /DoomRPG-ESP32.pak
```

Do not reintroduce a map-wide legacy texel pool, desktop pointer graph, or ZIP
runtime ownership for migrated map data. Cache sizes and record counts are NOT
hard architectural invariants; they are tunable bounded implementation choices.

## Entrance canonical witness

```text
resourceMapId = 1
resource = /intro.bsp
name = Entrance
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
runtime arena = 14095 B
runtimeFNV = c3882516
resident payload = 17891 B
spawn tile = 904
spawn direction = 64
spawn position = 544,1824,36
oldZ = 4
```

Owner fingerprints:

```text
mapStateFNV  = cd99b98e
scriptFNV    = f9e3d9df
lineFNV      = e5e74861
textureFNV   = f1fc1875
automapFNV   = 669b1aa7
topologyFNV  = 3f321e43
```

Cardinalities:

```text
nodes = 223
lines = 480
sprites = 344
events = 93
byteCodes = 265
strings = 94
native topology entities = 220
enemies = 30
destructibles = 13
```

Generic session baseline:

```text
targetMapId = 1
gameplayLoadMapId = 1
angle = 64
graphics textures = 33
graphics sprites = 45 -> 46 after dependency closure
catalog storage = 3120 B
catalog FNV = 29ffc14a
initial world frame = 71ca7465
HUD hp = 30/30
HUD armor = 0/20
HUD weapon = 2
HUD ammo = 8
```

Selected resident asset-cache baseline:

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

The range record stores `nameHash` and `relativeOffset` as 32-bit values and the
bounded `length` / payload `dataOffset` as 16-bit values. Compile-time assertions
keep the configured payload and large-range length representable. These numbers
are the selected implementation baseline, not permanent memory laws.

## Native gameplay boundary already hardware validated

```text
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
native collision/topology
SELECT event-first routing
EV_SHOW / EV_HIDE / EV_UNLOCK
EV_OPENLINE / EV_CLOSELINE
EV_DIALOG / EV_DIALOGNOBACK
EV_FORCEMESSAGE / EV_NOTE
state ops 11 / 19 / 20
regular door animation
mutable line texture variants
native idle weapon rendering
generic weapon attack frame 1 -> idle frame 0
weapon pickup eType=5
empty/human Action feedback (~1200 ms)
adjacent extinguisher fire removal with rollback-safe presentation
```

Still fail-closed/deferred:

```text
generic monster/destructible HP/damage combat
ammo/XP/sound/turn consequences
remote extinguisher miss/no-effect transaction
player-stat/inventory/ammo pickups
EV_CHANGEMAP / EV_GIVEMAP / EV_PASSWORD / EV_SAVEGAME / EV_CHECK_KEY
```

## World-render profiling milestone — hardware results

### 1. Plane renderer stack failure fixed

A clean real-CYD failure occurred after `[NATIVEPLANE]` returned but before its
caller resumed. `PlaneWork` was about 1.1 KiB of automatic stack inside an
already deep 9 KiB `loopTask` renderer path.

Permanent fix: `EspNativePlaneRenderer_render()` leases only `PlaneWork`
temporarily from heap and frees it before return. The six existing 2048 B plane
texture buffers remain temporary; no new persistent owner/BSS was added.

Hardware validation on the normal `esp32-cyd` firmware passed:

```text
old guard/retry turn
first regular door open 4/4
first regular door close 4/4
835 -> 834 former crash view
medkit/dialog open + close + DIALOGCHAIN RESUME
later guard-unlocked door
adjacent extinguisher fire clear
```

No stack canary/reboot. `ALIVE` heap returned to the expected baseline after
frames. This stack fix is accepted.

Do not solve renderer stack pressure by increasing `loopTask` blindly; the
transitional no-PSRAM boot heap is fragmentation-sensitive.

### 2. Plane cost is meaningful but not the worst stutter source

Observed gameplay plane phase is commonly about 70-150 ms depending on view and
physical 2048 B reads; some cold views are higher. Earlier profile samples showed
strong correlation between plane time and physical PAK reads.

`PlatformVideo_present()` remains around 34-35 ms and is not the primary target.

### 3. Sprite path can be either very warm or catastrophically cold

Same renderer, same map:

```text
warm sprite phase      ~= 6-26 ms, often 0-7 physical reads
cold/thrashing phase   ~= 500-870 ms, often 50-98 physical reads
```

The large fire room is therefore not intrinsically too heavy for the ESP32. It
can render around ~280-330 ms full-frame when its working set is warm, but an
asset-cache working-set collapse creates visible 0.7-2.1 s stalls.

### 4. Global working-set reset remains the central stutter witness

The resident small-range table/payload is a bounded working set. When it cannot
store another small range, current code first sacrifices transient large 2048 B
ranges when available; if still saturated, it resets the small range records and
payload globally.

Characteristic real-CYD witness:

```text
near saturation
 -> next view/presentation adds new exact ranges
 -> many physical reads / range stores
 -> table/payload collapses to a smaller freshly rebuilt working set
 -> following frame becomes fast again
```

The 19 KiB baseline proves both saturation dimensions can matter:

```text
payload-pressure witness:
  before first turn cache ~= 19096/19456 B, entries ~= 214/288
  turn sprites = 62 misses / 62 stores, ~= 547 ms sprite phase
  full frame ~= 2.0 s
  next move = 0 small misses, ~= 239 ms full frame

record-pressure witness:
  guard-unlocked door frame reaches 286/288 records
  following visibility expansion = 84 small stores / 90 physical reads
  rebuilt cache drops to ~= 82/288 records
  later door frames become warm again
```

This exactly matches the subjective pattern: generally fluid, isolated large
lag, then fluid again.

### 5. Previously rejected cache-policy experiments remain rejected

Do not blindly repeat:

```text
b6bf8f64859612517054bad6b6e24849004f91f2
  incremental FIFO-like eviction of oldest 3/8 records
  hardware result: less predictable / worse subjective stutter

efaa067fd9881a1c055b83eed4c80d5de0e33aad
  explicit revert back to global-reset baseline
  hardware result: user confirmed better playability

4df43f6fe96f05bf012172f15afe2a069ee93494
  release all large 2048 B resident ranges before gameplay
  hardware result: regression; more direct small-table saturation and large stalls

9d69272ed8394c230d5eec79a2adc37dd44f3c04
  explicit revert of large-off experiment
```

Large exact ranges are not only plane acceleration; they also provide sacrificial
headroom before a global small working-set reset. Any future replacement policy
must preserve the lessons from these rejected A/Bs.

## Resident-cache RAM baseline milestone — hardware results

### 1. RAM budget instrumentation

The branch records free heap, `MALLOC_CAP_8BIT`, largest 8-bit block, DMA free /
largest DMA, exact cache owner, observed heap delta, and lazy gameplay state.
Advisory reserves are deliberately not a fail gate.

Selected 19 KiB compact startup witness:

```text
CACHE_PRE  heap8=55100 largest8=36852
CACHE_POST heap8=27136 largest8=14324
owner      =23592
payload    =19456
records    =288
configured owner delta vs old 16KiB/256 main = +2432 B
observed heap8 delta = 27964 B
```

After cache priming and the lazy weapon owner:

```text
PRIMED / LAZY_PRE heap8 ~= 24476 largest8=14324
LAZY_POST          heap8 ~= 24476 largest8=14324
shapeData = NULL
mediaTexels = NULL
```

The difference between structural owner bytes and observed heap delta includes
live File/FS/allocator overhead; owner size alone must not be used as the full
system RAM cost.

### 2. 24 KiB / 384 records — rejected for renderer headroom

Candidate:

```text
payload = 24576 B
records = 384
owner = 31400 B
structural delta vs old main = +10240 B
```

Real CYD:

```text
CACHE_PRE  heap8=55100 largest8=36852
CACHE_POST heap8=19328 largest8=6900
observed heap8 delta=35772
```

The cold frame rendered, then the next warm plane reconstruction failed after
lazy gameplay ownership changed the heap topology. Session failed before gameplay
controls armed. Raw XPT2046 touch remained alive. This was a RAM/headroom failure,
not a touch bug. Candidate rejected.

### 3. 20 KiB / 320 records — smoother but rejected at dialog-chain snapshot

Candidate owner = 26280 B. General gameplay was materially smoother, but the
soldier dialog continuation exposed the next lazy owner:

```text
before topology snapshot heap8=21096 largest8=11764
[DIALOGCHAIN] TOPOLOGY-SNAPSHOT ownerBytes=2408
post-snapshot ALIVE heap8=18672
[NATIVEPLANE] FAILED textured floor/ceiling reconstruction
[DIALOGCHAIN] ROLLBACK ... exact=yes
second plane render failed
[RESIDENTGAMEPLAY] FAILED reason=dialog-resume-render-rollback
```

The 2408 B topology snapshot is rollback ownership for SHOW/HIDE and is not
optional scratch. Candidate rejected; do not hide this failure by weakening the
renderer or rollback semantics.

### 4. 18 KiB / 288 records — first full stable corpus

Owner = 23720 B. This candidate passed on the real CYD:

```text
regular movement/turn
first door animation
medkit/tutorial dialog + resume
soldier repeated dialog chain
2408 B topology snapshot + successful post-resume render
guard-unlocked door + 4-frame animation
entry into loaded fire room
two adjacent extinguisher fire clears
```

After the topology snapshot the steady witness was about:

```text
heap8=21232
largest8=14324
```

This proved the permanent-cache headroom boundary was below the rejected 20 KiB
layout and validated the strategy of retaining a larger-than-main bounded cache.

### 5. 19 KiB / 288 compact records — selected baseline

`ResidentRangeRecord` was compacted from 16 B to 12 B by bounding `length` and
`dataOffset` to 16 bits. With 288 records this recovers 1152 B of metadata. The
payload then grows from 18 KiB to 19 KiB while total owner shrinks by 128 B:

```text
18 KiB owner = 23720 B
19 KiB compact owner = 23592 B
payload gain = +1024 B
owner change = -128 B
records = 288 unchanged
```

Real-CYD validation passed the full discriminating corpus again:

```text
startup SMALL-COLD / SMALL-WARM / LARGE-LEARN / LARGE-WARM
first turn and regular door
medkit/tutorial dialog resume
soldier first SHOW/state continuation
2408 B topology snapshot + successful world render
remaining soldier dialog states
unlock + open guarded door
loaded room traversal
first and second extinguisher fire clears
```

Important steady RAM witnesses:

```text
early gameplay ALIVE heap8 ~= 24416 largest8=14324
after dialog-chain owner heap8 ~= 23784 largest8=14324
after 2408 B topology snapshot heap8 ~= 21360 largest8=14324
```

No renderer failure, rollback failure, stack canary, reboot, or loss of gameplay
controls occurred. `shapeData` and `mediaTexels` remained NULL.

Subjective hardware result: globally much smoother than the old 16 KiB baseline;
the loaded room is especially fluid, including fire clear attack/settle frames.
Examples from the selected run:

```text
loaded-room movement ~= 246-323 ms
first fire attack/settle ~= 302 / 294 ms
second fire attack/settle ~= 294 / 293 ms
warm sprite phases commonly ~= 7-21 ms
```

The remaining isolated stalls correlate with global cache recycle, not with
`PlatformVideo_present()`.

## Profiling interpretation

Hot-path Serial output has non-zero cost. `totalUs` is not a production-frame
benchmark because the gameplay-frame total timer spans diagnostic work as well.
However the sprite phase timer is captured immediately after
`EspNativeSpriteRenderer_render()` and before the `[SPRITEPROFILE]` `printf`, so
500-800 ms sprite phases and their simultaneous physical-read storms are real
renderer/storage work, not merely log printing.

A future production-like build should reduce hot-path diagnostics before judging
final responsiveness, but that is separate from the hardware validity of the
selected cache/RAM boundary.

## Memory / audio planning rule

The selected 19 KiB / 288 cache is a bounded implementation baseline, not an
invitation to consume all remaining heap. The no-PSRAM target still needs room
for future audio/I2S DMA, gameplay owners and allocator fragmentation.

Current advisory budget remains conservative and intentionally reports
`REVIEW_HEADROOM`; it is not a runtime gate. Do not interpret overlapping
8-bit/DMA capabilities as independent additive memory pools.

Any future cache-policy change should prefer better reuse of the existing owner
before increasing permanent RAM again.

## Next branch direction

This branch is locked. After it is merged:

1. read true GitHub `main` and exact SHA;
2. create a fresh `agent/*` branch from that SHA;
3. keep the selected `19 KiB / 288 / 12-byte record / 23592 B owner` baseline
   unchanged initially;
4. first make a production-like profiling A/B with hot diagnostics reduced enough
   to judge subjective responsiveness without changing renderer/cache semantics;
5. then attack the remaining global-reset cliff as its own bounded milestone,
   distinguishing payload pressure from record pressure;
6. do NOT simply reintroduce the rejected oldest-3/8 FIFO experiment or disable
   large ranges;
7. preserve exact fail-closed gameplay, dialog rollback and renderer headroom;
8. keep `shapeData == NULL`, `mediaTexels == NULL`, and `/DoomRPG-ESP32.pak` as
   the runtime backing store.

The next cache-policy milestone should optimize reuse/retention under the proven
RAM owner rather than increasing RAM first.
