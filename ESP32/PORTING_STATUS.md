# Doom RPG ESP32 CYD porting status

This file is the **authoritative recovery point** for the classic
ESP32-2432S028R Doom RPG port.

Use [`README.md`](README.md) for the stable build/architecture guide and
[`DOCUMENTATION.md`](DOCUMENTATION.md) for documentation ownership rules.
Detailed milestone logs are preserved in the linked archive files instead of
being recopied here.

## Hardware recovery baseline

Latest merged `main`:

```text
PR   = #41 — bounded intro disposal
main = 897e982f4b37039d984b13265beaa68a83dce98b
```

PR #41 is the latest merged hardware-affecting baseline.

Current development branch:

```text
branch    = agent/esp32-map1-structural-load
base main = 897e982f4b37039d984b13265beaa68a83dce98b
status    = REAL-CYD MEASUREMENT PASS; LEGACY STRUCTURAL PATH SAFELY REFUSED;
            NATIVE STREAMING BSP LOADER NEXT; BRANCH CONTINUES
```

This branch is intentionally **not merge-ready yet**. It has produced a safe
hardware measurement and the design evidence needed to replace the legacy map
loader, but it has not yet implemented the final native gameplay BSP runtime.

### Current safe boundary

Merged PR #41 proves the real CYD can park after bounded intro teardown and
before any gameplay map load:

```text
menu                    = MENU_NONE
state                   = ST_INTRO (9)
storyPage               = 3
storyTextPage           = 0
intro clock             = inactive
intro input             = inactive
imgSpaceBG              = NULL
imgLinesLayer           = NULL
imgPlanetLayer          = NULL
imgSpaceship            = NULL
storyText1[0]           = NULL
storyText1[1]           = NULL
storyText2              = NULL
render clip             = off
startupMap              = 1
heap8                   = 84408 on PR #41 validation build
largest8                = 36852
nodes                   = NULL
lines                   = NULL
mapSprites              = NULL
mediaTexelOffsets       = NULL
mediaBitShapeOffsets    = NULL
mapTextureTexels        = NULL
mapSpriteTexels         = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite LRU caches  = inactive
DoomCanvas_run          = NOT called
DoomCanvas_loadMap      = NOT called
```

The PR #41 teardown recovered exactly **33,768 B** of 8-bit heap:

```text
heap8     50640 -> 84408
largest8  13300 -> 36852
recovered          33768 B
```

The observed final intro framebuffer hash was `a7ee546a` both before and after
resource destruction. This hash is run-timing-specific; the regression contract
is equality before/after teardown, not one fixed final story hash.

On the current MAP_INTRO probe build, normal-firmware code-size/state movement
shifted the same logical post-dispose boundary slightly to:

```text
heap8     = 84384
largest8  = 36852
```

The MAP_INTRO feasibility probe temporarily loaded mappings plus a preflight BSP,
refused the unsafe legacy structural working set, called the existing fail cleanup
and returned to exactly `heap8=84384`, `largest8=36852`. Later `[ALIVE]`
heartbeats remained stable at that boundary.

## Current execution path

Validated normal-firmware path plus current measurement:

```text
video / SD / ZIP
    -> transitional Doom RPG core/layout startup
    -> menu structural startup
    -> direct opaque MENU_MAIN
    -> semantic XPT2046 menu input
    -> confirmed fresh Start Game
    -> dead-resource cleanup
    -> Player_reset behavior
    -> ST_INTRO + real prologue strings/images
    -> fitted deterministic t=0 story frame
    -> ESP32-owned 50 ms intro clock
    -> bounded More / Continue touch progression
    -> final intro-exit-ready PARK at ST_INTRO page 2
    -> one-shot bounded intro resource teardown
    -> ST_INTRO page 3 with intro assets/texts NULL
    -> MAP_INTRO feasibility probe arms
    -> inventory /intro.bsp
    -> legacy structural working set REFUSED before allocation
    -> temporary mappings/BSP released
    -> PARK again at post-intro memory boundary
```

The next real code path must be an ESP32-native BSP reader/runtime, not an attempt
to weaken the guard until `Render_beginLoadMapData()` happens to fit.

## Permanent architecture invariants

Target:

- ESP32-2432S028R / classic CYD
- ESP32-D0WD-V3, dual core, 240 MHz
- 4 MB flash
- no PSRAM
- ILI9341 320x240 landscape
- XPT2046 touch
- microSD backing store
- logical framebuffer 160x120 RGB565 = 38,400 B
- exact nearest-neighbor 2x panel presentation
- audio still deferred

Permanent graphics/resource direction:

```text
shapeData   == NULL
mediaTexels == NULL
```

The port must not resurrect the original monolithic `shapeData` or map-wide
`mediaTexels` architecture. SD/native resources must be consumed through bounded
working sets, caches or streams.

Permanent engine ownership rule:

```text
DoomRPG-RE = executable specification / format + behavior reference
final CYD engine = our ESP32-native engine
```

Desktop-derived types/functions and linker wrappers are migration scaffolding,
not permanent architecture. The final ESP32 build may stop compiling the desktop
engine entirely once native components own the required contracts.

## Documentation / development discipline

1. Branch from the exact latest merged hardware-validated `main`.
2. One branch = one coherent bounded objective; a measurement may legitimately
   reshape the implementation while staying on that branch.
3. Build/flash/test on the real classic CYD.
4. Fail closed before known unsafe working sets.
5. Preserve detailed evidence in a milestone document when it has long-term value.
6. Update this recovery point as the branch boundary changes.
7. Merge only when implementation, real hardware and documentation form a coherent boundary.
8. After merge, mark the milestone document as a historical merged archive.

## Merged milestone timeline

| PR | Milestone | Merge/reference SHA |
| ---: | --- | --- |
| #32 | fast opaque Options -> Main return without replaying MENUWALL/MENUSPRITE | `cc2cb40cf026b5a5e232dba67f884905aca42488` |
| #33 | normal/bring-up boot split + loading-bar flicker suppression | `1035d4413686624feb07aaf208821946cead5869` |
| #34 | permanent bring-up touch-hitbox overlay | `2b29ca7f3479c9add022ccc803bdbff7dd5ade34` |
| #35 | hardware-selected CYD color profile | `a87b50747fa69bba6624870f944cbb1111014276` |
| #36 | fresh Start Game entry + lifecycle cleanup | `5275e4a1c6eca703b51221e80f3b199178015a01` |
| #37 | first fitted deterministic `ST_INTRO` frame | `b934e21c7f2dbf6463a4d2dfa13d1e06614e2b96` |
| #38 | bounded ESP32-owned 50 ms intro clock | `58edfe5d7080a7e9e64ff5b516697ddf3cca31da` |
| #39 | full bounded intro touch progression | `7ba68955a9b0979924c5e759736fb483589be744` |
| #40 | documentation ownership / recovery cleanup | `98378ce94da6480bbc8939830c0453514d389c82` |
| #41 | bounded intro resource teardown + RAM recovery | `897e982f4b37039d984b13265beaa68a83dce98b` |

Current active hardware/design milestone:

```text
agent/esp32-map1-structural-load
  -> prove startupMap=1 is MAP_INTRO / /intro.bsp
  -> inventory the complete BSP on real hardware
  -> measure legacy mapping + structural working sets
  -> refuse unsafe resident-BSP + resident-runtime lifecycle
  -> return to stable post-intro PARK
  -> continue with ESP32-native streaming BSP loader
```

Earlier validated work leading to PR #32 includes native asset pack v2, zero
resident monolithic graphics pools, bounded wall/sprite frames and caches, real
menu BSP traversal, native projected wall/sprite rasterization, the fitted
160x120 main menu and semantic touch selection.

Detailed recent milestone documents:

- [`START_GAME.md`](START_GAME.md) — PR #36
- [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) — PR #37
- [`INTRO_CLOCK.md`](INTRO_CLOCK.md) — PR #38
- [`INTRO_INPUT.md`](INTRO_INPUT.md) — PR #39
- [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md) — PR #41 evidence
- [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) — current active branch

## Native graphics recovery references

These values are regression/bring-up references and should remain available even
though the normal path no longer replays every proof pass.

```text
sprite 172 texel FNV          = 0c0a7acd
wall 112 texel FNV            = 92d40704
synthetic projected wall FNV  = ad191f54
real walls framebuffer        = a6d87c4a
viewSprites list FNV          = 962cd657
sprite request FNV            = 4457ac94
walls + sprites framebuffer   = ffe0995e
faithful original MENU_MAIN   = 86c38260
historical fitted MENU_MAIN   = 1afa0223
failed double-gray wall frame = b6f86faa
```

Validated LRU evidence:

```text
Wall LRU3
  logical requests = 25
  hits             = 14
  physical misses  = 11
  evictions        = 8
  peak payload     = 6144 B

Sprite LRU3
  logical requests = 11
  hits             = 2
  physical misses  = 9
  evictions        = 6
  peak payload     = 6038 B
```

## Menu runtime and presentation recovery

Normal menu-map structural load stops before the legacy monolithic graphics
phase. Hardware runtime while `MENU_MAIN` is active:

```text
nodes          = 53
lines          = 120
mapSprites     = 44
runtimeSprites = 68
events         = 15
mapTextures    = 84
mapSpriteRefs  = 284
planeTextures  = 11
persistent used= 14092 B
shapeData      = NULL
mediaTexels    = NULL
```

Main menu model:

```text
Start Game
Options
Help/About
Exit
```

Fitted logical geometry:

```text
screen      = 160x120
logo target = 90x62 at 35,2
Start Game  y=67
Options     y=79
Help/About  y=91
Exit        y=103
```

Menu regression references:

```text
MENU_MAIN model FNV            = bbc2149b
layout FNV                     = 47b3656e
black + scaled logo            = 0ac1f9c6
Start Game selected            = 58a11171
Options selected               = 0cf107b1
Help/About selected            = 9db82b71
Exit selected                  = bdd775f9
MENU_MAIN_OPTIONS model        = e1ef01f7
MENU_MAIN_OPTIONS framebuffer  = 6058d47d
```

Start's real-CYD touch needed a narrow top tolerance after a measured tap landed at
logical `y=64` while the visible label starts at `y=67`:

```text
Start Game logical x=28..119 y=64..78
Options    logical x=28..119 y=79..90
Help/About logical x=28..119 y=91..102
Exit       logical x=28..119 y=103..114
```

Options Back remains:

```text
logical  x=15..119 y=65..78
physical x=30..239 y=130..157
```

Native menu activation must explicitly establish:

```text
MENU_MAIN visible + touch armed
    => DoomCanvas.state == ST_MENU (2)
```

because the optimized menu painter bypasses the original expensive
`MenuSystem_setMenu()` side effects.

## Touch platform contract

Current platform semantics:

```text
press edge -> one semantic callback
hold       -> no repeat
release    -> 50 ms stable-release debounce
next press -> next callback
```

Menu code applies its own select/confirm policy above this platform behavior.
Intro input reuses the same callback; no second XPT2046 poller exists.

Bring-up overlay meaning:

```text
red rectangle     = accepted logical hitbox scaled 2x
cyan cross        = last calibrated semantic touch
small yellow ring = exact touch center
Serial            = raw / physical / logical coordinates
```

The overlay is drawn after framebuffer presentation and never changes logical
framebuffer hashes.

A disposal-validation run produced a mild tester impression of having to insist
on the first `More`, but the relevant semantic taps were both accepted inside the
prompt band:

```text
logical 101,109 -> REVEAL accepted
logical 113,105 -> MORE accepted
```

This remains a non-blocking touch/re-arm UX observation rather than a
state-machine or hitbox failure.

## Fresh Start Game contract

Strict preconditions at confirmed Start:

```text
menu        = MENU_MAIN
selected    = 0
state       = ST_MENU
frame FNV   = 58a11171
shapeData   = NULL
mediaTexels = NULL
wall/sprite caches inactive
```

Fresh no-save action currently follows the recovered original behavior:

```text
MENU_MAIN / item 0
    -> MenuSystem_select()
    -> Menu_select(MENU_MAIN, 0)
    -> Menu_startGame(menu, 1)
    -> Player_reset()
    -> DoomCanvas_setState(ST_INTRO)
    -> DoomCanvas_loadPrologueText()
```

This is executable-spec scaffolding; equivalent behavior may later be owned by
native ESP32 types instead of these legacy functions.

The existing-save precheck occurs before destructive cleanup. Existing-save mode
keeps menu runtime because `MENU_MAIN_CONTINUE` still needs it.

### Fresh-start lifecycle cleanup

The optimized menu lifecycle otherwise leaves two classes of dead resources alive:

1. `imgLegals` / `g.bmp`, normally freed by the skipped legal-screen state path;
2. `menu.bsp` runtime/mappings, no longer needed after fresh New Game is confirmed.

Fresh Start therefore executes today:

```text
DoomRPG_freeImage(imgLegals)
Render_freeRuntime(render)
Game_unloadMapData(game)
```

Original measured build:

```text
heap8     29064 -> 84480
largest8  17396 -> 36852
recovered         55416 B
```

Later PR #39 build:

```text
heap8     29008 -> 84424
largest8  17396 -> 36852
recovered         55416 B
```

The exact 55,416-byte recovery is stable despite small build-to-build baseline
movement.

### Fresh Player reset contract

```text
level           = 1
currentXP       = 0
nextLevelXP     = 80
credits         = 0
keys            = 0
ammo[1]         = 8
weapon          = 2
weapons         = 0x00000004
disabledWeapons = 0
totalDeaths     = 0
```

## Intro resource plan

The current prologue loader uses the original three text buffers plus four packed
indexed BMP textures:

```text
c.bmp -> imgSpaceBG
  ZIP c/u       = 3675 / 12408 B
  packed pixels = 12288 B
  192x128, 4-bpp, palette 16

d.bmp -> imgLinesLayer
  ZIP c/u       = 149 / 12356 B
  packed pixels = 12288 B
  192x128, 4-bpp, palette 3

e.bmp -> imgPlanetLayer
  ZIP c/u       = 1352 / 8312 B
  packed pixels = 8192 B
  128x128, 4-bpp, palette 16

f.bmp -> imgSpaceship
  ZIP c/u       = 114 / 160 B
  packed pixels = 45 B
  9x9, 4-bpp, palette 8
```

ZIP totals:

```text
compressed   = 5290 B
uncompressed = 33236 B
```

The legacy ZIP peak is per file. After fresh-start cleanup all four assets load
successfully through the packed indexed BMP path; a native `.pak` migration was
not required to complete the intro milestone.

PR #41 measured teardown recovery:

```text
c.bmp / imgSpaceBG      +12436 B
d.bmp / imgLinesLayer   +12384 B
e.bmp / imgPlanetLayer   +8340 B
f.bmp / imgSpaceship      +164 B
storyText1[0]              +172 B
storyText1[1]              +116 B
storyText2                 +156 B
-------------------------------
total                    +33768 B
```

## Intro rendering recovery

### Black entry boundary

After real fresh Start/prologue loading:

```text
menu            = MENU_NONE
state           = ST_INTRO (9)
framebuffer FNV = 485915c5
storyPage       = 0
storyTextPage   = 0
```

`485915c5` is the black 160x120 RGB565 entry framebuffer.

### Story fit

Original 128x128 story coordinates do not fit 120 logical pixels vertically. The
first direct renderer produced valid pre-fit hardware FNV `6cf52a3e` but cropped
four logical pixels at top and bottom.

Final fitted mapping:

```text
virtual story space = 128x128
ESP32 viewport      = 120x120
viewport origin     = x20,y0
logical framebuffer = 160x120
physical story      = centered 240x240 at exact 2x
```

No intermediate framebuffer is allocated.

Final deterministic first fitted frame:

```text
t=0 FNV                 = 56438966
first-frame build heap8 = 50704 -> 50704
largest8                = 13300 -> 13300
```

See [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md) for full evidence.

## Intro clock recovery

Clock model:

```text
virtual step = 50 ms
nominal time = 0, 50, 100, 150, ...
max render   = one due frame per loop service
late ticks   = skipped, never catch-up rendered
```

Stable framebuffer checkpoints:

```text
t=0 ms     FNV=56438966
t=50 ms    FNV=da9cd50e
t=100 ms   FNV=c63cf367
t=200 ms   FNV=2620e850
t=1000 ms  FNV=e76fec13
```

PR #38 clock build RAM:

```text
heap8        = 50672 -> 50672 per rendered frame
largest8     = 13300 -> 13300 per rendered frame
deltaHeap    = 0
deltaLargest = 0
```

At virtual `t=1000 ms`:

```text
ticks elapsed    = 20
frames rendered  = 14
ticks skipped    = 6
effective render ~= 14 FPS
```

See [`INTRO_CLOCK.md`](INTRO_CLOCK.md) for detailed timing evidence.

## Intro input recovery

Fitted input geometry:

```text
story viewport = x20..139 y0..119
prompt band    = x20..139 y102..119
```

Across two PR #39 real-CYD captures, every bounded intro input branch is validated:

```text
out-of-band prompt touch          -> MISS PASS
page 0 / text 0 early reveal      -> PASS
page 0 / text 0 -> More           -> PASS
page 0 / text 1 early reveal      -> PASS
page 0 -> page 1 Continue         -> PASS
page 1 natural timeout -> page 2  -> PASS
page 1 touch skip -> page 2       -> PASS
page 2 early reveal               -> PASS
final Continue -> safe PARK       -> PASS
```

Natural page-1 transition:

```text
page-1 epoch = t=9150
AUTO-PAGE    = t=19200
elapsed      = 10.05 s
```

Touch-skip validation path:

```text
REVEAL page0/text0  t=2050
MORE                t=3300
REVEAL page0/text1  t=4600
CONTINUE page0->1   t=5400
SKIP-ANIM page1->2  t=7300
REVEAL page2        t=8400
FINAL-CONTINUE      t=9150
PARK tick=183 frames=115 skipped=68
```

The PR #39 build remained:

```text
heap8    = 50656
largest8 = 13300
```

through all measured frame and input transitions.

See [`INTRO_INPUT.md`](INTRO_INPUT.md) for both complete hardware paths.

## Intro disposal recovery

PR #41 mirrors only the resource-release behavior needed at the end of the intro
and deliberately omits the legacy immediate `DoomCanvas_loadMap(startupMap)`
transition.

PR #41 validation before disposal:

```text
state      = ST_INTRO (9)
storyPage  = 2
startupMap = 1
frame FNV  = a7ee546a
heap8      = 50640
largest8   = 13300
clip       = on
```

After freeing four images plus three story text buffers:

```text
state      = ST_INTRO (9)
storyPage  = 3
frame FNV  = a7ee546a (unchanged)
heap8      = 84408
largest8   = 36852
clip       = off
assets     = NULL
texts      = NULL
map load   = NOT called
```

The exact PR #41 measured heap recovery is **33,768 B**. All runtime map pools
remain NULL, native caches remain inactive, and `shapeData` / `mediaTexels`
remain NULL. Three later heartbeats stayed stable.

Current MAP_INTRO probe build repeated the same lifecycle with a small baseline
shift:

```text
before teardown  heap8=50620 largest8=13300
after teardown   heap8=84384 largest8=36852
recovered        33764 B
```

Do not replace the historical PR #41 figure with this later build-specific one.

See [`INTRO_DISPOSE.md`](INTRO_DISPOSE.md) for complete per-resource evidence.

## MAP_INTRO `/intro.bsp` feasibility measurement

### Map identity

Recovered enum/resource mapping:

```text
MAP_MENU     = 0
MAP_INTRO    = 1 -> /intro.bsp
MAP_SECTOR01 = 2 -> /level01.bsp
```

Therefore `startupMap=1` enters `/intro.bsp`; it is not yet `level01.bsp`.

### ZIP working sets

Normal `esp32-cyd` hardware measurement from the post-intro boundary:

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

### Legacy mapping arrays

Temporary `Render_loadMappings()` measured:

```text
heap8     84384 -> 75944
largest8  36852 -> 36852
resident cost   = 8440 B
```

The legacy negative `mappingMemory` sign is ignored; heap8 is authoritative.

### Exact structural inventory

The preflight parser consumed the resource exactly:

```text
nodes          = 223
lines          = 480
mapSprites     = 344
runtimeSprites = 368
events         = 93
byteCodes      = 265
strings        = 94
stringBytes    = 7873
parsed         = 21823 / 21823 B
trailing       = 0 B
```

These counts are regression targets for the next native reader.

### Why the legacy runtime is refused

Current desktop-derived runtime allocation plan:

```text
structural payload     = 55341 B
largest allocation     = 15360 B
safety headroom        = 4096 B
```

With mappings and the raw uncompressed BSP resident:

```text
heap8                  = 54104 B
largest8               = 20468 B
need + headroom        = 59437 B
largest needed block   = 15360 B
```

The largest allocation fits; aggregate memory does not.

Deficit:

```text
with 4096 B guard = 5333 B
with zero guard   = 1237 B
```

Therefore the guard is not the cause. Even with no safety margin, the old
resident-BSP + resident-runtime lifecycle is too large.

The probe emitted a fail-closed `[MAP1STRUCT] REFUSED`, freed temporary BSP and
mappings/runtime via cleanup, then heartbeats remained:

```text
heap8     = 84384
largest8  = 36852
```

No OOM/reset or hidden loader transition occurred.

See [`MAP1_STRUCTURAL_LOAD.md`](MAP1_STRUCTURAL_LOAD.md) for the complete design
interpretation and next native-loader plan.

## Current memory baselines

Build-to-build baseline movement is expected as small code/state objects are
added. The critical contract is that bounded render/input/load operations do not
allocate unexpectedly and that each milestone records its own exact build.

```text
interactive normal-menu, earlier build
  heap8    = 29064
  largest8 = 17396

interactive normal-menu, PR #39 build
  heap8    = 29008
  largest8 = 17396

bring-up menu with hitbox diagnostics
  heap8    = 28592
  largest8 = 17396

fresh cleanup, earlier build
  heap8    = 84480
  largest8 = 36852

fresh cleanup, PR #39 build
  heap8    = 84424
  largest8 = 36852

fresh Start/prologue PR #36 boundary
  heap8    = 50712
  largest8 = 13300

fitted first-frame build
  heap8    = 50704
  largest8 = 13300

intro-clock build
  heap8    = 50672
  largest8 = 13300

intro-input PR #39 build/final PARK
  heap8    = 50656
  largest8 = 13300

intro-dispose PR #41 validation, before teardown
  heap8    = 50640
  largest8 = 13300

intro-dispose PR #41 validation, after teardown
  heap8     = 84408
  largest8  = 36852
  recovered = 33768 B

MAP_INTRO probe build, before teardown
  heap8    = 50620
  largest8 = 13300

MAP_INTRO probe build, post-teardown / post-safe-refusal PARK
  heap8     = 84384
  largest8  = 36852
  teardown recovered in this build = 33764 B
```

Do not attribute small baseline differences to a particular allocation unless a
measurement isolates the cause.

## Display / pacing recovery

Hardware-selected profile:

```text
gamma       = 1.00
saturation  = 1.15
resampling  = nearest
```

Representative panel timing:

```text
normal full-screen Present          ~= 42.7 ms
first fitted ST_INTRO Present       = 42.761 ms
active intro Present                ~= 42.67..42.84 ms
bring-up Present + physical overlay ~= 44.3 ms
old neutral Present                 ~= 34.4 ms
```

The 50 ms virtual intro clock is a functional/RAM PASS but does not physically
sustain 20 rendered FPS. It skips stale virtual ticks by design and measures
roughly 14 rendered FPS.

Gameplay does **not** inherit a mandatory 20 FPS target. Doom RPG is turn-based,
so game-turn logic and input should be decoupled from render cadence. The desired
runtime is demand-driven:

```text
static scene -> no continuous redraw
input/game turn -> responsive regardless of panel FPS
animation -> present only useful visual frames
```

A low animation rate may be acceptable on this device, but no `5 FPS` target is
frozen yet. Native gameplay rendering must exist first; then optimize from real
hardware/perceived smoothness. `PlatformVideo_present()` and the saturation pass
remain measured candidates.

## Current deferred work

Still intentionally deferred:

- native BSP pass-2 population and final compact map representation
- native gameplay resource mapping/lookup policy
- Game/entity migration into native runtime
- player spawn/final gameplay state
- active normal gameplay loop and controls
- existing-save Continue / New Game submenu painter/action
- Video/Input/Sound menu actions
- Help/About and Exit real actions
- presentation optimization after native gameplay measurement
- intro touch re-arm/polish only if the `More` UX observation persists
- audio

## Next bounded implementation — same branch

Do **not** merge the current branch merely because the legacy feasibility probe
measured a safe refusal. Continue on:

```text
agent/esp32-map1-structural-load
```

Next objective is an ESP32-native streaming reader for `/intro.bsp`.

### Native pass 1 — inventory

From the stable post-intro boundary:

```text
SD ZIP entry /intro.bsp
    -> streaming DEFLATE with small bounded buffers
    -> native BSP reader
    -> validate/count only
    -> no full 21823 B BSP allocation
    -> no final runtime allocation yet
    -> PARK
```

Hardware PASS must reproduce exactly:

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

while keeping bounded/no-leak RAM and never allocating the full uncompressed BSP.

### Then native allocation + pass 2

Only after pass 1 is hardware-proven:

```text
exact counts
    -> allocate compact ESP32-native pools deliberately
    -> restart stream
    -> populate final pools directly
    -> raw BSP never coexists with complete runtime
```

Candidate design improvements are allowed and expected:

- compact native line/node/sprite structures instead of desktop pointer-heavy layouts
- one string pool + offset table instead of many heap allocations
- indexes/offsets where pointers are unnecessary
- mapping/resource data loaded only when native consumers need it
- largest allocations placed deliberately to control fragmentation

Do not choose exact packed structures until their native gameplay/renderer
consumers are understood.

The permanent constraints remain:

```text
shapeData   == NULL
mediaTexels == NULL
no full raw BSP resident beside complete runtime
DoomRPG-RE desktop architecture is not a required dependency
```
