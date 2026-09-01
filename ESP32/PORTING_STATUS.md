# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = fe69c2fbcaed874e2cf35828c326d8c8b3465375
branch = agent/esp32-world-render-profile
base main = fe69c2fbcaed874e2cf35828c326d8c8b3465375
current code HEAD before docs = 9d69272ed8394c230d5eec79a2adc37dd44f3c04
current code tree = 181e084bc8abdea1ebc258a5b411e2b60797b0ce
hardware-tested equivalent baseline = efaa067fd9881a1c055b83eed4c80d5de0e33aad
status = REAL-CYD HARDWARE PROFILE COMPLETE
branch policy = LOCKED; docs-only tail only
```

`9d69272...` is the explicit revert of the rejected `large=off` experiment and
has the exact same tree as the previously hardware-tested `efaa067...` baseline.
Do not add another performance experiment to this branch.

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

Current resident asset-cache implementation:

```text
owner = 21160 B
payload = 16384 B
range records = 256
large range size = 2048 B
normal priming learns ~= 2 large ranges
```

These cache numbers are the current implementation, not permanent memory laws.

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

Observed gameplay plane phase is commonly about 80-150 ms depending on view and
physical 2048 B reads; some heavier/cold views approach ~190 ms. Earlier profile
samples showed strong correlation between plane time and physical PAK reads.

`PlatformVideo_present()` remains around 34-35 ms and is not the primary target.

### 3. Sprite path can be either very warm or catastrophically cold

Same renderer, same room:

```text
warm sprite phase      ~= 6-26 ms, often 0-7 physical reads
cold/thrashing phase   ~= 500-870 ms, often 50-98 physical reads
```

The large fire room is therefore not intrinsically too heavy for the ESP32. It
can render around ~280-330 ms full-frame when its working set is warm, but an
asset-cache working-set collapse creates visible 0.7-2.1 s stalls.

### 4. Current cache reset is the central performance witness

The resident small-range table/payload is a bounded working set. When it cannot
store another small range, current code first sacrifices transient large 2048 B
ranges when available; if still saturated, it resets the small range records and
payload globally.

Real-CYD witnesses show the characteristic cliff:

```text
near saturation: entries ~= 229-254 / 256
 -> next view/presentation adds new ranges
 -> many physical reads (often 40-90+)
 -> entries/bytes collapse to a smaller freshly rebuilt working set
 -> following frame becomes fast again
```

This exactly matches the subjective pattern: playable -> sudden large lag ->
playable again.

### 5. Rejected cache experiments

Rejected and explicitly reverted:

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
  tree exactly equals efaa067... tested baseline
```

Important lesson: current large 2048 B ranges are not only plane acceleration;
they also act as sacrificial headroom before the global small working-set reset.
Do not remove them casually.

## Memory / audio planning rule for next milestone

Do not confuse current 16 KiB cache size with an invariant. The next performance
branch may deliberately test a larger bounded resident owner (for example a
24 KiB-class payload and/or more range records) if it improves gameplay stability.

But do this as a system RAM budget, not as an unbounded cache increase.
Before accepting a larger permanent owner, instrument and record at least:

```text
free heap
MALLOC_CAP_8BIT free + largest block
MALLOC_CAP_DMA free + largest block
steady gameplay lazy-owner floor
candidate resident-cache owner delta
explicit reserved headroom for future I2S/audio DMA
```

Goal: first put the game in a stable, reasonably fluid condition, then profile
which bytes/records are genuinely useful and optimize downward. This is preferred
over repeatedly forcing a tiny cache to thrash while trying to infer the final
optimal policy from noisy debug runs.

## Probe / production-like test rule

Current profiling `printf`/Serial output has non-zero cost and inflates subjective
latency, but it does not explain 50-98 physical SD reads or 500-870 ms sprite
phases. Storage thrash is real.

Once a candidate memory/cache configuration is stable, run a second
production-like firmware with hot-path performance probes disabled or strongly
reduced before judging final responsiveness.

## Next branch direction

This branch is locked. After it is merged:

1. read true GitHub `main` and exact SHA;
2. create a fresh `agent/*` branch from that SHA;
3. establish RAM/DMA/audio reserve witnesses;
4. test a deliberately more generous but bounded resident cache (24 KiB-class is
   a reasonable first A/B, not a predetermined final value);
5. test Entrance normal route, medkit/dialog, guard door and loaded fire room;
6. compare stutter distribution and physical reads, not only average frame time;
7. only after stable play, optimize cache size/record representation/policy down.

Do not continue cache-policy experiments on `agent/esp32-world-render-profile`.
