# ESP32 first bounded intro frame

Branch: `agent/esp32-first-intro-frame`

Base hardware-validated `main`:

```text
5275e4a1c6eca703b51221e80f3b199178015a01
```

Status: **HARDWARE PASS; DOCUMENTED; MERGE-READY**.

## Objective

Advance exactly one boundary beyond the validated fresh Start Game result:
render and present one real deterministic `ST_INTRO` frame, fit the original
128x128 story presentation to the 160x120 CYD logical framebuffer, then park.

This increment intentionally does **not** start the original general game loop,
process intro input, advance story pages or load the gameplay map.

## Entry contract inherited from the Start Game milestone

Fresh Start reaches:

```text
menu            = MENU_NONE (0)
state           = ST_INTRO (9)
framebuffer FNV = 485915c5
shapeData       = NULL
mediaTexels     = NULL
```

`485915c5` is the FNV-1a of the completely black 160x120 RGB565 framebuffer
presented after `DoomCanvas_loadPrologueText()` has loaded the real intro assets.

The Start Game milestone measured:

```text
heap8    = 50712
largest8 = 13300
```

The final fitted build begins the bounded intro frame with `heap8=50704`; the
8-byte build-to-build difference is not a frame allocation. The draw itself
remains allocation-free, as proven by identical before/after heap measurements.

## First hardware result: original 128x128 story geometry

The first bounded frame successfully executed the original story renderer on the
classic CYD:

```text
entry FNV       = 485915c5
first-frame FNV = 6cf52a3e
heap8           = 50712 -> 50712
largest8        = 13300 -> 13300
deltaHeap       = 0
deltaLargest    = 0
state           = ST_INTRO (9)
storyPage       = 0
storyTextPage   = 0
```

`[ALIVE]` continued at 5 s and 10 s with the same heap/largest-block values, so
state, RAM and lifecycle behavior passed.

The visible frame exposed one remaining presentation mismatch: Doom RPG's story
renderer assumes a 128x128 viewport. On the deliberately 160x120 ESP32 logical
framebuffer:

```text
SCR_CX = 80
SCR_CY = 60
legacy story rect = x 16..143, y -4..123
```

The horizontal axis fits, but the vertical axis extends four logical pixels above
and below the framebuffer. The original `More` / `Continue` baseline also reaches
`y=124`, so its bottom was cropped.

`6cf52a3e` is retained as the **pre-fit hardware baseline**, not the final
regression hash for this milestone.

## ESP32-native 128 -> 120 story fit

The bounded ESP32 path now calls `Esp32StoryFit_draw()` instead of directly using
`DoomCanvas_drawStory()`.

The original coordinates remain the behavioral specification but are treated as
a virtual square mapped directly to the CYD framebuffer:

```text
virtual story space : 128x128
ESP32 viewport      : 120x120
viewport origin     : x=20, y=0
logical framebuffer : 160x120
physical TFT        : exact 2x -> 320x240
```

The resulting story occupies a centered physical 240x240 square with 40 physical
pixels of black margin on each side.

The native renderer transforms at draw time:

- scrolling space background
- progressive story font glyphs
- `More` / `Continue`
- menu hand
- animated intro layers
- spaceship
- laser lines and their clip rectangle

There is **no 128x128 intermediate framebuffer** and no heap allocation in the fit
renderer. Indexed textures are sampled directly by the existing ESP32
`SDL_RenderCopy()` scaler into the shared 160x120 RGB565 framebuffer.

Hardware marker:

```text
[INTROFIT] virtual=128x128 -> viewport=120x120@(20,0) direct-to-framebuffer; no intermediate buffer
```

## Bounded execution

After fresh Start succeeds, `DoomRPG_esp32RenderFirstIntroFrame()` verifies that:

- state is still `ST_INTRO`
- menu is `MENU_NONE`
- story page/text page are both zero
- all three prologue text allocations exist
- all four intro BMP textures exist
- released menu runtime remains released
- `shapeData == NULL`
- `mediaTexels == NULL`
- wall and sprite LRU caches remain inactive
- the shared platform framebuffer still hashes to `485915c5`

The bridge establishes a deterministic local intro epoch:

```text
canvas.time          = 0
canvas.storyTextTime = 0
canvas.storyAnimTime = 0
canvas.showTextDone  = false
```

and executes exactly once:

```text
Esp32StoryFit_draw(canvas)
DoomRPG_flushGraphics(doomRpg)
```

There is no call to `DoomCanvas_run()`, no input dispatch and no map load.

## Final hardware PASS

Validated normal-firmware Serial result:

```text
=== Doom RPG ESP32 bounded first ST_INTRO frame ===
[INTRO1] Begin state=9 menu=0 page=0 textPage=0 frameFNV=485915c5 expectedEntry=485915c5 heap8=50704 largest8=13300
[INTROFIT] virtual=128x128 -> viewport=120x120@(20,0) direct-to-framebuffer; no intermediate buffer
[INTRO1] Drawn t=0 frameFNV=56438966 heap8=50704 largest8=13300 deltaHeap=0 deltaLargest=0 state=9 page=0 textPage=0
[VIDEO] Present 160x120 -> 320x240 exact 2x + sat1.15: 42761 us
[INTRO1] READY one deterministic ST_INTRO frame presented once FNV=56438966
[INTRO1] PARK state=9 page=0 textPage=0; no DoomCanvas_run, no input dispatch, no map load
[MAINSTART] READY first ST_INTRO frame rendered/presented; engine remains parked
[ALIVE] uptime=10008 ms heap=116468 heap8=50704 largest8=13300 SD=ready ZIP=ready VIDEO=ready CORE=ready LAYOUT=ready PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready touchIRQ=idle
```

Final regression contract:

```text
entry framebuffer FNV = 485915c5
fitted intro FNV       = 56438966
heap8                  = 50704 -> 50704
largest8               = 13300 -> 13300
deltaHeap              = 0
deltaLargest           = 0
state                   = ST_INTRO (9)
storyPage               = 0
storyTextPage           = 0
shapeData               = NULL
mediaTexels             = NULL
wall/sprite caches      = inactive
```

Hardware visual validation confirms that the complete fitted story viewport is
visible and that the hand plus `More` are inside the display.

This is the merge-ready recovery point for the increment.

## Next milestone after merge

The next branch may add an **ESP32-owned timed multi-frame `ST_INTRO` clock** that
repeatedly calls `Esp32StoryFit_draw()` while preserving the same bounded memory
architecture.

Still deferred beyond this increment:

- active timed multi-frame intro progression
- intro touch/key handling (`More` / `Continue`)
- intro disposal / transition to loading
- first gameplay map load
- existing-save Continue / New Game submenu painter/action
- active normal game loop and gameplay controls
- audio
