# ESP32 first gameplay BSP structural load

Branch: `agent/esp32-map1-structural-load`

Base merged `main`:

```text
897e982f4b37039d984b13265beaa68a83dce98b
```

That base is PR #41, the hardware-validated bounded intro teardown.

Status: **REAL-CYD MEASUREMENT PASS; NATIVE `.pak` BSP PASS 1 IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS; BRANCH CONTINUES**.

## What this branch has established

This branch first answered a feasibility question before committing to an engine
architecture:

```text
Can the desktop/J2ME-derived resident-BSP + resident-runtime loader fit
/intro.bsp safely on the classic no-PSRAM CYD?
```

The real-CYD answer is now measured:

```text
NO.
```

The legacy feasibility probe refused before the unsafe allocation, cleaned every
temporary resource and returned to the exact post-intro boundary without reset or
heap drift.

That measurement is now being used as intended: the active firmware path on this
same branch has moved to the first reusable **ESP32-native BSP reader**.

## Permanent architecture interpretation

DoomRPG-RE is an **executable specification and data-format reference**. It is not
the architecture contract for the final CYD firmware.

The final ESP32 engine may completely stop compiling the desktop-derived sources.
Current names/calls such as:

```text
Render_t
DoomCanvas_t
Render_beginLoadMap()
Render_beginLoadMapData()
DoomCanvas_run()
```

are migration/probe scaffolding while behavior and formats are recovered. They
are not permanent engine boundaries.

Target direction:

```text
original Doom RPG data formats / behavior
                |
                v
        ESP32-native readers
                |
                v
      ESP32-native runtime model
                |
                v
       ESP32-native renderer/game
```

not:

```text
original data
    -> desktop engine kept underneath forever
    -> ESP32 wrappers around every incompatible subsystem
```

## Recovery boundary inherited from PR #41

PR #41 proved the classic CYD can park after intro resources are released and
before any gameplay map load:

```text
menu                    = MENU_NONE
state                   = ST_INTRO (9)
storyPage               = 3
storyTextPage           = 0
intro clock/input       = inactive
intro images/texts      = NULL
render clip             = off
startupMap              = 1
heap8                   = 84408 on the PR #41 validation build
largest8                = 36852
nodes/lines/mapSprites  = NULL
mapping/ref arrays      = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite caches      = inactive
Game entities/monsters  = 0
DoomCanvas_loadMap      = NOT called
```

The legacy-feasibility probe build had a small normal build-to-build shift:

```text
post-intro heap8     = 84384
post-intro largest8  = 36852
```

## Important map-ID clarification

`startupMap == 1` is **not** `level01.bsp`.

Recovered enum/resource mapping:

```text
MAP_MENU     = 0
MAP_INTRO    = 1 -> /intro.bsp
MAP_SECTOR01 = 2 -> /level01.bsp
```

The first post-prologue gameplay BSP is therefore:

```text
MAP_INTRO / /intro.bsp
```

`/level01.bsp` remains later work.

---

# Phase A — legacy feasibility measurement

## Fail-closed measurement scaffold

The first implementation on this branch deliberately used the recovered loader
only as a measuring instrument:

```text
post-intro PARK / page 3
    -> ZIP/memory preflight
    -> temporary Render_loadMappings()
    -> parse raw /intro.bsp to calculate exact structural allocations
    -> if safe, release probe BSP
    -> Render_beginLoadMap(MAP_INTRO)
    -> Render_beginLoadMapData()
    -> hard stop before Render_loadBitShapes()
```

It forbade:

```text
Render_loadBitShapes()
Render_loadTexels()
Game_loadMapEntities()
Game_loadWorldState()
Game_spawnPlayer()
DoomCanvas_updateView()
DoomCanvas_setState(ST_PLAYING)
DoomCanvas_run()
```

Permanent no-PSRAM invariant:

```text
shapeData   == NULL
mediaTexels == NULL
```

The real hardware refused before the structural allocator, so the prepared
bitshape hard-stop was never reached during the validation run.

## Real-CYD measurement profile

Validation used normal optimized firmware:

```text
esp32-cyd
```

not `esp32-cyd-bringup`.

### Intro teardown still passed

```text
before teardown
  heap8     = 50620
  largest8  = 13300

after teardown
  heap8     = 84384
  largest8  = 36852
  recovered = 33764 B
```

PR #41 had measured `33768 B`; the four-byte difference is ordinary build
baseline movement. Resource order and the `36852` largest block remained stable.

### Legacy ZIP working sets

```text
mappings.bin
  compressed     = 2156 B
  uncompressed   = 8392 B
  miniz state    = 10992 B
  transient      = 21540 B

/intro.bsp
  compressed     = 11150 B
  uncompressed   = 21823 B
  miniz state    = 10992 B
  transient      = 43965 B
```

Both individual inflates fit the initial post-intro boundary.

### Legacy mapping cost

```text
heap8     84384 -> 75944
largest8  36852 -> 36852
resident cost   = 8440 B
```

The old `mappingMemory=-8440` diagnostic has reversed accounting sign; heap
measurement is authoritative.

### Exact `/intro.bsp` inventory

The bounded parser consumed the file exactly:

```text
nodes          = 223
lines          = 480
mapSprites     = 344
runtimeSprites = 368   (344 + 16 custom + 8 drop in the legacy model)
events         = 93
byteCodes      = 265
strings        = 94
legacy string allocation bytes = 7873
raw string payload bytes        = 7779
parsed         = 21823 / 21823 B
trailing       = 0 B
```

The distinction between the two string figures matters:

```text
7779 B = bytes physically stored in the BSP strings
7873 B = legacy allocation plan, adding one NUL byte for each of 94 strings
```

These are primary regression values for the native reader.

### Legacy structural allocation plan

With current desktop-derived runtime types:

```text
structural allocation payload = 55341 B
largest single allocation     = 15360 B
safety headroom               = 4096 B
```

With mappings and the full uncompressed BSP resident simultaneously:

```text
heap8 available       = 54104 B
largest8              = 20468 B
required + headroom   = 59437 B
largest allocation    = 15360 B
```

The largest individual allocation would fit. The total simultaneous working set
would not.

Deficit:

```text
with 4096 B safety headroom:
  59437 - 54104 = 5333 B short

with ZERO safety headroom:
  55341 - 54104 = 1237 B short
```

Therefore the refusal is **not** caused by an overly conservative guard. The
resident-BSP + resident-runtime lifecycle genuinely does not fit even with zero
safety margin.

## Why the legacy lifecycle loses

The problem is not that `/intro.bsp` is intrinsically huge; it is only 21,823 B
uncompressed.

The waste comes from coexistence:

```text
mappings resident
+ complete uncompressed BSP resident
+ progressively allocated runtime structures
```

The legacy loader does not release the raw BSP until nodes, lines, sprites,
scripts, strings, blockmap and plane references have all been instantiated.

Trying to shave ~5 KiB until that architecture happens to pass would optimize the
wrong design.

## Safe refusal / cleanup proof

Hardware emitted:

```text
[MAP1STRUCT] REFUSED structural working set does not fit with raw BSP resident
```

before entering the real structural allocator.

The fail path released the temporary BSP plus mapping/runtime fields. Later
heartbeats returned exactly to:

```text
heap8     = 84384
largest8  = 36852
```

No reset, OOM, hidden map transition or heap drift occurred.

Classification:

```text
measurement / safety-gate PASS
legacy structural feasibility = REFUSED
```

The legacy probe source and its `Render_loadBitShapes` linker gate are now
**retired from the active branch tree**. Their evidence remains preserved in Git
history and in this document.

---

# Phase B — ESP32-native BSP reader pass 1

## Why the native `.pak` is the correct source

The first design sketch considered streaming DEFLATE directly from
`DoomRPG.zip`. That would still be better than the old loader, but DEFLATE needs
history/window state and would add complexity and RAM pressure we have already
solved elsewhere.

`DoomRPG-ESP32.pak` v2 already mirrors every ZIP entry **uncompressed** behind a
hash-sorted on-disk index. It provides allocation-free `seek/readRange` access.

Therefore the active native path is:

```text
/DoomRPG-ESP32.pak on SD
          |
          v
  EspAssetPack lookup
          |
          v
  fixed 256-byte window
          |
          v
      EspBspReader
          |
          v
 scalar inventory only
```

No ZIP inflate occurs in this pass.

## Component ownership

### `esp_bsp_reader.c/.h`

This is intended to survive into the final ESP32 engine.

It depends on:

```text
esp_asset_pack
stdint/string primitives
```

It does **not** depend on:

```text
Render_t
DoomCanvas_t
Game_t
Node_t
Line_t
Sprite_t
```

The reader is generic for BSP resources; `/intro.bsp` is only its first hardware
regression target.

### `native_map1_bsp_pass1.c/.h`

This is temporary lifecycle scaffolding. It uses the current recovered game
objects only to trigger the reader safely after PR #41's intro teardown and to
prove that no legacy runtime fields appear.

### `native_map1_lifecycle_bridge.c`

The existing cross-object wrapper remains temporary. It now only:

```text
Esp32IntroDispose_reset()
    -> reset native BSP pass-1 state

Esp32IntroDispose_service()
    -> run validated disposer
    -> arm pass 1
    -> next Arduino loop runs native inventory
```

It no longer wraps `Render_loadBitShapes()`.

## Reader memory model

The complete BSP is never allocated.

```text
fixed reader window = 256 B
inventory           = scalar counters/header fields
asset pack index    = remains on SD
BSP payload         = remains on SD
```

The reader walks every source byte in order. Record bodies that do not yet need
semantic decoding are consumed through the same 256-byte window, so CRC/FNV cover
the complete file rather than only section headers.

For the 21,823-byte `/intro.bsp`, the theoretical payload-window count is about:

```text
ceil(21823 / 256) = 86 reads
```

The actual hardware count and elapsed time are logged.

## Integrity proof

Pack v2 stores the original ZIP CRC32 for each entry. Pass 1 computes CRC32 while
streaming the BSP and requires exact equality.

It also computes FNV-1a for a cheap deterministic regression fingerprint.

Expected log shape:

```text
[BSPREAD] ENTRY /intro.bsp offset=... size=21823 crc32=... window=256B
[BSPREAD] HEADER name='...' loadMapId=... spawn=... dir=... camera=... floorTex=... ceilingTex=...
[BSPREAD] INVENTORY nodes=223 lines=480 mapSprites=344 events=93 byteCodes=265 strings=94 stringData=7779 legacyStringAlloc=7873 maxString=... structuralEnd=21823 trailing=0
[BSPREAD] STREAM bytes=21823/21823 readCalls=... window=256B fnv1a=... crc32=... verified=yes
```

## Pass-1 runtime contract

After PR #41 disposal, pass 1 requires:

```text
state                   = ST_INTRO
storyPage               = 3
startupMap              = MAP_INTRO (1)
intro resources         = NULL
intro clock/input       = inactive
legacy mappings         = NULL
legacy runtime arrays   = NULL
shapeData               = NULL
mediaTexels             = NULL
entities/monsters       = 0
native wall/sprite LRU  = inactive
```

Then it performs only:

```text
open DoomRPG-ESP32.pak if needed
find /intro.bsp
stream complete entry through 256 B window
parse header + section counts/string lengths
CRC32 verify
FNV compute
close pack if this call opened it
PARK
```

Forbidden in this pass:

```text
readZipFileEntry(/intro.bsp)
Render_loadMappings()
Render_beginLoadMap()
Render_beginLoadMapData()
Render_loadBitShapes()
Render_loadTexels()
Game_loadMapEntities()
any native map-runtime allocation
any gameplay state transition
```

## Hardware regression targets

The native pass requires the previously measured inventory exactly:

```text
sourceBytes        = 21823
nodes              = 223
lines              = 480
mapSprites         = 344
events             = 93
byteCodes          = 265
strings            = 94
stringData         = 7779
legacyStringAlloc  = 7873
structuralEnd      = 21823
trailing           = 0
CRC32              = pack index CRC32
```

`runtimeSprites=368` remains a **legacy runtime-plan reference**, not a raw BSP
field and not a requirement for the future native runtime.

## Hardware PASS criteria for native pass 1

PASS requires all of the following on normal `esp32-cyd`:

- PR #41 intro teardown still succeeds first;
- `NATIVEBSP1` arms only after teardown reports done;
- `/intro.bsp` is read from `DoomRPG-ESP32.pak`, not inflated from ZIP;
- the reader consumes exactly `21823/21823` bytes;
- all inventory counts match the regression target;
- CRC32 matches the pack entry;
- FNV-1a is reported for future regression use;
- `readCalls` and elapsed milliseconds are measured;
- no legacy mapping/runtime field becomes resident;
- `shapeData == NULL`;
- `mediaTexels == NULL`;
- entities/monsters remain zero;
- framebuffer FNV is unchanged across the inventory pass;
- `heap8` is identical before/after the complete open/read/close cycle;
- largest 8-bit free block is identical before/after;
- later `[ALIVE]` heartbeats remain stable.

Expected final shape:

```text
[NATIVEBSP1] READY ...
[NATIVEBSP1] STREAM window=256B readCalls=... elapsed=...ms fnv1a=... crc32=... verified=yes
[NATIVEBSP1] RAM heap8=X->X delta=0 largest8=Y->Y delta=0 frameFNV=Z->Z
[NATIVEBSP1] PARK ... mappings=NULL runtime=NULL shapeData=0x0 mediaTexels=0x0 entities=0 monsters=0 noLegacyMapLoader=yes
[ALIVE] ...
```

---

# After native pass-1 hardware PASS

The next work on this branch is **not** to restore the desktop structures.

It is to define the smallest native runtime actually required by our renderer and
game logic, then populate it from the same BSP source.

Preferred flow:

```text
PASS 1
  -> inventory/validate from .pak
  -> exact native allocation plan

CONTROLLED ALLOCATION
  -> consolidated native pools / arrays
  -> largest pools deliberately ordered
  -> measure fragmentation

PASS 2
  -> re-read /intro.bsp through EspBspReader-style windows
  -> decode directly into final native pools
  -> never materialize complete raw BSP
```

The native structures are not required to match `Node_t`, `Line_t` or `Sprite_t`.
Field widths and ownership should follow actual ESP32 consumers.

Immediate candidates discovered by the measurement remain:

```text
480 legacy Line_t entries
  -> old largest allocation = 15360 B
  -> inspect which fields our renderer really needs

94 strings
  -> packed string pool + offset table instead of 94 heap allocations

344 map sprites
  -> compact indexes instead of pointer-heavy desktop relationships

223 BSP nodes
  -> compact child/index representation where consumers allow it
```

Do not freeze packed sizes until pass-2 consumers are mapped.

Mappings are also not assumed permanently resident. The old mapping arrays cost
`8440 B`; native resource IDs/mappings should be represented only when required by
native rendering/resource consumers.

## Performance philosophy

No fixed gameplay FPS target is imposed yet.

Doom RPG is turn-based. Input/game logic must be independent from panel
presentation cadence. Prefer demand-driven rendering:

```text
static scene
  -> no pointless continuous redraw

player action / animation
  -> update logic
  -> render only visually useful frames

input cadence
  != game-turn cadence
  != render cadence
```

The intro feels acceptable despite roughly 14 measured rendered FPS. Gameplay
animations may tolerate substantially less than a real-time shooter, but no
arbitrary `5 FPS` target is frozen into the architecture. Hardware measurements
and perceived smoothness will decide optimization effort once native gameplay
rendering exists.

Priority remains:

```text
correctness
-> bounded RAM
-> stable input/game logic
-> correct visuals
-> measured performance optimization
```
