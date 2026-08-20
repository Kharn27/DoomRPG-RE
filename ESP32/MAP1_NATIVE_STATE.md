# ESP32 MAP_INTRO native mutable tile state

Branch: `agent/esp32-map1-native-state`

Base merged `main`:

```text
PR   = #44 — native compact MAP_INTRO access contract
main = ddcf19e6166f210a6f63fec1c608234ee3e253ea
```

Hardware-tested code:

```text
9a17654b56a190932615bba4894e90debd0e3773
```

Status: **REAL-CYD HARDWARE PASS; FIRST NATIVE MUTABLE WORLD STATE VALIDATED; MERGE-READY**.

## Objective

Create the first genuinely mutable world/spatial state owned by the ESP32-native engine, while keeping the hardware-proven compact BSP arena byte-for-byte immutable.

This milestone stops at a 32x32 tile-state overlay. It does **not** activate entities, monsters, player spawn, rendering or `ST_PLAYING`.

## Permanent ownership

Reusable native component:

```text
ESP32/include/esp_map_state.h
ESP32/src/esp_map_state.c
```

Temporary real-CYD validation scaffold:

```text
ESP32/include/native_map1_state_probe.h
ESP32/src/native_map1_state_probe.c
```

Permanent direction:

```text
immutable EspMapRuntime arena
        -> allocation-free accessors
        -> small mutable EspMapState
        -> later explicit entity/script/render overlays
```

`esp_map_state.c` depends on the native map-access API and ESP32 allocator only. It does not depend on `Render_t`, `DoomCanvas_t`, `Game_t`, `Node_t`, `Line_t` or `Sprite_t`.

## Why tile state is the first mutable consumer

The recovered desktop `Render_t` contains:

```text
byte mapFlags[1024]
```

This array is real gameplay/spatial state rather than pointer-heavy desktop baggage. It carries wall, secret, entrance, event and later visited/automap state.

Recovered bit contract:

```text
BIT_AM_WALL     = 1
BIT_AM_SECRET   = 2
BIT_AM_ENTRANCE = 4
BIT_AM_EVENTS   = 8
BIT_AM_VISITED  = 16
```

Native equivalents:

```text
ESP_MAP_TILE_WALL
ESP_MAP_TILE_SECRET
ESP_MAP_TILE_ENTRANCE
ESP_MAP_TILE_EVENTS
ESP_MAP_TILE_VISITED
```

## Exact persistent plan and measured cost

One separately owned mutable allocation:

```text
32 x 32 x uint8_t = 1024 B payload
```

Real classic-CYD result:

```text
heap8 before       = 70056
heap8 after        = 69016
actual heap use    = 1040 B
payload            = 1024 B
allocator overhead = 16 B
largest8           = 36852 -> 36852
```

The immutable native map arena remains separate:

```text
arena payload = 14095 B
arena heap    = 14112 B
arenaFNV      = c3882516
```

Combined native structural + mutable spatial cost is therefore:

```text
14112 B arena actual heap
+1040 B tile-state actual heap
------------------------------
15152 B actual persistent heap
```

Compared with the measured legacy pointer-heavy structural allocation:

```text
legacy structural = 55341 B
native arena+state = 15152 B
saved              = 40189 B
reduction          ~= 72.6%
```

This comparison is intentionally conservative: the native side now includes both immutable structure and the first mutable spatial state.

## Recovered initialization semantics

### 1. Block-map base

The 256-byte source block map stores four 2-bit cells per byte. The hardware-proven access layer exposes all 1024 logical values through `EspMapRuntime_getBlockCell()`.

Known and revalidated MAP_INTRO distribution:

```text
0 = 298
1 = 697
2 = 27
3 = 2
sum = 1024
```

`EspMapState_buildFromRuntime()` expands those values into the low two bits of all 1024 mutable tile bytes.

### 2. Entrance cells

The reference loader marks a tile with `BIT_AM_ENTRANCE` for each line whose texture ID is `7`.

Important recovered detail: the reference mutates line coordinates **before** calculating the entrance midpoint:

```text
if flags & 512:
    flags & 8  -> x1/x2 += 3
    flags & 16 -> x1/x2 -= 3
else if flags & 256:
    flags & 8  -> y1/y2 += 3
    flags & 16 -> y1/y2 -= 3
```

Then:

```text
x = (x1 + ((x2 - x1) / 2)) >> 6
y = (y1 + ((y2 - y1) / 2)) >> 6
tile = y * 32 + x
```

The immutable `EspMapRuntime_getLine()` correctly returns source coordinates without those mutations. `EspMapState` owns this recovered runtime semantic explicitly.

Hardware result:

```text
texture-7 line refs = 4
unique entrance cells = 4
first entrance tile = 68
```

All four recovered entrance references resolve in range and to distinct tiles.

### 3. Event-bearing cells

Recovered loader rule:

```text
if event & 0x01f80000:
    mapFlags[event & 1023] |= BIT_AM_EVENTS
```

The native state reproduces exactly that rule through `EspMapRuntime_getEvent()`.

Hardware result:

```text
source events             = 93
qualifying event refs     = 93
unique event-bearing cells = 93
first event tile          = 68
```

Every MAP_INTRO event qualifies for the recovered event-bearing-cell rule, and all 93 resolve to unique tile indexes.

Tile `68` is both an entrance and event-bearing tile. This is useful confirmation that `tileFlags` must be a composable bit field rather than an exclusive enum.

### 4. Visited is intentionally absent

`BIT_AM_VISITED` is **not** synthesized by this initial-state builder.

The reference applies visited semantics later during world/entity activation and save/load. Hardware validation confirmed:

```text
visited cells = 0
```

## Public native state API

```text
EspMapState_reset()
EspMapState_buildFromRuntime()
EspMapState_isReady()
EspMapState_view()
EspMapState_getTileFlags(index)
```

The raw mutable pointer is not exposed as a writable public buffer. Later state mutations should receive explicit APIs so state ownership remains controlled.

## Hardware validation probe

After the hardware-proven `MAPACCESS` stage completes, `MAPSTATEPROBE` arms and runs on the next Arduino service.

Validation sequence:

1. require the complete previous intro/BSP/runtime/access boundary;
2. recompute the resident arena FNV before state construction;
3. allocate/build exactly 1024 bytes of native tile state;
4. independently rebuild expected entrance tiles from every texture-7 line;
5. independently rebuild expected event-bearing tiles from every qualifying event;
6. walk all 1024 state bytes through the public API;
7. require low two bits to equal the proven block-map accessor value;
8. require entrance/event bits to match independently derived tile sets in both directions;
9. require `BIT_AM_VISITED == 0` and no unknown bits;
10. revalidate block distribution `298 / 697 / 27 / 2`;
11. verify out-of-range tile access fails;
12. compute FNV-1a over all 1024 state bytes;
13. require framebuffer and immutable arena FNV unchanged;
14. measure exact heap cost, allocator overhead and largest free block;
15. PARK before entities/gameplay.

The independent entrance/event expected sets use two 1024-bit stack bitsets (128 B each), not another persistent 1024-byte oracle.

## Real-CYD hardware result

Exact serial evidence:

```text
[MAPSTATE] READY bytes=1024 fnv=cd99b98e base=298/697/27/2 entranceRefs=4 entranceCells=4 eventRefs=93 eventCells=93 visited=0
[MAPSTATEPROBE] READY stateFNV=cd99b98e elapsed=9ms base=298/697/27/2 entrance=4/4 events=93/93 firstEntrance=68 firstEvent=68
[MAPSTATEPROBE] RAM heap8=70056->69016 used=1040 payload=1024 allocatorOverhead=16 largest8=36852->36852 frameFNV=7a95b5b5->7a95b5b5 arenaFNV=c3882516->c3882516
[MAPSTATEPROBE] PARK state=9 page=3 nativeArena=yes nativeTileState=yes tileBytes=1024 immutableArena=yes entities=0 monsters=0 noGameplay=yes
```

Canonical mutable-state fingerprint:

```text
stateFNV = cd99b98e
```

Build/verification time:

```text
9 ms
```

Inherited regressions also remained valid on this build:

```text
arenaFNV   = c3882516
decodedFNV = a426dd18
MAPACCESS full sweep = 4 ms
base       = 298 / 697 / 27 / 2
entities   = 0
monsters   = 0
ST_PLAYING = no
```

The small variation from the earlier 3 ms accessor sweep to 4 ms is normal runtime timing variation; the semantic fingerprint and zero-drift contract are unchanged.

Later heartbeats remained stable at:

```text
heap8    = 69016
largest8 = 36852
```

## Hardware gates — verdict

All planned gates passed:

```text
payload                  = 1024 B                    PASS
allocator overhead       = 16 B <= 64 B             PASS
largest8 after state     = 36852 >= 32768 B         PASS
base counts              = 298 / 697 / 27 / 2       PASS
entrance topology        = 4 refs / 4 cells          PASS
event topology           = 93 refs / 93 cells        PASS
visited cells            = 0                         PASS
state bounds             = fail closed               PASS
framebuffer              = unchanged                 PASS
arenaFNV                 = c3882516 unchanged        PASS
entities/monsters        = 0 / 0                     PASS
ST_PLAYING               = not entered               PASS
```

## Still forbidden

```text
legacy mapFlags ownership
entity/monster activation
entityDb[1024]
sprite/node linked lists
door mutation
visited/save-state mutation
player spawn
native gameplay renderer
ST_PLAYING
continuous gameplay loop
```

## Merge recommendation

**MERGE this branch.**

The hardware-affecting code tested on the real classic CYD is:

```text
9a17654b56a190932615bba4894e90debd0e3773
```

Any commits after that SHA should remain documentation-only until merge; if so, no additional flash is required.

## Next boundary after merge

The strongest next consumer signal now comes from the event topology:

```text
93 event-bearing tiles
93 unique event cells
```

A useful next milestone is therefore a **native tile -> event lookup/index layer** built over the immutable event records and/or a compact index overlay, so `EspMapState` can answer “this tile has an event” and native gameplay can resolve the corresponding event without desktop `tileEvents` ownership.

Other later candidates remain:

```text
- explicit visited-state API + save/automap semantics,
- minimal entity occupancy/index overlay,
- first native collision/movement query.
```

Do not recreate the desktop entity graph preemptively.
