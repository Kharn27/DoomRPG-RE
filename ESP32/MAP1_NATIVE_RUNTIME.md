# ESP32 MAP_INTRO native resident runtime

Branch: `agent/esp32-map1-native-runtime`

Base merged `main`:

```text
PR   = #42 — MAP_INTRO structural probe + native BSP pass 1
main = c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e
```

Implementation head before hardware validation:

```text
ed4ddda37d941ce6acb01148f92b6f5aebe2a275
```

Status: **IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS**.

## Objective

Turn the hardware-validated 14,095-byte compact MAP_INTRO plan into a genuinely resident native map base, then PARK before entities, rendering or `ST_PLAYING`.

This is the first milestone that keeps native gameplay-map structure persistently resident in RAM.

## Architectural ownership

`esp_map_runtime.c/.h` is intended to survive into the final ESP32 engine. It depends only on the native asset pack and BSP inventory contracts. It does not depend on `Render_t`, `DoomCanvas_t`, `Game_t`, `Node_t`, `Line_t` or `Sprite_t`.

`native_map1_runtime_load.c/.h` and the lifecycle bridge remain temporary migration/test scaffolding. They exist only to trigger the native component from the already validated post-intro boundary and verify that no legacy gameplay runtime appears.

Permanent direction remains:

```text
Doom RPG data / recovered behavior
        -> EspBspReader
        -> compact immutable EspMapRuntime
        -> small mutable index-based overlays
        -> native renderer/gameplay
```

## Inherited source proof

PR #42 established `/intro.bsp` as:

```text
name       = Entrance
size       = 21823 B
CRC32      = 623f34e4
FNV-1a     = d5cc751f
nodes      = 223
lines      = 480
mapSprites = 344
events     = 93
byteCodes  = 265
strings    = 94
```

Exact source offsets:

```text
nodes         = 35
lines         = 2267
mapSprites    = 7069
events        = 8791
byteCodes     = 9165
strings       = 11552
blockMap      = 19519
planeTextures = 19775
end           = 21823
```

## Exact arena contract

The runtime allocates **one 14,095-byte 8-bit-capable arena**.

The arena is deliberately byte-addressed. No C struct packing/alignment is allowed to silently inflate the hardware budget.

```text
arena + 0      nodes          2230 B
arena + 2230   lines          4800 B
arena + 7030   mapSprites     1720 B
arena + 8750   events          372 B
arena + 9122   byteCodes      2385 B
arena + 11507  stringOffsets   188 B
arena + 11695  blockMap        256 B
arena + 11951  planeMap       2048 B
arena + 13999  texture IDs      32 B
arena + 14031  sprite IDs       32 B
arena + 14063  plane IDs        32 B
--------------------------------------
end            14095 B
```

Nodes/lines/sprites/events/bytecodes remain in their original compact BSP record encoding. Future consumers decode fields on demand or build small overlays; they are not expanded into desktop pointer-heavy objects.

## Strings stay on SD

The `7,779 B` string payload is not copied into the arena.

For each of 94 strings, the runtime stores only one little-endian `uint16` BSP-entry-relative payload offset:

```text
94 x 2 B = 188 B
```

`/intro.bsp` is below 64 KiB, so 16-bit source offsets are valid for this map. The runtime fails closed for a source larger than `UINT16_MAX` under this plan rather than truncating offsets.

String lengths are discovered through a bounded 256-byte cached window while building the offset table. String payload bytes remain on SD.

## Direct population

After an inventory regression pass, population performs bounded native-pack reads directly into final arena locations:

```text
nodes section        -> arena nodes
lines section        -> arena lines
mapSprites section   -> arena sprites
events section       -> arena events
byteCodes section    -> arena byteCodes
strings section      -> 256 B scratch only; build offset table
blockMap section     -> arena blockMap
plane section        -> arena planeMap
resource sets        -> copy pass-1 bitsets
```

The complete raw BSP is never resident.

The loader records:

```text
populateReadCalls
populateElapsedMs
arenaFNV
heap8 before/after
largest8 before/after
allocator overhead above 14095 B payload
framebuffer FNV before/after
```

## Why pass 1 is temporarily repeated

The already hardware-validated PR #42 pass-1 implementation is intentionally left bit-identical on this branch.

The temporary lifecycle probe therefore inventories `/intro.bsp` again immediately before allocation instead of modifying pass 1 to retain hidden state. This costs roughly the previously measured 161 ms once during map load but isolates the new runtime milestone cleanly.

A later native map orchestrator will naturally perform inventory -> plan -> allocation/population as one operation without the duplicate validation scan.

## Fail-closed precondition

Before native allocation:

```text
intro disposal        = done
native BSP pass1      = done
state                 = ST_INTRO (9)
storyPage             = 3
startupMap            = MAP_INTRO (1)
legacy map arrays     = NULL
legacy mappings       = NULL
shapeData             = NULL
mediaTexels           = NULL
wall/sprite caches    = inactive
entities/monsters     = 0
native map arena      = absent
```

## Required postcondition

After success:

```text
native arena          = resident
arena payload         = exactly 14095 B
legacy map runtime    = still NULL
shapeData             = NULL
mediaTexels           = NULL
entities/monsters     = 0
state                 = ST_INTRO (9)
storyPage             = 3
framebuffer           = unchanged
ST_PLAYING            = NOT entered
```

The heap reduction is expected to be at least 14,095 B. The difference between measured heap consumption and the payload is recorded as allocator overhead; this milestone intentionally measures that value on real hardware instead of assuming it.

If any postcondition fails after allocation, `EspMapRuntime_reset()` frees the arena before returning and logs the recovered heap boundary.

## Forbidden work

This milestone does not add:

```text
legacy Render_beginLoadMapData()
shapeData/mediaTexels
entity or monster spawn
mutable door/script overlays
custom/drop sprite pools
resource-cache payloads
map rendering
player spawn
ST_PLAYING
continuous gameplay loop
```

## Real-CYD PASS criteria

Normal `esp32-cyd` must show:

- PR #42 native BSP pass 1 still succeeds first;
- the runtime arms only after pass 1 is done;
- inventory regression still reports `21823 B`, CRC `623f34e4`, FNV `d5cc751f`, plan `14095 B`;
- one native arena becomes resident;
- `arenaBytes=14095`;
- all counts/section byte sizes match the plan;
- a deterministic `arenaFNV` is reported;
- population read count/time are reported;
- heap use is >= 14,095 B and allocator overhead is measured;
- largest free block remains healthy and is recorded;
- logical framebuffer FNV is unchanged;
- all legacy runtime pointers remain NULL;
- entities/monsters remain zero;
- later `[ALIVE]` heartbeats remain stable with the arena resident.

Expected tail:

```text
[NATIVEMAP] ARMED ...
=== Doom RPG ESP32-native MAP_INTRO resident structural base ===
[NATIVEMAP] CONTRACT ...
[BSPREAD] ...
[NATIVEMAP] BEGIN ... plan=14095
[MAPRT] ARENA bytes=14095 ...
[MAPRT] READY arenaBytes=14095 populateReadCalls=... arenaFNV=...
[NATIVEMAP] READY arenaBytes=14095 ...
[NATIVEMAP] RESIDENT ...
[NATIVEMAP] RAM heap8=...->... used=... payload=14095 allocatorOverhead=... largest8=...->... frameFNV=X->X
[NATIVEMAP] PARK ... nativeArena=yes ... noGameplay=yes
[ALIVE] ...
```

## Next boundary after PASS

If this arena is hardware-stable, the following milestone should add the **smallest mutable native overlays needed by the first consumer**, still using integer indices instead of pointers where practical. Rendering/entity activation should remain separated until the memory ownership model is measured and stable.
