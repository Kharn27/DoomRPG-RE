# ESP32 MAP_INTRO native resident runtime

Branch: `agent/esp32-map1-native-runtime`

Base merged `main`:

```text
PR   = #42 — MAP_INTRO structural probe + native BSP pass 1
main = c71ac1fb07c2e281bc3f8a70c102dd22c7b9300e
```

Hardware-tested implementation head:

```text
ed4ddda37d941ce6acb01148f92b6f5aebe2a275
```

Status: **REAL-CYD HARDWARE PASS; NATIVE MAP ARENA RESIDENT; MERGE-READY**.

## Objective

Turn the hardware-validated 14,095-byte compact MAP_INTRO plan into a genuinely resident native map base, then PARK before entities, rendering or `ST_PLAYING`.

This milestone is the first point where a real gameplay BSP structure remains persistently resident in **our ESP32-native runtime**, not in the desktop-derived `Render_t` model.

## Architectural ownership

`esp_map_runtime.c/.h` is intended to survive into the final ESP32 engine. It depends on the native asset pack and BSP inventory contracts and does not depend on `Render_t`, `DoomCanvas_t`, `Game_t`, `Node_t`, `Line_t` or `Sprite_t`.

`native_map1_runtime_load.c/.h` and the lifecycle bridge remain temporary migration/test scaffolding.

Permanent direction:

```text
Doom RPG data / recovered behavior
        -> EspBspReader
        -> compact immutable EspMapRuntime
        -> small mutable index-based overlays
        -> native renderer/gameplay
```

## Inherited MAP_INTRO proof

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

Exact payload-relative source offsets:

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

The runtime allocates one byte-addressed 8-bit-capable arena with an exact payload of **14,095 B**.

No C struct packing/alignment is allowed to silently inflate the plan:

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
end                         = 14095 B
```

Nodes/lines/sprites/events/bytecodes remain in their original compact BSP record encoding. Future consumers decode fields on demand or add small index-based mutable overlays rather than expanding everything into pointer-heavy desktop objects.

## Strings remain on SD

The `7,779 B` of string payload is not copied into the arena.

For each of the 94 strings, the runtime stores one little-endian `uint16_t` BSP-entry-relative payload offset:

```text
94 x 2 B = 188 B
```

The longest string in `/intro.bsp` is 313 B, so this map can materialize any one string with a roughly 314-byte NUL-terminated scratch buffer when a future consumer needs it.

This 16-bit offset plan is valid because `/intro.bsp < 64 KiB`; the loader fails closed for larger sources rather than truncating offsets.

## Direct `.pak` population

After inventory regression, the runtime populates final arena locations directly from `DoomRPG-ESP32.pak`:

```text
nodes section        -> arena nodes
lines section        -> arena lines
mapSprites section   -> arena sprites
events section       -> arena events
byteCodes section    -> arena byteCodes
strings section      -> bounded 256 B scanner -> offset table only
blockMap section     -> arena blockMap
plane section        -> arena planeMap
resource sets        -> copy pass-1 bitsets
```

The complete 21,823-byte raw BSP is never resident.

## Real-CYD hardware PASS

Validation used normal optimized firmware:

```text
esp32-cyd
```

### PR #42 regression remained intact

Before the resident arena was created, the existing native BSP pass still reproduced:

```text
sourceBytes  = 21823
readCalls    = 86
elapsed      = 166 ms
FNV-1a       = d5cc751f
CRC32        = 623f34e4
plan         = 14095 B
heap8        = 84240 -> 84240
largest8     = 36852 -> 36852
frameFNV     = 11b4cc0e -> 11b4cc0e
```

The small post-intro heap shift versus PR #42 (`84376 -> 84240`) is ordinary build-to-build code/state movement; the pass itself still has zero drift.

### Temporary second inventory

The validation scaffold intentionally leaves the PR #42 pass untouched and inventories the BSP again immediately before allocation:

```text
second inventory = 145 ms
```

This is temporary validation overhead, not a required final-engine lifecycle. A native orchestrator can retain the already validated inventory and avoid this duplicate scan.

### Arena population

Hardware then reported:

```text
[MAPRT] ARENA bytes=14095
[MAPRT] READY arenaBytes=14095 populateReadCalls=33 arenaFNV=c3882516 strings=94 sourceCRC=623f34e4
```

Measured population cost:

```text
populateReadCalls = 33
populateElapsed   = 62 ms
arenaFNV          = c3882516
```

`c3882516` is now the first deterministic regression fingerprint for the complete compact resident MAP_INTRO structural base.

### Exact persistent RAM cost

Real allocator result:

```text
heap8     84240 -> 70128
used              14112 B
payload            14095 B
allocator overhead    17 B
largest8  36852 -> 36852
```

The actual persistent cost is therefore only 17 B above the planned payload.

Compared with the measured legacy structural allocation:

```text
legacy structural     = 55341 B
native actual heap use= 14112 B
saved                 = 41229 B
reduction             ~= 74.5%
```

The fact that `largest8` remains exactly `36852` after the 14,112-byte allocation is particularly healthy: the arena did not consume or split the largest available free block.

### Frame/state integrity

The resident load did not touch the visible image:

```text
frameFNV = 11b4cc0e -> 11b4cc0e
```

Postconditions:

```text
state          = ST_INTRO (9)
storyPage      = 3
startupMap     = 1
nativeArena    = yes
legacy runtime = NULL
shapeData      = NULL
mediaTexels    = NULL
entities       = 0
monsters       = 0
ST_PLAYING     = NOT entered
```

A later heartbeat remained stable with the arena resident:

```text
heap8     = 70128
largest8  = 36852
```

No reset, OOM, leak or hidden gameplay transition occurred.

## Load-time interpretation

Current temporary validation sequence costs approximately:

```text
pass1 inventory       166 ms
second inventory      145 ms   # temporary duplicate
arena population       62 ms
--------------------------------
current scaffold      ~373 ms
```

The duplicate 145 ms scan is removable once a native orchestrator owns inventory -> plan -> allocation/population as one lifecycle. Using the current measurements as a rough reference, the non-duplicated work is around 228 ms before any later gameplay-resource work is added.

This is map-load time, not frame-loop time, and is not currently an optimization priority.

## Merge classification

```text
native BSP regression              = PASS
one exact 14095-B arena             = PASS
direct native-pack population       = PASS
string payload left on SD           = PASS
arena FNV regression                = PASS (c3882516)
allocator overhead measured         = PASS (17 B)
largest free block preserved        = PASS (36852 B)
framebuffer unchanged               = PASS
legacy runtime still absent         = PASS
entities/monsters still absent      = PASS
stable resident heartbeat           = PASS
```

This branch is **MERGE-READY**.

No additional hardware run is required for this milestone if subsequent commits are documentation-only. The hardware-tested code head is `ed4ddda37d941ce6acb01148f92b6f5aebe2a275`.

Two harmless Serial formatting defects remain in that tested binary (`spriteAsTexture=0bounded8=yes` and `strings=94/188BblockMap=...`). They are intentionally not changed before merge because they are cosmetic and changing them would alter the tested firmware binary for no functional benefit.

## Next bounded milestone after merge

Start from fresh merged `main`.

The next milestone should add the **smallest native mutable/indexed layer or first consumer needed to make this structural base useful**, without regressing into desktop object ownership.

Preferred direction:

```text
resident immutable EspMapRuntime
        -> native accessors/decoders by index
        -> minimal mutable overlays only where gameplay actually mutates data
        -> establish first bounded spatial/render/game consumer
        -> measure RAM again
        -> PARK
```

Continue to avoid pointer-heavy per-record objects. Use integer indices (`uint16_t` where sufficient) for links/ownership whenever practical.

Still defer until their own measured milestones:

```text
full entity/monster activation
player spawn
ST_PLAYING
gameplay rendering loop
monolithic shapeData/mediaTexels
```
