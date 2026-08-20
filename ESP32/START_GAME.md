# ESP32 fresh Start Game entry and lifecycle cleanup

Historical milestone: **PR #36 — HARDWARE PASS — MERGED ARCHIVE**.

Base hardware-selected display milestone:

```text
main = a87b50747fa69bba6624870f944cbb1111014276
```

Merged to `main` at:

```text
5275e4a1c6eca703b51221e80f3b199178015a01
```

This archive preserves the detailed evidence behind the first real
`MENU_MAIN -> Start Game -> ST_INTRO` transition on the no-PSRAM CYD.
For the current recovery point, see [`PORTING_STATUS.md`](PORTING_STATUS.md).

## Objective

Execute the original fresh New Game semantics through the real menu action while
keeping the ESP32 bounded graphics architecture intact:

```text
MENU_MAIN / item 0
    -> MenuSystem_select()
    -> Menu_select(MENU_MAIN, 0)
    -> Menu_startGame(menu, 1)
    -> Player_reset()
    -> DoomCanvas_setState(ST_INTRO)
    -> DoomCanvas_loadPrologueText()
```

Gameplay map loading and the broad normal game loop remained deferred.

## Strict Start preconditions

The native menu path intentionally bypasses the expensive original menu painter,
but Start still requires the original state-machine contract:

```text
menu        = MENU_MAIN
selected    = 0
state       = ST_MENU
frame FNV   = 58a11171
shapeData   = NULL
mediaTexels = NULL
wall/sprite caches inactive
```

The optimized menu activation therefore explicitly synchronizes
`DoomCanvas.state` to `ST_MENU (2)` when the native `MENU_MAIN` becomes active.
Without this correction, a direct Start could fail until an Options -> Back round
trip happened to establish the state through the original menu transition.

Hardware marker:

```text
[MENUTOUCH] STATE SYNC canvas=0->2 source=native-MENU_MAIN activation
```

## First hardware attempt: intro ZIP OOM

The first real fresh Start reached `DoomCanvas_loadPrologueText()` correctly but
failed while inflating `c.bmp`.

At that point only about:

```text
heap8 = 29064 B
```

remained, while the legacy ZIP path temporarily needed the compressed payload,
uncompressed BMP output and a miniz inflate state of:

```text
10992 B
```

The resulting failure was an intentional `DoomRPG_Error()` abort/reboot, not a
logical return to the main menu. The state-machine path itself had reached the
correct prologue loader.

## Why dead memory was still resident

The native ESP32 boot intentionally skips parts of the original presentation
lifecycle. Two large resource groups were therefore still alive even though a
fresh irreversible New Game no longer needed them:

1. `imgLegals` / `g.bmp`, normally released by the original legal-screen state
   path;
2. `menu.bsp` runtime and mapping arrays, retained by the optimized opaque menu
   even though the menu map is no longer needed after fresh Start confirmation.

The existing-save path is detected **before** this cleanup and deliberately keeps
menu runtime because it still needs the Continue/New Game menu.

## Fresh-start lifecycle cleanup

For the fresh-profile path only, before the original `MenuSystem_select()` action:

```text
DoomRPG_freeImage(imgLegals)
Render_freeRuntime(render)
Game_unloadMapData(game)
```

Original hardware measurement:

```text
before cleanup
  heap8    = 29064
  largest8 = 17396

after cleanup
  heap8    = 84480
  largest8 = 36852

recovered = 55416 B
```

Released runtime contract:

```text
nodes                   = NULL
lines                   = NULL
mapSprites              = NULL
mediaTexelOffsets       = NULL
mediaBitShapeOffsets    = NULL
mapTextureTexels        = NULL
mapSpriteTexels         = NULL
shapeData               = NULL
mediaTexels             = NULL
wall/sprite caches      = inactive
```

A later PR #39 build measured `29008 -> 84424` with the **same exact 55,416-byte
recovery** and `largest8=36852`, confirming that the cleanup amount remained
stable despite small build-to-build baseline movement.

## Intro asset ZIP plan

The original prologue loader requests four indexed BMP assets:

```text
c.bmp -> imgSpaceBG
  ZIP compressed   = 3675 B
  uncompressed BMP = 12408 B
  packed pixels    = 12288 B
  192x128, 4-bpp, palette 16

d.bmp -> imgLinesLayer
  ZIP compressed   = 149 B
  uncompressed BMP = 12356 B
  packed pixels    = 12288 B
  192x128, 4-bpp, palette 3

e.bmp -> imgPlanetLayer
  ZIP compressed   = 1352 B
  uncompressed BMP = 8312 B
  packed pixels    = 8192 B
  128x128, 4-bpp, palette 16

f.bmp -> imgSpaceship
  ZIP compressed   = 114 B
  uncompressed BMP = 160 B
  packed pixels    = 45 B
  9x9, 4-bpp, palette 8
```

Totals:

```text
ZIP compressed   = 5290 B
ZIP uncompressed = 33236 B
```

The important peak is per file, not the sum of all four. After the lifecycle
cleanup, all four loaded successfully through the existing packed indexed ESP32
BMP path. No direct `.pak` migration was necessary for this milestone.

Representative loader evidence:

```text
[ZIP] inflate c.bmp ... state=10992
[SDL] Adopt packed indexed texture 192x128 bpp=4 bytes=12288 palette=16
[ZIP] inflate d.bmp ... state=10992
[SDL] Adopt packed indexed texture 192x128 bpp=4 bytes=12288 palette=3
[ZIP] inflate e.bmp ... state=10992
[SDL] Adopt packed indexed texture 128x128 bpp=4 bytes=8192 palette=16
[ZIP] inflate f.bmp ... state=10992
[SDL] Adopt packed indexed texture 9x9 bpp=4 bytes=45 palette=8
```

## Fresh Player reset contract

Hardware validation confirmed the original reset semantics exactly:

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

## Successful fresh Start boundary

After the real menu action and prologue resource load:

```text
menu            = MENU_NONE (0)
state           = ST_INTRO (9)
framebuffer FNV = 485915c5
heap8           = 50712
largest8        = 13300
shapeData       = NULL
mediaTexels     = NULL
```

The three prologue text allocations are non-NULL:

```text
storyText1[0] != NULL
storyText1[1] != NULL
storyText2     != NULL
storyPage      = 0
storyTextPage  = 0
```

Representative hardware sequence:

```text
[MAINSTART] Begin menu=1 selected=0 state=2 framebufferFNV=58a11171
[MAINSTART] Existing-save precheck=no -> fresh cleanup allowed
[MAINSTART] Fresh-start cleanup ... heap8=29064->84480 gained=55416
...
[MAINSTART] After select menu=0 state=9 framebufferFNV=485915c5 heap8=50712 largest8=13300
[MAINSTART] READY real MenuSystem_select -> Menu_startGame(new) -> Player_reset -> ST_INTRO
```

This was the validated handoff used by PR #37 to introduce the first bounded
story frame.

## Historical next boundary at merge time

After PR #36, the next objective was exactly one deterministic `ST_INTRO` frame,
with no broad game loop, input progression or map load. That work became PR #37
and is archived in [`FIRST_INTRO_FRAME.md`](FIRST_INTRO_FRAME.md).

The live roadmap is maintained only in [`PORTING_STATUS.md`](PORTING_STATUS.md).
