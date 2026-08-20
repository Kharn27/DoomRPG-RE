# ESP32 first gameplay BSP structural load

Branch: `agent/esp32-map1-structural-load`

Base merged `main`:

```text
897e982f4b37039d984b13265beaa68a83dce98b
```

That base is PR #41, the hardware-validated bounded intro teardown.

Status: **REAL-CYD MEASUREMENT PASS; LEGACY STRUCTURAL PATH SAFELY REFUSED; NATIVE LOADER NEXT; BRANCH CONTINUES**.

## What this milestone actually delivered

This increment did **not** implement the final gameplay map loader.

It implemented a bounded measurement scaffold that answers a more important
question first: can the reverse-engineered desktop/J2ME-derived structural loader
be allowed to instantiate the first post-prologue BSP on the classic no-PSRAM
CYD without crossing the validated memory boundary?

The real-CYD answer is now measured:

```text
NO — the legacy resident-BSP + resident-runtime working set does not fit.
```

The probe refused before allocation, cleaned its temporary mappings/BSP data and
returned to the exact post-intro safe boundary without reset or heap drift.

That refusal is the successful result of this measurement milestone, not a failed
attempt that should be forced through by weakening the guard.

## Permanent architecture interpretation

DoomRPG-RE is an **executable specification and data-format reference** for the
ESP32 work. It is not an architecture that the final CYD engine is required to
retain.

The final ESP32 engine may completely stop compiling the desktop-derived engine
sources. Names and calls such as:

```text
Render_t
DoomCanvas_t
Render_beginLoadMap()
Render_beginLoadMapData()
DoomCanvas_run()
```

are currently useful compatibility/probe scaffolding while behavior and formats
are being recovered. They are not permanent engine boundaries.

The target direction is:

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

PR #41 proved the classic CYD can park after the intro resources have been
released and before any gameplay map load:

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

The current probe build has a small expected baseline shift. Immediately after
its intro teardown it measured:

```text
heap8     = 84384
largest8  = 36852
```

## Important map-ID clarification

`startupMap == 1` is **not** `level01.bsp`.

The recovered engine enums and `Game_init()` mapping are:

```text
MAP_MENU     = 0
MAP_INTRO    = 1 -> /intro.bsp
MAP_SECTOR01 = 2 -> /level01.bsp
```

So the first post-prologue gameplay BSP opened by the original lifecycle is:

```text
MAP_INTRO / /intro.bsp
```

`/level01.bsp` remains later work.

## Measurement scaffold

The branch added a fail-closed structural probe around the original loader. Its
intended sequence was:

```text
post-intro PARK / page 3
    -> ZIP/memory preflight
    -> temporary real Render_loadMappings()
    -> parse raw /intro.bsp to calculate exact structural allocations
    -> if safe, release probe BSP
    -> real Render_beginLoadMap(MAP_INTRO)
    -> real Render_beginLoadMapData()
    -> stop before Render_loadBitShapes()
```

The probe deliberately forbids:

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

The permanent no-PSRAM rule also remains:

```text
shapeData   == NULL
mediaTexels == NULL
```

A linker wrapper was prepared to block the first `Render_loadBitShapes()` call if
the structural phase proved safe. The real hardware preflight refused earlier,
so the legacy structural allocator was never entered on this validation run.

## Real-CYD measurement

Validation used the normal optimized profile:

```text
esp32-cyd
```

not `esp32-cyd-bringup`.

### Intro teardown still passes

This probe build entered disposal at:

```text
heap8     = 50620
largest8  = 13300
```

and returned:

```text
heap8     = 84384
largest8  = 36852
recovered = 33764 B
```

The four-byte difference versus the PR #41 `33768 B` teardown measurement is a
normal build-to-build baseline shift. The resource sequence and post-dispose
largest block remain consistent.

### ZIP working sets

At the post-intro boundary:

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

Both individual legacy inflates fit the initial boundary.

### Mapping cost

The temporary original mapping loader measured:

```text
heap8     84384 -> 75944
largest8  36852 -> 36852
resident cost   = 8440 B
```

The legacy `mappingMemory=-8440` diagnostic has reversed accounting sign; the
heap measurement above is authoritative.

### Exact `/intro.bsp` structural inventory

The bounded parser consumed the complete BSP exactly:

```text
nodes          = 223
lines          = 480
mapSprites     = 344
runtimeSprites = 368   (344 + 16 custom + 8 drop)
events         = 93
byteCodes      = 265
strings        = 94
stringBytes    = 7873
parsed         = 21823 / 21823 B
trailing       = 0 B
```

This is now a primary reference for the native loader. It proves the parser's
understanding of the file layout reaches the exact end of the real resource.

### Legacy structural allocation plan

Using the current ESP32 build's desktop-derived runtime structures, the planner
calculated:

```text
structural allocation payload = 55341 B
largest single allocation     = 15360 B
safety headroom               = 4096 B
```

With mappings and the raw uncompressed BSP simultaneously resident:

```text
heap8 available       = 54104 B
largest8              = 20468 B
required + headroom   = 59437 B
largest allocation    = 15360 B
```

So the largest individual allocation would fit, but the total simultaneous
working set would not.

The deficit is:

```text
with 4096 B safety headroom:
  59437 - 54104 = 5333 B short

with ZERO safety headroom:
  55341 - 54104 = 1237 B short
```

Therefore the refusal is **not caused by an overly conservative 4096-byte guard**.
The desktop-derived lifecycle genuinely requires more free heap than exists even
if the safety margin is removed entirely.

## Why the desktop-derived loader loses

The key architectural problem is not `/intro.bsp` itself. The file is only
21,823 bytes uncompressed.

The problem is the simultaneous lifecycle:

```text
mappings resident
+ complete uncompressed BSP resident
+ progressively allocated runtime structures
```

The original loader does not release the raw BSP until after nodes, lines,
sprites, scripts, strings, blockmap and plane references have all been
instantiated.

On a desktop this is harmless. On the no-PSRAM CYD it wastes exactly the memory
we need to construct the level.

Trying to shave roughly five kilobytes until this legacy path happens to pass is
therefore the wrong architectural goal.

## Safe refusal and cleanup proof

The hardware emitted:

```text
[MAP1STRUCT] REFUSED structural working set does not fit with raw BSP resident
```

before entering the real structural allocator.

The fail path then:

```text
free temporary probe BSP
Render_freeRuntime()
    -> releases mappings/reference arrays
    -> releases any partial runtime fields
return to PARK
```

Later normal-firmware heartbeats were stable:

```text
[ALIVE] ... heap8=84384 largest8=36852 ...
[ALIVE] ... heap8=84384 largest8=36852 ...
```

No reset, OOM, hidden map transition or heap drift occurred. The probe ended on
the same logical boundary from which it started.

## Result classification

This run is classified as:

```text
measurement / safety-gate PASS
legacy structural loading feasibility = REFUSED
final gameplay loader                 = NOT IMPLEMENTED YET
```

This branch therefore continues. There is no reason to merge merely to record a
measurement scaffold before implementing the native replacement it motivated.

## Next implementation on this branch: native streaming BSP loader

The next code should stop calling `Render_beginLoadMapData()` for this path.

Preferred first architecture:

```text
                SD ZIP entry
                    |
                    v
             streaming DEFLATE
                    |
              small input/output
                   buffers
                    |
                    v
              EspBspReader
                 /     \
                /       \
          pass 1         pass 2
         inventory       populate
             |              |
             v              v
          exact plan -> ESP32-native map runtime
```

Names are provisional; the architectural ownership is not.

### Pass 1 — inventory / validation

Stream `/intro.bsp` without retaining the full 21,823-byte uncompressed file.
Validate and count:

```text
nodes
lines
sprites
events
bytecodes
strings / string payload
blockmap
plane/resource references
```

No gameplay runtime structures need to exist yet.

### Controlled allocation

After pass 1, allocate the final native runtime from exact measured counts.
Prefer consolidated pools and bounded arrays over many independent heap
allocations. Allocate the largest/most critical pools deliberately so
fragmentation is measurable rather than accidental.

The native structures are **not required** to match `Node_t`, `Line_t` or
`Sprite_t`. Their fields, widths and ownership should be chosen for the ESP32
renderer/gameplay consumers we actually need.

Immediate optimization candidates discovered by this measurement include:

```text
480 desktop-derived Line_t entries
  -> current largest allocation = 15360 B

94 strings
  -> one packed string pool + offset table is preferable to many allocations

344 map sprites / 368 runtime sprites
  -> native compact representation can use indexes instead of desktop pointers

223 BSP nodes
  -> child/adjacency indexes can replace pointer-heavy desktop layout where useful
```

Do not prematurely choose exact packed sizes until their real consumers are
mapped.

### Pass 2 — populate directly

Restart the ZIP stream, parse the same BSP again and write directly into the
already-sized native pools.

The complete raw BSP should never coexist with the complete runtime.

Two DEFLATE passes cost extra SD/CPU time but dramatically reduce peak RAM. On
this target that is the correct trade until hardware measurements prove a
one-pass strategy is both simpler and equally bounded.

### Mappings are no longer assumed resident

The current probe measured the old mapping arrays at **8440 B**. A native BSP
inventory does not automatically need those arrays resident.

Resource IDs can be inventoried first; native resource/mapping data can then be
loaded or represented only when the native renderer/resource manager needs it.
The next design should therefore treat `Render_loadMappings()` as another legacy
behavior to understand, not as a mandatory permanent dependency.

## Performance philosophy for gameplay

No fixed gameplay FPS target is being imposed yet.

Doom RPG is turn-based. Gameplay logic/input responsiveness must not be tied to
panel presentation rate. The preferred architecture is demand-driven:

```text
static scene
  -> no pointless continuous full-screen redraw

player action / animation
  -> update game state
  -> render only the frames that have visual value

render cadence
  != game-turn cadence
  != input polling cadence
```

The intro currently feels acceptable despite measuring around 14 rendered FPS.
Gameplay animations can tolerate a lower cadence than a real-time shooter, but
`5 FPS` is not being frozen as a specification either. Once the native map and
renderer path exist, hardware measurements and perceived smoothness will decide
where optimization effort is worthwhile.

Priority order remains:

```text
correctness
-> bounded RAM
-> stable input/game logic
-> correct visuals
-> measured performance optimization
```

## Next hardware success criterion

The next meaningful PASS for this branch is no longer "make the old structural
loader fit".

It is:

```text
post-intro heap boundary
    -> native streaming pass 1 over /intro.bsp
    -> exact same structural inventory as this probe
    -> no full 21823 B BSP allocation
    -> bounded/no-leak RAM
    -> PARK
```

Expected inventory regression target:

```text
nodes=223
lines=480
mapSprites=344
runtimeSprites=368
events=93
byteCodes=265
strings=94
stringBytes=7873
parsed=21823
```

Only after that native pass-1 parser is hardware-proven should the branch advance
to native allocation + pass-2 population.
