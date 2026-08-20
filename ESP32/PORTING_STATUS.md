# Doom RPG ESP32 CYD porting status

This file is the **authoritative current recovery point** for the classic ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for the stable build/architecture guide, [`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules, and milestone documents for detailed hardware evidence.

The previous full recovery catalog is preserved unchanged in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md) so no historical menu/touch/FNV/RAM measurements were lost when this file was condensed around the new native-map boundary.

## Current branch state

Latest merged hardware baseline **before this candidate is merged**:

```text
PR   = #41 — bounded intro disposal
main = 897e982f4b37039d984b13265beaa68a83dce98b
```

Current candidate:

```text
branch = agent/esp32-map1-structural-load
hardware-validated code head = 45833b68b0e185630b1e5a769e54a051196c70e8
status = REAL-CYD HARDWARE PASS; NATIVE BSP PASS1 + MAP PLAN VALIDATED; MERGE-READY
```

Documentation-only commits may follow the hardware-tested code head. They do not change the firmware binary or invalidate the hardware result.

Detailed evidence: [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md).

## Target / permanent invariants

```text
board        = ESP32-2432S028R classic CYD
MCU          = ESP32-D0WD-V3 dual-core 240 MHz
flash        = 4 MB
PSRAM        = none
display      = ILI9341 320x240 landscape
touch        = XPT2046
storage      = microSD
framebuffer  = 160x120 RGB565 = 38400 B
presentation = exact nearest-neighbor 2x
audio        = deferred
```

Permanent resource rule:

```text
shapeData   == NULL
mediaTexels == NULL
```

Permanent ownership rule:

```text
DoomRPG-RE = executable specification / data-format + behavior reference
final CYD engine = our ESP32-native engine
```

Desktop-derived `Render_t`, `DoomCanvas_t`, pointer-heavy map structures and linker wrappers are temporary migration scaffolding, not architecture requirements.

## Current hardware-safe boundary

The current normal-firmware run reaches the validated PR #41 teardown boundary and then executes the native BSP inventory/plan without changing it:

```text
menu                    = MENU_NONE
state                   = ST_INTRO (9)
storyPage               = 3
storyTextPage           = 0
intro clock/input       = inactive
intro images/texts      = NULL
render clip             = off
startupMap              = 1 (MAP_INTRO)
heap8                   = 84376 on current candidate build
largest8                = 36852
legacy nodes            = NULL
legacy lines            = NULL
legacy mapSprites       = NULL
legacy mappings         = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite LRU caches  = inactive
entities                = 0
monsters                = 0
DoomCanvas_run          = NOT called
legacy map loader       = NOT called
```

PR #41 itself measured `heap8=84408`, `largest8=36852`; the small heap shift is ordinary build-to-build code/state movement. The stable contract is the logical boundary plus before/after equality for bounded probes.

## Current execution path

```text
video / SD / ZIP
    -> transitional core/layout startup
    -> native/bounded menu runtime
    -> semantic XPT2046 input
    -> fresh Start Game
    -> menu/dead-resource cleanup
    -> Player_reset behavior
    -> ST_INTRO
    -> fitted deterministic intro renderer
    -> bounded 50 ms intro clock
    -> bounded More / Continue input
    -> final page-2 PARK
    -> bounded intro teardown
    -> ST_INTRO page 3
    -> native EspBspReader over DoomRPG-ESP32.pak
    -> /intro.bsp inventory + offsets + resource sets + compact map plan
    -> CRC/FNV proof
    -> zero heap/framebuffer drift
    -> PARK
```

No legacy gameplay map load occurs.

## MAP_INTRO identity

```text
MAP_MENU     = 0
MAP_INTRO    = 1 -> /intro.bsp -> name "Entrance"
MAP_SECTOR01 = 2 -> /level01.bsp
```

The current milestone is therefore the first post-prologue gameplay BSP, not yet `/level01.bsp`.

## Legacy feasibility result

The legacy loader model was measured first and safely refused.

```text
mappings resident cost       = 8440 B
/intro.bsp uncompressed      = 21823 B
legacy structural payload    = 55341 B
largest legacy allocation    = 15360 B
heap with raw BSP resident   = 54104 B
zero-headroom deficit        = 1237 B
4096-B-headroom deficit      = 5333 B
```

The failure was total simultaneous working set, not inability to satisfy the largest individual allocation. The probe cleaned all temporary data and returned to the same post-intro heap/largest-block boundary.

Conclusion:

```text
resident raw BSP + resident pointer-heavy runtime = rejected architecture
```

## Native BSP reader hardware proof

Source:

```text
DoomRPG-ESP32.pak
entry = /intro.bsp
offset = 1945016
size   = 21823
window = 256 B
```

Header:

```text
name       = Entrance
loadMapId  = 1
spawn      = 904
direction  = 64
camera     = 648
floorTex   = 145
ceilingTex = 112
```

Exact structural inventory:

```text
nodes          = 223
lines          = 480
mapSprites     = 344
events         = 93
byteCodes      = 265
strings        = 94
stringData     = 7779 B
legacyStringAlloc = 7873 B
maxString      = 313 B
```

Integrity/performance:

```text
bytes     = 21823 / 21823
readCalls = 86
elapsed   = 161 ms on inventory+plan pass
FNV-1a    = d5cc751f
CRC32     = 623f34e4
verified  = yes
trailing  = 0
```

Earlier inventory-only hardware run measured `125 ms`; the additional semantic resource analysis increased the one-shot pass to `161 ms`. This is map-load work, not frame-loop work.

## Exact BSP section offsets

Offsets are payload-relative and hardware-regression checked:

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

Future native population can therefore use direct `EspAssetPack_readRange()` section reads.

## Map-derived resource sets

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

The 0..255 bitsets are complete for `/intro.bsp`. Future maps must remain fail-closed if overflow counters become non-zero.

`textureReq=33` and `spriteReq=45` are **map-derived dependencies only**. They do not include every global gameplay sprite/resource that the legacy engine used to inject in code. Global/native ownership will be designed deliberately.

The recovered `EV_CHANGESPRITE` dependency path uses opcode `34`; this BSP contains no such opcode. No current map sprite uses the sprite-as-tile resource path either.

## Compact native structural plan

First persistent native baseline:

```text
nodes          = 2230 B
lines          = 4800 B
mapSprites     = 1720 B
events         = 372 B
byteCodes      = 2385 B
stringOffsets  = 188 B
blockMap       = 256 B
planeMap       = 2048 B
resourceSets   = 96 B
-------------------------
persistent     = 14095 B
```

Compared with the measured legacy structural payload:

```text
legacy structural = 55341 B
native base plan   = 14095 B
saved              = 41246 B
reduction          ~= 74.5%
```

`14095 B` is a **structural base**, not the final whole-game RAM cost.

Included: immutable compact map records, scripts/events, packed blockmap/plane map, string offsets and bounded resource sets.

Excluded: mutable entities/monsters, player state, doors/script overlays, dynamic sprite links/order, custom/drop pools, cache payloads, renderer working buffers and string scratch.

Preferred runtime model:

```text
compact immutable map base
        +
small mutable overlays addressed by integer index
```

Do not expand every record into pointer-heavy desktop objects.

## Zero-drift hardware proof

Final tested pass:

```text
heap8     84376 -> 84376  delta=0
largest8  36852 -> 36852  delta=0
frameFNV  8e274563 -> 8e274563
```

The final story frame hash is run-timing-specific; equality across the native pass is the invariant.

Later `[ALIVE]` heartbeats remained stable.

## Stable earlier recovery references

The previous full catalog is archived unchanged, but the following values remain especially useful:

```text
logical framebuffer           = 160x120 RGB565 = 38400 B
normal full-screen Present    ~= 42.7 ms
wall LRU3 peak payload        = 6144 B
sprite LRU3 peak payload      = 6038 B
menu persistent used          = 14092 B
fresh Start cleanup recovered = 55416 B
intro teardown recovered      = 33768 B on PR #41
```

Intro clock regression hashes:

```text
t=0     56438966
t=50    da9cd50e
t=100   c63cf367
t=200   2620e850
t=1000  e76fec13
```

Menu/touch/FNV details and older per-build heap values remain in [`archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md`](archive/PORTING_STATUS_PRE_MAP1_NATIVE_PASS1.md).

## Merge recommendation

**Merge this branch. No additional hardware test is required for this milestone.**

The final hardware-affecting code was tested at:

```text
45833b68b0e185630b1e5a769e54a051196c70e8
```

Changes after that point are documentation-only.

After merge, branch fresh from the new `main`.

## Next bounded milestone after merge

Objective: make the compact native structural base genuinely resident, then PARK again before gameplay.

Recommended boundary:

```text
post-intro page-3 PARK
    -> native BSP inventory regression
    -> allocate one compact arena or deliberately few pools
    -> populate nodes/lines/mapSprites/events/byteCodes by readRange()
    -> build uint16 string-offset table, leave text payload on SD
    -> copy packed blockMap + plane map
    -> retain bounded map-derived resource bitsets
    -> add only the minimum index-based mutable overlays required by the next consumer
    -> measure exact heap/largest-block cost
    -> PARK
```

Still forbidden in that next milestone:

```text
shapeData/mediaTexels
full raw BSP allocation
legacy Render_beginLoadMapData lifecycle
entity spawn/game world activation
ST_PLAYING
continuous gameplay loop
```

First prove the native map base can live in RAM at the planned cost. Rendering/gameplay consumers come after that boundary is hardware-stable.
