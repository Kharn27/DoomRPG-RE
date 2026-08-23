# ESP32 native reversible resident handoff milestone

Branch: `agent/esp32-native-resident-handoff`

Base merged `main`:

```text
PR   = #66 — native transition preflight
main = 9f981f490282200f216aef66d22608d2244beb00
```

Firmware candidate:

```text
f71520281254ff9d0b2d5e4be1b3611e29ca87c4
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

The target `/junction.bsp` is already hardware-proven readable, CRC-valid and structurally bounded while Entrance remains resident. The next boundary is therefore not another read-only preflight: it is proving that the current native resident owners can be torn down and rebuilt in a dependency-safe, RAM-safe order.

This milestone introduces a permanent generic resident-map lifecycle and a temporary destructive-but-reversible hardware proof:

```text
Entrance resident
 -> explicit native resetAll()
 -> EMPTY
 -> build Junction resident runtime + all current mutable owners
 -> capture Junction
 -> explicit resetAll()
 -> EMPTY
 -> rebuild Entrance
 -> require exact source restoration
```

The test does **not** call legacy `DoomCanvas_loadMap()`, does not mutate `Game/Menu/Player/Render`, does not enter `ST_PLAYING`, does not commit Junction as the active gameplay map, and leaves Entrance resident at PARK.

## Permanent resident lifecycle

Files:

```text
ESP32/include/esp_map_resident_lifecycle.h
ESP32/src/esp_map_resident_lifecycle.c
```

Permanent API:

```text
EspMapResidentLifecycle_resetAll()
EspMapResidentLifecycle_isEmpty()
EspMapResidentLifecycle_isReady()
EspMapResidentLifecycle_capture()
EspMapResidentLifecycle_loadFromEmpty()
```

### Explicit teardown rule

`loadFromEmpty()` never hides a destructive source teardown. If any current owner is live it returns `ESP_MAP_RESIDENT_NOT_EMPTY` before opening the PAK.

The only destructive primitive is explicit:

```text
EspMapResidentLifecycle_resetAll()
```

Dependency-safe release order:

```text
sprite topology
 -> automap state
 -> line texture state
 -> line state
 -> script state
 -> map/tile state
 -> immutable runtime arena
```

Every owner already has its own real `reset()` and heap ownership. No mutable overlay owns the immutable arena.

### Empty-only build

`EspMapResidentLifecycle_loadFromEmpty(resourceName, inventory, snapshot)` requires:

```text
all seven owners empty
PAK closed on entry
valid pre-inventoried BSP
```

It then owns one temporary PAK session and builds:

```text
EspMapRuntime
EspMapState
EspMapScriptState
EspMapLineState
EspMapLineTextureState
EspMapAutomapState
EspMapSpriteTopology + /entities.db
```

The PAK is closed before return. On any failure the lifecycle is reset back to EMPTY and the result snapshot is zeroed.

`PACK_BUSY` does not steal or close a caller-owned PAK session.

## Pointer-free resident snapshot

```text
EspMapResidentSnapshot = 96 B
```

It contains only scalar sizes, FNVs and cardinalities:

```text
runtimeArenaBytes
mapStateBytes
scriptStateBytes
lineStateBytes
textureStateBytes
automapStateBytes
topologyBytes
totalPayloadBytes

runtimeFNV1a
mapStateFNV1a
scriptStateFNV1a
lineStateFNV1a
textureStateFNV1a
automapStateFNV1a
topologyFNV1a

nodeCount
lineCount
spriteCount
eventCount
byteCodeCount
stringCount
entityCount
enemyCount
destructibleCount
```

No pointer or legacy object is retained in the snapshot.

## Source Entrance snapshot target

From the hardware-proven current owners and their exact storage formulas:

```text
runtime arena       14112 B
map state            1024 B
script state           81 B
line state            120 B
texture state          60 B
automap state         103 B
topology              2408 B
---------------------------
logical payload      17908 B
```

Existing hardware actual allocation:

```text
18008 B
```

Therefore expected allocator overhead released by `resetAll()`:

```text
100 B
```

Deterministic source snapshot target:

```text
snapshotBytes = 96
snapshotFNV   = 97090c81

arenaFNV      = c3882516
mapStateFNV   = cd99b98e
scriptFNV     = f9e3d9df
lineFNV       = e5e74861
textureFNV    = f1fc1875
automapFNV    = 669b1aa7
topologyFNV   = 3f321e43

nodes=223 lines=480 sprites=344 events=93 byteCodes=265 strings=94
entities=220 enemies=30 destructibles=13
```

The 96-byte whole-snapshot FNV is a static prediction; the real CYD remains authoritative.

## Junction resident payload prediction

The preflight has already hardware-proven the immutable Junction plan and source counts. Applying the current generic mutable-owner storage formulas gives:

```text
runtime arena       8867 B
map state           1024 B
script state          73 B   # ceil(66/2) + ceil(319/8)
line state            52 B   # 2 * ceil(207/8)
texture state         26 B   # ceil(207/8)
automap state         32 B   # ceil(207/8)+ceil(48/8)
topology              336 B  # 48 * 7
--------------------------
logical payload     10410 B
```

These payload sizes are format-derived expectations. The following remain intentionally unknown until hardware:

```text
actual Junction heap allocation cost
allocator overhead
runtime/map/script/line/texture/automap/topology FNVs
whole Junction snapshot FNV
entity/enemy/destructible counts
largest-block behavior through the round trip
build elapsed time
```

## Temporary reversible handoff probe

Files:

```text
ESP32/include/native_resident_handoff_probe.h
ESP32/src/native_resident_handoff_probe.c
```

The probe arms only after the corrected transition-preflight probe has completed.

### Phase 0 — source and fail-closed gates

Before any teardown it requires current Entrance to match the exact source snapshot and proves:

```text
loadFromEmpty while Entrance live -> NOT_EMPTY
result zero
Entrance snapshot unchanged
PAK never opened

NULL resource -> INVALID + zero result
capture(NULL) -> refused
```

### Phase 1 — explicit source release

The probe inventories `/intro.bsp` and `/junction.bsp` first, then performs the destructive call:

```text
EspMapResidentLifecycle_resetAll()
```

Acceptance at `EMPTY1`:

```text
all owners empty
capture refused + zero output
heapEmpty1 - heapSource = 18008 B
logical source payload = 17908 B
allocator overhead = 100 B
```

### Phase 2 — PAK ownership gate

While EMPTY, the probe deliberately opens the PAK itself and calls `loadFromEmpty()`.

Required:

```text
PACK_BUSY
result zero
caller still owns the open PAK
caller closes it explicitly
heap/largest return exactly to EMPTY1
```

### Phase 3 — temporary Junction residency

The permanent lifecycle builds full Junction native residency from the pre-inventoried target.

Expected shape:

```text
snapshotBytes=96
payload=10410
arena=8867
state=1024
script=73
line=52
texture=26
automap=32
topology=336

nodes=77
lines=207
sprites=48
events=66
byteCodes=319
strings=126
```

All seven Junction FNVs must be non-zero. Topology counts must be sane and will be established by hardware.

The probe captures the target twice without rebuilding and requires byte-exact equality.

### Phase 4 — target release / fragmentation gate

After releasing Junction:

```text
heapEmpty2    == heapEmpty1
largestEmpty2 == largestEmpty1
all owners empty
```

This deliberately makes allocator fragmentation visible before a committed map swap is allowed.

### Phase 5 — exact Entrance restoration

The source is rebuilt through the same permanent `loadFromEmpty()` API.

Acceptance:

```text
restored snapshot byte-identical to source
restored snapshotFNV=97090c81
heapRestored == heapSource
largestRestored == largestSource
PAK closed
```

The probe then requires unchanged framebuffer, legacy Player witness and legacy transition/menu witness.

### Recovery behavior

Any failure after source release enters the probe recovery path:

```text
close PAK if needed
reset all native resident owners
attempt loadFromEmpty(/intro.bsp)
print [RESIDENTHANDOFF] RECOVERY ...
```

A failed target proof is therefore not intentionally allowed to strand the board with an empty native runtime.

## Expected Serial family

```text
[RESIDENTHANDOFFPROBE] ARMED ...

=== Doom RPG ESP32-native reversible resident handoff ===
[RESIDENTHANDOFFPROBE] CONTRACT ...

[BSPREAD] ... /intro.bsp ...
[BSPREAD] ... /junction.bsp ...

# Junction build uses the existing permanent builders:
[MAPRT] ARENA ...
[MAPRT] READY ...
[MAPSTATE] READY ...
[MAPLINESTATE] READY ...
[MAPLINETEX] READY ...
[MAPAUTOMAP] READY ...

# Entrance restoration prints the same builder families again.

[RESIDENTHANDOFF] SOURCE snapshotBytes=96 payload=17908 snapshotFNV=97090c81 ...
[RESIDENTHANDOFF] EMPTY1 ... released=18008 sourcePayload=17908 allocatorOverhead=100 allOwnersEmpty=yes
[RESIDENTHANDOFF] GATES notEmpty=1 invalid=1 nullCapture=1 packBusy=1 busyZero=1 callerOwnsPack=1 emptyAtomic=yes
[RESIDENTHANDOFF] JUNCTION snapshotBytes=96 payload=10410 heapCost=... allocatorOverhead=... snapshotFNV=... elapsed=...ms arena=8867 state=1024 script=73 line=52 texture=26 automap=32 topology=336
[RESIDENTHANDOFF] JUNCTIONFNV arena=... map=... script=... line=... texture=... automap=... topology=...
[RESIDENTHANDOFF] JUNCTIONTOPO nodes=77 lines=207 sprites=48 events=66 byteCodes=319 strings=126 entities=... enemies=... destructibles=... heap8=... largest8=...
[RESIDENTHANDOFF] EMPTY2 ... emptyExact=1 ... fragmentationDelta=0
[RESIDENTHANDOFF] RESTORE snapshotFNV=97090c81 exact=1 ...
[RESIDENTHANDOFF] RAM ... finalDelta=0 ...
[RESIDENTHANDOFF] LEGACY ... DoomCanvas_loadMapCalled=no menuMutation=no legacyPlayerMutation=no
[RESIDENTHANDOFF] PARK state=9 page=3 nativeResidentLifecycle=yes reversibleHandoff=yes junctionResidentProven=yes sourceRestored=yes targetLeftResident=no packClosed=yes persistentBytes=18008 mapSwapCommitted=no entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

Use normal PlatformIO environment `esp32-cyd`.

No CI status is published for the firmware candidate. No local build or hardware PASS is claimed.

## Boundary after PASS

A hardware PASS would prove for the first time that `/junction.bsp` can exist as the **complete current native resident owner set**, not merely as a read-only inventory, while also proving the source can be restored exactly with no final heap/largest-block drift.

Still outside this milestone:

```text
committing Junction as the active map after the stats-menu pause
native transition state machine / point-of-no-return ownership
spawn placement and loadType handoff
actual stats-menu presentation/input
full entity/monster gameplay beyond compact topology
ST_PLAYING progression
native gameplay renderer
sound playback
```

The next milestone after a PASS should use this permanent lifecycle to own a bounded **committed transition state machine** rather than reimplementing teardown/build ordering.

The real classic CYD Serial log is the final hardware source of truth.
