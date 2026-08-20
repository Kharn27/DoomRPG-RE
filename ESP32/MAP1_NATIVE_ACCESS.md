# ESP32 MAP_INTRO native compact access contract

Branch: `agent/esp32-map1-native-access`

Base merged `main`:

```text
PR   = #43 — persistent compact native MAP_INTRO arena
main = 503fdd66fae625a45446fb4ea0853abc71d7dda3
```

Status: **IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS**.

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

The reference renderer later applies conditional ±3 coordinate nudges based on line flags and derives Z/length values. Those are runtime/render semantics and are intentionally **not** hidden inside the immutable accessor. A future native consumer may reproduce the required behavior explicitly.

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

## Expected hardware boundary

Inherited resident state before `MAPACCESS`:

```text
state        = ST_INTRO (9)
storyPage    = 3
startupMap   = 1
arenaBytes   = 14095
arenaFNV     = c3882516
heap8        ~= 70128 on PR #43 candidate build
largest8     = 36852
legacy map   = NULL
shapeData    = NULL
mediaTexels  = NULL
entities     = 0
monsters     = 0
```

Build-to-build heap movement is allowed; accessor execution itself must have zero drift.

Expected new log tail:

```text
[MAPACCESS] ARMED ...
=== Doom RPG ESP32-native MAP_INTRO compact access contract ===
[MAPACCESS] CONTRACT ...
[MAPACCESS] READY decodedFNV=........ elapsed=...ms ...
[MAPACCESS] SAMPLE node0=... line0=... sprite0=... event0=... code0=... string0=...
[MAPACCESS] BLOCK flags0=... flags1=... flags2=... flags3=... resources=33/45/12 strings=..... .... boundsChecks=yes
[MAPACCESS] RAM heap8=X->X delta=0 largest8=Y->Y delta=0 frameFNV=Z->Z arenaFNV=c3882516
[MAPACCESS] PARK ... nativeArena=yes immutable=yes overlays=none entities=0 monsters=0 noGameplay=yes
[ALIVE] ...
```

The first real-CYD run will establish the canonical decoded FNV and useful first-record/block-map samples for future consumers.

## Forbidden work

Still not part of this milestone:

```text
mutable entity/monster state
door/script overlays
sprite linked lists/relinking
legacy line-coordinate mutation hidden in accessors
custom/drop sprite pools
texture/sprite cache activation
map rendering
player spawn
ST_PLAYING
```

## Next boundary after hardware PASS

Once the read-only access contract is hardware-proven, choose the first real native consumer deliberately. The preferred next step is a **small index-based spatial/runtime overlay**, driven by actual consumer needs, rather than recreating all desktop runtime fields preemptively.
