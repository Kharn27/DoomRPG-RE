# ESP32 MAP_INTRO native mutable tile state

Branch: `agent/esp32-map1-native-state`

Base merged `main`:

```text
PR   = #44 — native compact MAP_INTRO access contract
main = ddcf19e6166f210a6f63fec1c608234ee3e253ea
```

Status: **IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS**.

## Objective

Create the first genuinely mutable world/spatial state owned by the ESP32-native engine, while keeping the hardware-proven compact BSP arena byte-for-byte immutable.

This milestone deliberately stops at a 32x32 tile-state overlay. It does **not** activate entities, monsters, player spawn, rendering or `ST_PLAYING`.

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

This array is not pointer-heavy object baggage. It is real gameplay/spatial state used for walls, secrets, entrances, event-bearing cells and later visited/automap state.

The recovered bit contract is:

```text
BIT_AM_WALL     = 1
BIT_AM_SECRET   = 2
BIT_AM_ENTRANCE = 4
BIT_AM_EVENTS   = 8
BIT_AM_VISITED  = 16
```

The native equivalents are:

```text
ESP_MAP_TILE_WALL
ESP_MAP_TILE_SECRET
ESP_MAP_TILE_ENTRANCE
ESP_MAP_TILE_EVENTS
ESP_MAP_TILE_VISITED
```

## Exact persistent plan

One separately owned mutable allocation:

```text
32 x 32 x uint8_t = 1024 B
```

The immutable native map arena remains:

```text
arena payload = 14095 B
arenaFNV      = c3882516
```

The new state is intentionally **not appended to the arena** because it has different lifecycle/ownership semantics: map source is immutable; tile/world state mutates during gameplay and save/load.

## Recovered initialization semantics

### 1. Block-map base

The 256-byte source block map stores four 2-bit cells per byte. The hardware-proven access layer already exposes all 1024 logical values through:

```text
EspMapRuntime_getBlockCell()
```

Known MAP_INTRO distribution from PR #44:

```text
0 = 298
1 = 697
2 = 27
3 = 2
sum = 1024
```

`EspMapState_buildFromRuntime()` expands those values directly into the low two bits of all 1024 mutable tile bytes.

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

The PR #44 immutable `EspMapRuntime_getLine()` correctly returns **source** coordinates without those mutations. `EspMapState` is the first appropriate consumer to reproduce this runtime semantic explicitly.

The builder fails closed if a recovered entrance midpoint falls outside the 32x32 map.

### 3. Event-bearing cells

The recovered loader marks:

```text
if event & 0x01f80000:
    mapFlags[event & 1023] |= BIT_AM_EVENTS
```

The native state reproduces exactly that rule through `EspMapRuntime_getEvent()`.

### 4. Visited is intentionally absent

`BIT_AM_VISITED` is **not** synthesized by this initial-state builder.

The reference marks entrance cells visited later during entity/world activation and also restores visited state from saves. Those are later gameplay/save-state semantics and must remain separate from initial spatial-state construction.

Hardware validation therefore requires:

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

## Temporary hardware probe

After the hardware-proven `MAPACCESS` stage completes, `MAPSTATEPROBE` arms and runs on the next Arduino service.

Validation sequence:

1. require the full previous intro/BSP/runtime/access boundary;
2. recompute the actual resident arena FNV before state construction;
3. allocate/build exactly 1024 bytes of native tile state;
4. independently rebuild the set of expected entrance tiles from every texture-7 line;
5. independently rebuild the set of expected event-bearing tiles from every qualifying event;
6. walk all 1024 state bytes via the public API;
7. require low two bits to equal the hardware-proven block-map accessor value;
8. require entrance/event bits to match the independently derived tile sets exactly in both directions;
9. require `BIT_AM_VISITED == 0` and no unknown bits;
10. verify the known block distribution `298 / 697 / 27 / 2`;
11. verify out-of-range tile access fails;
12. compute a canonical FNV-1a over the complete 1024-byte native tile state;
13. require framebuffer and immutable arena FNV to remain unchanged;
14. measure exact heap cost, allocator overhead and largest free block;
15. PARK before entities/gameplay.

The independent expected entrance/event sets use two 1024-bit stack bitsets (128 B each), not another 1024-byte persistent oracle.

## Memory gate

Planned persistent payload:

```text
1024 B
```

The hardware probe accepts at most:

```text
allocator overhead <= 64 B
largest8 after state >= 32768 B
```

These are conservative safety gates, not expected costs. The real CYD run will establish the actual allocator overhead and fragmentation result.

## Values to establish on hardware

The first run must establish:

```text
stateFNV
build elapsed ms
entrance texture-7 line refs
unique entrance cells
event refs with trigger mask
unique event cells
first entrance tile
first event tile
heap used / allocator overhead
largest8 after state
```

Inherited regressions must remain:

```text
arenaFNV   = c3882516
decodedFNV = a426dd18
base       = 298 / 697 / 27 / 2
entities   = 0
monsters   = 0
ST_PLAYING = no
```

## Expected log tail

```text
[MAPSTATEPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO mutable tile state ===
[MAPSTATEPROBE] CONTRACT ...
[MAPSTATE] READY bytes=1024 fnv=........ base=298/697/27/2 entranceRefs=... entranceCells=... eventRefs=... eventCells=... visited=0
[MAPSTATEPROBE] READY stateFNV=........ elapsed=...ms base=298/697/27/2 entrance=.../... events=.../... firstEntrance=... firstEvent=...
[MAPSTATEPROBE] RAM heap8=X->Y used=... payload=1024 allocatorOverhead=... largest8=A->B frameFNV=F->F arenaFNV=c3882516->c3882516
[MAPSTATEPROBE] PARK state=9 page=3 nativeArena=yes nativeTileState=yes tileBytes=1024 immutableArena=yes entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
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

## Next boundary after hardware PASS

If the 1 KiB native tile state passes on real hardware, it becomes the first proven mutable gameplay substrate. The next step should be chosen from a real consumer need, likely one of:

```text
- explicit visited-state API + automap semantics,
- event lookup/indexing by tile without desktop tileEvents ownership,
- minimal entity occupancy/index overlay,
- first native collision/movement query.
```

Do not pre-create the desktop entity graph merely because those consumers eventually need richer state.
