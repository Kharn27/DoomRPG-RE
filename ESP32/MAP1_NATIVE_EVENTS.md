# ESP32 MAP_INTRO native tile event lookup

Branch: `agent/esp32-map1-native-events`

Base merged `main`:

```text
PR   = #45 — first native mutable MAP_INTRO tile state
main = feec8a7fcb839dbd9f6de708f56f26b69a1e79e9
```

Status: **IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS**.

## Objective

Let native gameplay resolve `tile -> event record` without recreating desktop `Render.tileEvents` ownership, without allocating a 1024-entry index, and without executing bytecode yet.

The already proven native layers remain:

```text
EspMapRuntime immutable arena
    14095 B payload / 14112 B actual heap
    arenaFNV = c3882516

EspMapState mutable tile flags
    1024 B payload / 1040 B actual heap
    stateFNV = cd99b98e
```

This milestone adds **0 persistent bytes**.

## Recovered reference behavior

The desktop loader stores each event as one 32-bit value. The tile key is:

```text
eventTile = event & 1023
```

The legacy `Render_findEventIndex()` performs a binary search over the event array using that low-10-bit key. That proves the event array is intended to be ordered by tile.

MAP_INTRO hardware evidence from PR #45 already established:

```text
source events          = 93
qualifying event refs  = 93
unique event cells     = 93
first event tile       = 68
first event value      = 00080044
```

Tile 68 is also an entrance tile.

## Architecture decision: no duplicate index

A tempting implementation would allocate something like:

```text
uint8/uint16 eventIndexByTile[1024]
```

That would spend another 1–2 KiB to index only 93 event-bearing cells.

Instead the native engine keeps the 372-byte compact event section already resident in `EspMapRuntime` and performs a lower-bound binary search directly through `EspMapRuntime_getEvent()`.

With 93 records, a hit/miss requires only about seven comparisons in the worst case.

Permanent component:

```text
ESP32/include/esp_map_events.h
ESP32/src/esp_map_events.c
```

Public contract:

```text
EspMapEvents_findByTile(tileIndex, &eventRef)
```

Returned value:

```text
EspMapEventRef
    index      uint16
    tileIndex  uint16
    value      uint32
```

Bounds:

```text
valid tile = 0..1023
invalid tile / NULL output / missing runtime -> false
```

The search uses deterministic `lower_bound` semantics. If a future BSP contains duplicate event tiles, it returns the first matching record. MAP_INTRO itself is expected to remain strictly unique and the hardware probe validates that property.

## Why this belongs outside `EspMapState`

`EspMapState` owns mutable spatial flags such as `EVENTS`; it should not duplicate immutable event payloads or indices.

The intended flow is:

```text
EspMapState_getTileFlags(tile)
    -> EVENTS bit says whether an event exists
    -> EspMapEvents_findByTile(tile)
    -> immutable raw event record + compact index
```

Later code may decode/execute the returned event, but execution is deliberately outside this milestone.

## Temporary hardware probe

Validation scaffold:

```text
ESP32/include/native_map1_events_probe.h
ESP32/src/native_map1_events_probe.c
```

It runs only after the full PR #45 tile-state probe is done.

The real-CYD probe must:

1. require inherited `arenaFNV=c3882516` and `stateFNV=cd99b98e`;
2. scan all 93 source events and require strictly increasing tile keys;
3. require all MAP_INTRO events to retain the recovered trigger mask used by tile-state construction;
4. require first event = tile 68 / index 0 / value `00080044`;
5. query **all 1024 tiles** through `EspMapEvents_findByTile()`;
6. compare every result against an independent sequential event cursor;
7. require `EspMapState.EVENTS` to match lookup existence for all 1024 tiles;
8. require exactly 93 hits and 931 misses;
9. require tile 1024 and NULL-output queries to fail closed;
10. compute a canonical `lookupFNV` across all 1024 lookup results;
11. require zero heap and largest-block drift;
12. require framebuffer, immutable arena and mutable tile state all byte-for-byte unchanged;
13. PARK before bytecode execution, entities, rendering or `ST_PLAYING`.

## Expected inherited boundary

Build-to-build heap baseline may move with code size, but before this probe the runtime must still contain:

```text
arena payload        = 14095 B
arena actual heap    = 14112 B
arenaFNV             = c3882516

tile-state payload   = 1024 B
tile-state heap      = 1040 B
stateFNV             = cd99b98e

combined native heap = 15152 B
largest8             = 36852 on PR #45 hardware run
entities/monsters    = 0 / 0
ST_PLAYING           = no
```

The new event lookup itself must consume:

```text
persistent bytes = 0
heap drift       = 0
```

## Expected new log tail

```text
[MAPEVENTPROBE] ARMED ...

=== Doom RPG ESP32-native MAP_INTRO tile event lookup ===
[MAPEVENTPROBE] CONTRACT binary search directly over compact immutable event records; 0 persistent bytes; no bytecode execution, entities, rendering or gameplay
[MAPEVENTS] READY events=93 firstTile=68 lastTile=... sortedUnique=yes persistentBytes=0
[MAPEVENTPROBE] READY lookupFNV=........ elapsed=...ms hits=93 misses=931 first=68/0/00080044 last=.../92/........ stateEvents=93/93
[MAPEVENTPROBE] RAM heap8=X->X delta=0 largest8=Y->Y delta=0 frameFNV=F->F arenaFNV=c3882516->c3882516 stateFNV=cd99b98e->cd99b98e
[MAPEVENTPROBE] PARK state=9 page=3 nativeArena=yes nativeTileState=yes nativeEventLookup=yes persistentBytes=0 entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

The hardware run will establish:

```text
lookupFNV
full 1024-query elapsed time
last event tile
last event raw value
```

## Still forbidden

```text
bytecode execution / Game_runEvent()
legacy Render.tileEvents ownership
legacy Render_findEventIndex() use
entity/monster activation
visited/save-state mutation
player spawn
native gameplay rendering
ST_PLAYING
```

## Next boundary after hardware PASS

If the lookup contract passes, native code will be able to answer both:

```text
Does this tile carry an event?  -> EspMapState
Which compact event is it?      -> EspMapEvents
```

The next milestone can then study the raw event bit layout and bytecode linkage needed for a bounded native event descriptor/execution path, without reintroducing desktop `Game_t`/`Render_t` ownership.
