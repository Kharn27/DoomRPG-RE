# ESP32 MAP_INTRO native tile event lookup

Branch: `agent/esp32-map1-native-events`

Base merged `main`:

```text
PR   = #45 — first native mutable MAP_INTRO tile state
main = feec8a7fcb839dbd9f6de708f56f26b69a1e79e9
```

Hardware-tested code:

```text
a522c56403ff3269e02e93213b8f7d643bfba0af
```

Status: **REAL-CYD HARDWARE PASS; ALLOCATION-FREE NATIVE TILE EVENT LOOKUP VALIDATED; MERGE-READY**.

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

Legacy `Render_findEventIndex()` binary-searches the event array using that low-10-bit key. The native port preserves the useful behavior, not the desktop ownership model.

## Architecture decision: no duplicate tile index

A 1024-entry `eventIndexByTile` would cost another 1–2 KiB to index only 93 event-bearing cells.

Instead, the native engine keeps the 372-byte compact event section already resident in `EspMapRuntime` and performs a lower-bound binary search directly through `EspMapRuntime_getEvent()`.

Permanent component:

```text
ESP32/include/esp_map_events.h
ESP32/src/esp_map_events.c
```

Public API:

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

Bounds contract:

```text
valid tile = 0..1023
invalid tile / NULL output / missing runtime -> false
```

The implementation uses deterministic lower-bound semantics. If a future map ever has duplicate event tiles, the first matching record is returned. MAP_INTRO itself is now hardware-proven strictly ordered and unique.

## Why this belongs outside `EspMapState`

`EspMapState` owns mutable spatial flags such as `EVENTS`; it should not duplicate immutable event payloads or indices.

The intended native flow is:

```text
EspMapState_getTileFlags(tile)
    -> EVENTS bit says whether an event exists
    -> EspMapEvents_findByTile(tile)
    -> immutable raw event record + compact event index
```

Bytecode decoding/execution remains a later concern.

## Hardware validation strategy

The temporary probe:

```text
ESP32/include/native_map1_events_probe.h
ESP32/src/native_map1_events_probe.c
```

runs only after the complete native tile-state hardware boundary is proven.

It validates:

1. inherited `arenaFNV=c3882516` and `stateFNV=cd99b98e`;
2. all 93 raw event records;
3. strictly increasing `(event & 1023)` tile keys;
4. retained trigger-mask semantics for all MAP_INTRO events;
5. first event = tile 68 / index 0 / value `00080044`;
6. all **1024 tile lookups** through the public API;
7. every result against an independent sequential oracle;
8. exact agreement with `EspMapState.EVENTS` for every tile;
9. exactly 93 hits and 931 misses;
10. tile 1024 and NULL-output rejection;
11. canonical `lookupFNV` over the complete 1024-query result stream;
12. zero heap/largest-block drift;
13. framebuffer, immutable arena and mutable tile state byte-for-byte unchanged;
14. PARK before bytecode execution, entities, rendering or `ST_PLAYING`.

## Real-CYD hardware result

Exact serial evidence:

```text
[MAPEVENTS] READY events=93 firstTile=68 lastTile=968 sortedUnique=yes persistentBytes=0
[MAPEVENTPROBE] READY lookupFNV=63430151 elapsed=5ms hits=93 misses=931 first=68/0/00080044 last=968/92/000c23c8 stateEvents=93/93
[MAPEVENTPROBE] RAM heap8=69000->69000 delta=0 largest8=36852->36852 delta=0 frameFNV=b8b39f0f->b8b39f0f arenaFNV=c3882516->c3882516 stateFNV=cd99b98e->cd99b98e
[MAPEVENTPROBE] PARK state=9 page=3 nativeArena=yes nativeTileState=yes nativeEventLookup=yes persistentBytes=0 entities=0 monsters=0 noGameplay=yes
```

Canonical event-lookup fingerprint:

```text
lookupFNV = 63430151
```

Measured full sweep:

```text
queries        = 1024
hits           = 93
misses         = 931
elapsed        = 5 ms
persistent     = 0 B
heap drift     = 0 B
largest drift  = 0 B
```

Event topology established on hardware:

```text
records        = 93
first tile     = 68
first index    = 0
first value    = 00080044
last tile      = 968
last index     = 92
last value     = 000c23c8
sorted         = yes
unique         = yes
state agreement = 93 / 93
```

The old engine's ordering assumption is therefore now an explicit hardware-proven native contract for MAP_INTRO.

## Integrity proof

The event lookup changed no persistent state:

```text
heap8      = 69000 -> 69000
largest8   = 36852 -> 36852
frameFNV   = b8b39f0f -> b8b39f0f
arenaFNV   = c3882516 -> c3882516
stateFNV   = cd99b98e -> cd99b98e
entities   = 0
monsters   = 0
ST_PLAYING = no
```

Later `[ALIVE]` heartbeats remained stable at:

```text
heap8    = 69000
largest8 = 36852
```

The 16-byte lower absolute heap baseline versus the previous branch is a normal build-to-build code-size effect; the event lookup itself has exactly zero runtime heap drift.

## Native regression ladder

The port now has five useful fingerprints:

```text
source BSP FNV = d5cc751f
arenaFNV       = c3882516
decodedFNV     = a426dd18
stateFNV       = cd99b98e
lookupFNV      = 63430151
```

Each protects a different layer: source bytes, compact storage, decoded semantics, mutable spatial state, and tile-event resolution.

## Hardware gates — verdict

```text
93 events scanned                         PASS
strictly sorted/unique tile keys          PASS
1024 public lookups                       PASS
93 hits / 931 misses                      PASS
sequential oracle agreement               PASS
EspMapState.EVENTS agreement = 93/93      PASS
bounds / NULL rejection                   PASS
persistent bytes = 0                      PASS
heap drift = 0                            PASS
largest8 drift = 0                        PASS
framebuffer unchanged                     PASS
arenaFNV unchanged                        PASS
stateFNV unchanged                        PASS
entities/monsters = 0/0                   PASS
ST_PLAYING not entered                    PASS
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

## Merge recommendation

**MERGE this branch.**

Hardware-affecting code tested on the real classic CYD:

```text
a522c56403ff3269e02e93213b8f7d643bfba0af
```

Commits after that SHA must remain documentation-only until merge; if so, no additional flash is required.

## Next bounded milestone after merge

Native code can now answer both:

```text
Does this tile carry an event? -> EspMapState
Which event is it?             -> EspMapEvents
```

The next bounded milestone should decode the **raw event descriptor and its bytecode linkage** without executing it yet. The objective is to determine, validate and expose the exact event fields needed to locate the relevant compact bytecode sequence while preserving:

```text
arenaFNV   = c3882516
stateFNV   = cd99b98e
lookupFNV  = 63430151
heap drift = 0 if possible
```

Do not jump directly into `Game_runEvent()`, entities or `ST_PLAYING` before the descriptor/linkage contract is hardware-proven.