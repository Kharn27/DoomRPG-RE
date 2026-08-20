# ESP32 first gameplay BSP structural load

Branch: `agent/esp32-map1-structural-load`

Base merged `main`:

```text
897e982f4b37039d984b13265beaa68a83dce98b
```

That base is PR #41, the hardware-validated bounded intro teardown.

Hardware-validated implementation head:

```text
45833b68b0e185630b1e5a769e54a051196c70e8
```

Status: **REAL-CYD HARDWARE PASS; ESP32-NATIVE BSP PASS 1 + COMPACT MAP PLAN VALIDATED; MERGE-READY**.

This document is the milestone evidence for the first post-prologue gameplay BSP boundary. It records both the failed legacy feasibility model and the native replacement that was proven on a classic no-PSRAM CYD.

## Executive result

The branch established two complementary facts:

```text
legacy resident BSP + pointer-heavy runtime  -> does not fit safely
native .pak reader + compact structural plan -> fits trivially and is hardware-proven
```

The active implementation no longer attempts to make `Render_beginLoadMapData()` fit. `DoomRPG-RE` remains an executable specification/data-format reference; the destination is our own ESP32-native engine.

The first real post-prologue BSP is:

```text
startupMap = 1
MAP_INTRO  = 1
file       = /intro.bsp
name       = Entrance
```

`MAP_SECTOR01 = 2` maps to `/level01.bsp` and remains later work.

---

# Architecture contract

Permanent direction:

```text
original Doom RPG data / recovered behavior
                |
                v
        ESP32-native readers
                |
                v
       compact native map data
                |
                v
    mutable state overlays by index
                |
                v
      ESP32-native renderer/game
```

Not:

```text
original data
    -> desktop engine retained underneath
    -> pointer-heavy legacy structures
    -> wrappers around incompatible ownership/lifecycles
```

Permanent no-PSRAM invariants remain:

```text
shapeData   == NULL
mediaTexels == NULL
```

The final ESP32 build may stop compiling the desktop-derived engine completely once native components own the required behavior and formats.

---

# Phase A — legacy feasibility measurement

The first probe deliberately used the recovered loader only as an instrument. It measured the exact cost before allowing the unsafe structural allocation.

Normal firmware (`esp32-cyd`) reached the post-intro boundary at:

```text
heap8     = 84384
largest8  = 36852
```

Legacy ZIP/mapping measurements:

```text
mappings.bin
  compressed     = 2156 B
  uncompressed   = 8392 B
  miniz state    = 10992 B
  transient      = 21540 B
  resident cost  = 8440 B

/intro.bsp
  compressed     = 11150 B
  uncompressed   = 21823 B
  miniz state    = 10992 B
  transient      = 43965 B
```

Exact structural inventory:

```text
nodes          = 223
lines          = 480
mapSprites     = 344
runtimeSprites = 368   # legacy only: +16 custom +8 drop
events         = 93
byteCodes      = 265
strings        = 94
raw string data= 7779 B
legacy strings = 7873 B # includes one NUL per string
parsed         = 21823 / 21823 B
trailing       = 0 B
```

The desktop-derived structural allocation plan was:

```text
payload                    = 55341 B
largest single allocation  = 15360 B
safety headroom            = 4096 B
```

With mappings and the complete raw BSP resident, only `54104 B` of 8-bit heap remained. Therefore:

```text
zero-headroom deficit = 55341 - 54104 = 1237 B
with 4096 B headroom  = 59437 - 54104 = 5333 B
```

The largest individual allocation would have fitted (`15360 <= 20468` largest block); the simultaneous total working set would not.

The probe failed closed before allocation, cleaned mappings/BSP temporaries, and returned exactly to:

```text
heap8     = 84384
largest8  = 36852
```

No OOM, reset, leak or hidden map transition occurred.

Conclusion:

```text
measurement/safety-gate = PASS
legacy lifecycle        = REFUSED by real memory budget
```

Trying to shave a few KiB from that lifecycle would optimize the wrong architecture.

---

# Phase B — native `.pak` BSP reader

## Source model

`DoomRPG-ESP32.pak` stores the game resources uncompressed behind an on-disk hash index and supports bounded random-access reads.

The native reader therefore uses:

```text
DoomRPG-ESP32.pak
        -> find /intro.bsp
        -> fixed 256 B window
        -> sequential parser
        -> complete CRC32 + FNV proof
```

No ZIP inflate, complete BSP allocation or legacy mapping table is required.

`esp_bsp_reader.c/.h` intentionally has no dependency on `Render_t`, `DoomCanvas_t`, `Game_t`, `Node_t`, `Line_t` or `Sprite_t`. It is intended to remain part of the final native engine.

`native_map1_bsp_pass1.c/.h` and the lifecycle bridge are temporary handoff scaffolding only.

## First hardware PASS — pure inventory

The initial native pass consumed every byte of `/intro.bsp` through a 256-byte window:

```text
source       = 21823 B
readCalls    = 86
elapsed      = 125 ms
FNV-1a       = d5cc751f
CRC32        = 623f34e4
CRC verified = yes
```

The hardware reproduced all structural counts exactly and measured:

```text
heap8     84376 -> 84376  delta=0
largest8  36852 -> 36852  delta=0
```

The logical framebuffer FNV also remained unchanged across the reader call.

---

# Phase C — section offsets, resource sets and compact plan

The reader was then extended without allocating any map runtime. The second normal-firmware CYD run is the merge-quality hardware evidence for this branch.

## Entry/header proof

```text
entry offset = 1945016
entry size   = 21823
CRC32        = 623f34e4
window       = 256 B

name         = Entrance
loadMapId    = 1
spawn        = 904
direction    = 64
camera       = 648
floorTex     = 145
ceilingTex   = 112
```

Integrity remained:

```text
bytes        = 21823 / 21823
readCalls    = 86
FNV-1a       = d5cc751f
CRC32        = 623f34e4
verified     = yes
trailing     = 0
```

## Exact BSP section offsets

All offsets are relative to the `/intro.bsp` payload in the native pack:

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

These offsets are now regression-checked by the pass-1 lifecycle probe. They let future loaders use `EspAssetPack_readRange()` directly on individual sections rather than replaying or materializing the complete BSP.

## Map-derived resource inventory

Real `/intro.bsp` resource usage:

```text
unique line texture IDs      = 20
unique map-sprite IDs        = 48
required texture IDs         = 33
required sprite IDs          = 45
unique plane texture IDs     = 12
EV_CHANGESPRITE bytecodes    = 0
sprite-as-texture references = 0
ID overflow                  = 0 / 0 / 0
```

Interpretation:

- `lineTex=20` and `mapSpriteIds=48` are direct logical IDs physically referenced by map records;
- `textureReq=33` includes map-derived texture dependencies such as floor/ceiling, planes and the recovered line companion rules (`34->92`, `33->91`, `9<->10`);
- `spriteReq=45` is the map-derived sprite dependency set;
- the recovered `EV_CHANGESPRITE` path is opcode `34`; `/intro.bsp` contains no such bytecode, so `changeSprite=0` is a real property of this map;
- no map sprite in `/intro.bsp` uses the recovered sprite-as-tile path, so `spriteAsTexture=0` is also a real property, not a parser omission;
- all relevant IDs fit in the current 0..255 bitsets. The generic reader still counts overflows and must fail closed before trusting a truncated set on a future map.

Important scope rule: `textureReq=33` / `spriteReq=45` are **map-derived requirements**, not a declaration of the complete future gameplay asset set. The reference loader also injects groups of global sprites in code. Those globals are desktop/J2ME ownership policy and will be measured/assigned deliberately by the native engine instead of copied blindly.

## Compact structural plan

The first native plan deliberately keeps immutable source records compact and keeps string payloads on SD:

```text
nodes          2230 B  # 223 x 10 B raw native records
lines          4800 B  # 480 x 10 B
mapSprites     1720 B  # 344 x 5 B
events          372 B  # 93 x 4 B
byteCodes      2385 B  # 265 x 9 B
stringOffsets   188 B  # 94 x uint16_t
blockMap        256 B  # packed 2-bit cells
planeMap       2048 B  # original two 1024-byte maps
resourceSets     96 B  # three 256-ID bitsets
-----------------------------------------------
persistent    14095 B
```

Legacy structural reference:

```text
legacy structural = 55341 B
native base plan   = 14095 B
saved              = 41246 B
reduction          = ~74.5%
```

This is approximately 3.93x smaller than the measured legacy structural payload.

### What `14095 B` does and does not mean

It is a **bounded structural baseline**, not the final complete gameplay-RAM number.

Included:

```text
immutable BSP node/line/sprite records
events and bytecode records
packed blockmap
plane ID map
string offset table
bounded resource-ID sets
```

Intentionally excluded:

```text
mutable entities/monsters
player/gameplay state
door and script mutable overlays
runtime sprite links/order
custom/drop sprite pools
resource-cache payloads
string scratch buffer
renderer working buffers
```

The correct native direction is therefore:

```text
compact immutable map base
        +
small mutable overlays addressed by uint16/index
```

rather than expanding every BSP record into a pointer-heavy mutable desktop object.

The longest string in this map is `313 B`; a `314 B` temporary buffer would be sufficient to materialize any `/intro.bsp` string with a NUL terminator. That value is map-specific and must not be assumed for later BSPs without measurement.

## Final hardware performance/memory proof

The extended inventory+plan pass measured:

```text
readCalls = 86
elapsed   = 161 ms
FNV-1a    = d5cc751f
CRC32     = 623f34e4
```

The extra semantic parsing/resource marking increased the one-shot scan from the earlier `125 ms` inventory-only run to `161 ms`. This is not currently a performance concern: it is a bounded map-load operation, not a frame-loop cost.

Memory and framebuffer remained exactly unchanged:

```text
heap8     84376 -> 84376  delta=0
largest8  36852 -> 36852  delta=0
frameFNV  8e274563 -> 8e274563
```

The particular final story framebuffer hash is run-timing-specific; the contract is equality before/after the native pass.

Postconditions remained:

```text
state         = ST_INTRO (9)
storyPage     = 3
startupMap    = 1
legacy maps   = NULL
shapeData     = NULL
mediaTexels   = NULL
entities      = 0
monsters      = 0
legacy loader = NOT called
```

A later `[ALIVE]` heartbeat remained stable at the same heap/largest-block boundary.

Classification:

```text
native BSP reader              = REAL-CYD PASS
complete payload verification  = PASS
section offsets                = PASS
map-derived resource inventory = PASS
compact structural plan        = PASS
zero heap drift                = PASS
zero framebuffer drift         = PASS
safe PARK after pass           = PASS
```

---

# Merge boundary

This branch is **MERGE-READY**.

No additional hardware test is required for this milestone because the final changes after the successful run are documentation-only. The hardware-validated implementation is commit `45833b68b0e185630b1e5a769e54a051196c70e8`.

After merge, create a fresh branch from the new `main` for the first persistent native map-runtime allocation/population milestone.

Recommended next objective:

```text
post-intro page-3 PARK
    -> native BSP inventory/plan regression
    -> allocate one compact structural arena (or a deliberately small number of pools)
    -> readRange nodes/lines/sprites/events/bytecodes/blockmap/planes directly
    -> build string-offset table without copying string payloads
    -> keep mutable gameplay state separate by index
    -> verify exact heap cost + fragmentation
    -> PARK
```

Do not yet enter `ST_PLAYING`, spawn entities or load monolithic graphics. First prove that the planned native structural base can be made resident at the expected cost while preserving:

```text
shapeData   == NULL
mediaTexels == NULL
```

Only after that hardware boundary is stable should the next milestone add native rendering/gameplay consumers.
