# ESP32 MAP_INTRO native compact access contract

Branch: `agent/esp32-map1-native-access`

Base merged `main`:

```text
PR   = #43 — persistent compact native MAP_INTRO arena
main = 503fdd66fae625a45446fb4ea0853abc71d7dda3
```

Hardware-tested implementation head:

```text
dfe25218b74db9d2765850fbc29057e703c57154
```

Status: **REAL-CYD HARDWARE PASS; NATIVE COMPACT ACCESS CONTRACT VALIDATED; MERGE-READY**.

## Objective

Make the hardware-validated 14,095-byte resident `EspMapRuntime` directly usable by future native renderer/gameplay code without expanding its compact records into desktop-derived pointer-heavy structures.

This milestone remains read-only and allocation-free after the arena is resident. It does not add entities, overlays, rendering or `ST_PLAYING`.

## Permanent ownership boundary

The reusable API lives in:

```text
ESP32/include/esp_map_runtime.h
ESP32/src/esp_map_access.c
```

The temporary real-CYD validation scaffold lives in:

```text
ESP32/include/native_map1_access_probe.h
ESP32/src/native_map1_access_probe.c
```

`esp_map_access.c` depends only on `EspMapRuntime`; it has no dependency on `Render_t`, `DoomCanvas_t`, `Game_t`, `Node_t`, `Line_t` or `Sprite_t`.

Target ownership remains:

```text
Doom RPG BSP data
    -> EspBspReader
    -> compact immutable EspMapRuntime arena
    -> allocation-free native accessors
    -> small explicit mutable overlays / consumers
    -> native renderer + gameplay
```

## Why accessors instead of C structs over the arena

The arena is deliberately byte-addressed and contains records at their original compact widths:

```text
node     = 10 B
line     = 10 B
sprite   = 5 B
event    = 4 B
bytecode = 9 B
```

Casting arena addresses to ordinary C structs would reintroduce alignment/padding assumptions and would be unsafe for unaligned records such as 5-byte sprites.

The accessors therefore decode little-endian fields explicitly and return small temporary value structs on the caller stack.

No accessor allocates memory or changes the arena.

## Recovered source decode contract

### Node — 10 bytes

Recovered reference behavior:

```text
raw[0] << 3 -> x1
raw[1] << 3 -> y1
raw[2] << 3 -> x2
raw[3] << 3 -> y2

args1 = (raw[4] << 16) | (raw[5] << 3)
args2 = LE16(raw+6) | (LE16(raw+8) << 16)
```

Native value type:

```text
EspMapNode
  x1/y1/x2/y2 : uint16
  args1/args2 : uint32
```

### Line — 10 bytes

```text
raw[0] << 3 -> x1
raw[1] << 3 -> y1
raw[2] << 3 -> x2
raw[3] << 3 -> y2
LE16(raw+4)  -> texture
LE32(raw+6)  -> flags
```

`EspMapRuntime_getLine()` returns these **source coordinates**.

The reference renderer later applies conditional +/-3 coordinate nudges based on line flags and derives Z/length values. Those are runtime/render semantics and are intentionally **not** hidden inside the immutable accessor. A future native consumer may reproduce the required behavior explicitly.

### Map sprite — 5 bytes

```text
raw[0] << 3 -> x
raw[1] << 3 -> y
info = raw[2] | (LE16(raw+3) << 16)
```

Again, this is source state. The reference runtime later performs one-unit coordinate nudges for some flag combinations, relinks sprites into BSP nodes and may OR additional runtime flags. Those mutations belong in future overlays/consumers, not in `EspMapRuntime_getMapSprite()`.

### Event — 4 bytes

```text
LE32(record) -> event value
```

### Bytecode — 9 bytes

```text
byte 0       -> opcode ID
LE32(raw+1)  -> arg1
LE32(raw+5)  -> arg2
```

### Strings

The existing resident table keeps one BSP-entry-relative payload offset per string:

```text
94 x uint16 = 188 B
```

`EspMapRuntime_getStringSourceOffset()` remains bounds-checked. Text payload stays on SD.

### Block map

The 256-byte block map represents 1024 logical cells, four 2-bit values per source byte:

```text
cell 0 -> bits 1..0
cell 1 -> bits 3..2
cell 2 -> bits 5..4
cell 3 -> bits 7..6
```

`EspMapRuntime_getBlockCell()` exposes one 0..3 value by cell index.

### Plane texture maps

Two consecutive 1024-byte maps remain resident in source form:

```text
plane 0 : 1024 texture IDs
plane 1 : 1024 texture IDs
```

`EspMapRuntime_getPlaneTexture(plane, cell)` returns the original logical texture ID.

### Resource bitsets

The arena contains three 256-ID sets:

```text
required textures
required sprites
plane textures used
```

They begin at an unaligned arena offset, so the access layer does **not** cast them to `uint32_t*`. It reads each 32-bit word byte-wise/little-endian and exposes:

```text
EspMapRuntime_textureRequired(id)
EspMapRuntime_spriteRequired(id)
EspMapRuntime_planeTextureUsed(id)
```

IDs >= 256 return false; future BSP inventory still fails closed if a required set cannot be represented completely.

## Temporary hardware validation probe

After the PR #43 resident arena reports done, the lifecycle bridge arms `MAPACCESS` and executes it on the following Arduino loop.

The probe:

1. verifies the inherited arena regression (`14095 B`, `arenaFNV=c3882516`);
2. decodes every node, line, map sprite, event and bytecode through the public accessors;
3. independently compares each decoded field with the underlying compact bytes;
4. walks every string offset and requires strict monotonic in-range payload offsets;
5. decodes all 1024 block cells and compares them with the packed bytes;
6. decodes all 2048 plane cells and compares them with the resident plane bytes;
7. walks all 256 resource IDs and requires the known MAP_INTRO counts `33 / 45 / 12`;
8. explicitly tests out-of-range indexes for every accessor family;
9. computes one canonical FNV-1a over the **decoded semantic values**, independent of arena padding/layout;
10. requires exact before/after equality for heap8, largest8 and framebuffer FNV.

No heap allocation occurs during the probe.

## Real-CYD hardware PASS

Validation used the normal optimized `esp32-cyd` build at hardware-affecting head:

```text
dfe25218b74db9d2765850fbc29057e703c57154
```

### Inherited BSP + arena regressions

The earlier native contracts remained intact on this build:

```text
BSP bytes/FNV/CRC = 21823 / d5cc751f / 623f34e4
BSP readCalls      = 86
BSP elapsed        = 166 ms
compact plan       = 14095 B
pass1 heap8        = 84224 -> 84224
pass1 largest8     = 36852 -> 36852
pass1 frameFNV     = 1f4d9cd6 -> 1f4d9cd6

second inventory   = 145 ms
arena population   = 63 ms
arena bytes/FNV    = 14095 / c3882516
population reads   = 33
resident heap8     = 84224 -> 70112
actual heap use    = 14112 B
allocator overhead = 17 B
resident largest8  = 36852 -> 36852
resident frameFNV  = 1f4d9cd6 -> 1f4d9cd6
```

The small heap baseline movement versus PR #43 is normal build-to-build code/state movement; all stage-local drift contracts still pass exactly.

### Complete accessor sweep

The hardware probe successfully consumed every exposed compact family through the public API:

```text
nodes       = 223
lines       = 480
sprites     = 344
events      = 93
byteCodes   = 265
strings     = 94
blockCells  = 1024
planeCells  = 2048
resource IDs checked = 256
```

Canonical decoded semantic fingerprint:

```text
decodedFNV = a426dd18
elapsed    = 3 ms
```

`a426dd18` is now the hardware regression hash for the decoded MAP_INTRO access contract. It complements `arenaFNV=c3882516`: the arena hash fingerprints physical compact storage, while the decoded hash fingerprints the values exposed to native consumers.

### Hardware-decoded reference sample

```text
node0   = 64,128-1984,1984 args=00010400/00640001
line0   = 1728,1440-1696,1472 tex=87 flags=00000000
sprite0 = 160,1440 info=00010091
event0  = 00080044
code0   = 16/000001cb/000000d0
string0 = 11554
```

These values are source semantics before any future mutable/runtime transformation.

### Block map, strings and resources

The 1024 unpacked 2-bit block cells have this distribution:

```text
value 0 = 298
value 1 = 697
value 2 = 27
value 3 = 2
----------------
total   = 1024
```

The string payload-offset table spans:

```text
first = 11554
last  = 19512
```

The complete resource-set sweep reproduced:

```text
required textures = 33
required sprites  = 45
plane textures    = 12
```

All explicit out-of-range accessor tests failed closed as required (`boundsChecks=yes`).

### Zero-cost read contract

The entire semantic sweep has no persistent or transient heap effect visible to the allocator:

```text
heap8     = 70112 -> 70112
largest8  = 36852 -> 36852
frameFNV  = 1f4d9cd6 -> 1f4d9cd6
arenaFNV  = c3882516
```

The probe leaves:

```text
state       = ST_INTRO page 3
nativeArena = resident + immutable
overlays    = none
entities    = 0
monsters    = 0
gameplay    = not entered
```

A later `[ALIVE]` heartbeat remained stable at `heap8=70112`, `largest8=36852`.

No OOM, reset, leak, framebuffer mutation, arena mutation or hidden gameplay transition occurred.

## Cosmetic inherited Serial formatting

Two harmless spacing defects remain visible in inherited logs:

```text
spriteAsTexture=0bounded8=yes
strings=94/188BblockMap=...
```

They are cosmetic only and are deliberately not changed after this hardware pass, so the merge candidate remains the exact tested firmware plus documentation-only commits.

## Merge classification

```text
BSP reader regression             = PASS
resident arena regression         = PASS
all compact record accessors      = PASS
all strings offsets               = PASS
1024 block cells                  = PASS
2048 plane cells                  = PASS
256 resource IDs                  = PASS
out-of-range fail-closed checks   = PASS
canonical decoded FNV             = PASS (a426dd18)
access sweep elapsed              = PASS (3 ms)
heap drift                        = PASS (0 B)
largest-block drift               = PASS (0 B)
framebuffer drift                 = PASS (none)
arena mutation                    = PASS (none)
legacy runtime/entities/gameplay  = absent as required
stable heartbeat                  = PASS
```

This branch is **MERGE-READY**.

No additional hardware test is required if subsequent commits are documentation-only.

## Next bounded milestone after merge

The first useful native mutable consumer is now clear from the recovered reference behavior: create a small **spatial tile-state overlay** instead of recreating pointer-heavy object graphs.

Preferred next direction:

```text
resident immutable EspMapRuntime
    -> allocation-free accessors (hardware proven)
    -> EspMapState tileFlags[1024]
         initialize low block-map bits
         derive BIT_AM_ENTRANCE from relevant lines
         derive BIT_AM_EVENTS from events
    -> measure exact ~1 KiB mutable cost
    -> verify source arena remains immutable
    -> PARK
```

This is meaningful gameplay/world state and a real consumer of the new API, while still deferring entities, sprite relinking, renderer activation, player spawn and `ST_PLAYING` to later measured milestones.
