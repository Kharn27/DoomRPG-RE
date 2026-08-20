# ESP32 first gameplay BSP structural load

Branch: `agent/esp32-map1-structural-load`

Base merged `main`:

```text
897e982f4b37039d984b13265beaa68a83dce98b
```

That base is PR #41, the hardware-validated bounded intro teardown.

Status: **IMPLEMENTED; AWAITING REAL-CYD HARDWARE PASS**.

## Recovery boundary inherited from PR #41

The real classic CYD is currently parked after the intro resources have been
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

## Important map-ID clarification

`startupMap == 1` is **not** `level01.bsp` in the reverse-engineered engine.
The original enums and `Game_init()` mapping are:

```text
MAP_MENU     = 0
MAP_INTRO    = 1 -> /intro.bsp
MAP_SECTOR01 = 2 -> /level01.bsp
```

So this milestone is the first post-prologue gameplay BSP boundary, but the real
resource being opened is:

```text
MAP_INTRO / /intro.bsp
```

`/level01.bsp` is intentionally out of scope.

## Objective

Advance exactly one bounded step:

```text
post-intro PARK / page 3
    -> preflight mappings.bin + intro.bsp memory working sets
    -> real Render_loadMappings()
    -> inspect raw intro.bsp structural plan
    -> release preflight BSP
    -> real Render_beginLoadMap(MAP_INTRO)
    -> real Render_beginLoadMapData()
       nodes
       lines
       sprites
       events + bytecodes
       strings
       blockmap
       plane texture references
    -> raw BSP freed by original loader
    -> HARD STOP at first Render_loadBitShapes()
    -> PARK with structural runtime resident
```

This milestone deliberately does **not** execute:

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

The permanent no-PSRAM rule remains:

```text
shapeData   == NULL
mediaTexels == NULL
```

## Why the hard stop is safe and exact

The original `Render_beginLoadMapData()` order is:

```text
parse structural BSP data
    -> SDL_free(raw BSP ioBuffer)
    -> DoomCanvas_updateLoadingBar()
    -> Render_loadBitShapes()
    -> DoomCanvas_updateLoadingBar()
    -> Render_loadTexels()
```

The ESP32 linker wraps `Render_loadBitShapes()`. During normal menu startup and
all unrelated paths the wrapper delegates to the original function. Only while
the one-shot MAP_INTRO milestone is active does it:

1. capture the structural boundary;
2. NULL the already-freed `render->ioBuffer` pointer;
3. return `false` without calling the real bitshape loader.

The original `Render_beginLoadMapData()` therefore returns immediately and never
enters either legacy monolithic graphics loader.

No source-tree `Render.c` patch is needed.

## Intro-dispose lifecycle bridge

The already hardware-validated intro implementation is left untouched.
ESP32 linker wrappers bridge two calls across object boundaries:

```text
Esp32IntroDispose_reset()
    -> original reset
    -> reset MAP_INTRO structural one-shot state

Esp32IntroDispose_service()
    -> original dispose service
    -> MAP_INTRO structural service
```

After final Continue:

```text
loop N
  original intro dispose executes
  MAP1STRUCT sees dispose done -> ARMED only

loop N+1
  original disposer is already done
  MAP1STRUCT performs the bounded structural load
```

This preserves a distinct lifecycle checkpoint between resource teardown and BSP
loading.

## Memory preflight

The milestone refuses to enter the structural loader blindly.

### ZIP working-set checks

For both `mappings.bin` and `/intro.bsp`, Serial reports:

```text
compressed bytes
uncompressed bytes
ESP32 miniz state = 10992 B
compressed + uncompressed + inflate state
current heap8
current largest8 block
```

A legacy ZIP inflate is refused if its compressed payload, uncompressed payload,
or miniz state cannot fit the current largest block, or if the complete transient
set exceeds free 8-bit heap.

### Exact BSP structural plan

With mappings resident, `/intro.bsp` is temporarily decoded once for a bounded
preflight parser. The parser follows the exact byte layout consumed by the
original loader and measures:

```text
nodes
lines
map sprites
runtime sprites = map sprites + 16 custom + 8 drop
map events
bytecodes
strings and string payload bytes
blockmap
plane-texture references
```

It then calculates the actual target allocations using the ESP32 build's
`sizeof(Node_t)`, `sizeof(Line_t)` and `sizeof(Sprite_t)`, plus the fixed resource
reference arrays.

The preflight requires:

```text
planned structural payload + 4096 B safety headroom < heap with raw BSP resident
largest planned allocation <= largest block with raw BSP resident
```

The temporary preflight BSP is then released before the real loader starts.

If this check refuses the load on hardware, that is a **safe measurement result**,
not permission to weaken the guard. The next code change must reduce the working
set or change the resource strategy first.

## Expected Serial shape

Exact counts and RAM consumption are intentionally not predicted; measuring them
on the real CYD is the purpose of this milestone.

After the validated intro disposal logs:

```text
[INTRODISP] READY ... heap8=...->84408 ...
[INTRODISP] PARK ...
[MAP1STRUCT] ARMED post-intro boundary; MAP_INTRO structural load starts on next loop service

=== Doom RPG ESP32 first gameplay BSP structural load ===
[MAP1STRUCT] BEGIN state=9 page=3 mapId=1 enum=MAP_INTRO file=/intro.bsp heap8=... largest8=...
[MAP1STRUCT] CONTRACT real Render_beginLoadMap + structural Render_beginLoadMapData only; bitshapes/texels/entities/finalize forbidden
[MAP1STRUCT] ZIP mappings.bin ...
[MAP1STRUCT] ZIP /intro.bsp ...
[MAP1STRUCT] PREFLIGHT -> Render_loadMappings() first, then inspect BSP with mappings resident
[MAP1STRUCT] MAPPINGS READY ...
[MAP1STRUCT] PLAN nodes=... lines=... mapSprites=... runtimeSprites=... events=... byteCodes=... strings=... stringBytes=... parsed=.../... trailing=...
[MAP1STRUCT] PLAN allocPayload=... largestAlloc=... rawResidentHeap8=... rawResidentLargest8=... safetyHeadroom=4096
[MAP1STRUCT] PREFLIGHT PASS ...
[MAP1STRUCT] -> Render_beginLoadMap(map=1) real header/mappings path
[MAP1STRUCT] HEADER result=1 mapName='...' mapNameID=1 ... ioPos=33 ...
[MAP1STRUCT] -> Render_beginLoadMapData(); ...
[MAP1STRUCT] CAPTURE after BSP structural parse / before bitshapes+texels ...
[MAP1STRUCT] CAPTURE counts ...
[MAP1STRUCT] GATE Render_loadBitShapes blocked; legacy graphics tail not entered
[MAP1STRUCT] READY map=1 file=/intro.bsp name='...' nodes=... lines=... ...
[MAP1STRUCT] RAM heap8=...->... used=... largest8=...->... ...
[MAP1STRUCT] PARK state=9 page=3 mapId=1 entities=0 monsters=0 shapeData=0x0 mediaTexels=0x0 noBitShapes=yes noTexels=yes noGameEntities=yes noFinalize=yes
[ALIVE] ...
```

A `[MAP1STRUCT] REFUSED ...` or `[MAP1STRUCT] FAILED ...` message is a fail-closed
hardware result. Do not proceed to later loaders from that build.

## Hardware PASS criteria

PASS on the classic no-PSRAM CYD requires all of the following:

- PR #41 post-intro disposal still passes first;
- MAP1STRUCT arms only after the disposer reports done;
- the following loop starts with `state=ST_INTRO`, `storyPage=3`, `mapId=1`;
- the selected resource is `/intro.bsp` / `MAP_INTRO`;
- ZIP/memory preflight passes without OOM or reset;
- real `Render_loadMappings()` succeeds;
- the preflight parser reaches the complete structural end of the BSP;
- real `Render_beginLoadMap(MAP_INTRO)` reaches `ioBufferPos=33`;
- real `Render_beginLoadMapData()` reaches the bitshape gate;
- structural nodes/lines/sprites/events/reference arrays remain resident;
- `render->ioBuffer == NULL` after the boundary;
- `shapeData == NULL`;
- `mediaTexels == NULL`;
- wall/sprite native caches remain inactive;
- `Game.numEntities == 0` and `Game.numMonsters == 0`;
- no `Render_loadBitShapes()` body executes;
- no `Render_loadTexels()` executes;
- no `Game_loadMapEntities()` executes;
- no gameplay finalization/state transition occurs;
- no crash/reset;
- later `[ALIVE]` heartbeat(s) remain stable at the measured structural RAM
  boundary.

## Next boundary after hardware PASS

If the structural MAP_INTRO working set fits and remains stable, the next branch
will own the next original loading phase separately. Candidate decomposition:

```text
structural MAP_INTRO resident
    -> establish bounded native graphics/resource readiness for this map
    -> then Game_loadMapEntities() as another measured lifecycle step
    -> only later spawn/finalize/render gameplay
```

The exact next slice should be chosen from the measured counts/RAM produced by
this milestone, not guessed in advance.
