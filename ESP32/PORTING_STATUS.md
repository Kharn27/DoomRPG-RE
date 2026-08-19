# Doom RPG ESP32 CYD porting status

This file is the recovery point for the ESP32-2432S028R classic Cheap Yellow
Display port. Update it on the same branch as every hardware-validated increment,
before merge.

## Target

- ESP32-2432S028R / classic CYD, no PSRAM
- ESP32-D0WD-V3, dual core, 240 MHz
- 4 MB flash
- ILI9341 320x240 landscape
- XPT2046 touch
- microSD-backed game data
- internal framebuffer: 160x120 RGB565 = 38,400 B
- gameplay viewport: 160x80 at framebuffer y=20
- physical output: exact nearest-neighbour 2x to 320x240
- audio still disabled during bring-up

## Project direction

DoomRPG-RE is treated as an executable specification for behaviour, data formats
and useful rendering semantics, not as an architecture contract. The ESP32 port
is progressively becoming its own constrained engine:

- bounded deterministic RAM use
- SD as immutable backing store
- measured working sets and evidence-driven caches
- no monolithic `shapeData`
- no map-wide `mediaTexels`
- one shared 160x120 RGB565 framebuffer
- storage isolated behind GFXRM
- cache policy isolated from storage/rasterization
- original BSP, projection, game and menu behaviour preserved where useful
- one small hardware-validated subsystem per increment

Core philosophy:

> We are no longer forcing DoomRPG-RE onto ESP32. We are building an ESP32 Doom
> RPG engine from the behaviour and data model proven by DoomRPG-RE.

## Increment discipline

1. Start from the exact latest hardware-validated `main` SHA.
2. One branch = one small measurable objective.
3. Build/flash/test on the real CYD.
4. Fix failures on the same branch.
5. Only after hardware PASS, update all relevant `.md` files on that branch.
6. Only then is the branch merge-ready.
7. Merge.
8. Only after merge acknowledgement start the next increment.

Documentation is part of the increment, not a later cleanup task.

## Important merged milestones

- native 160x120 framebuffer + exact 2x TFT output
- real engine object graph / HUD / MenuSystem / Render startup
- real `menu.bsp` structural load stopped before legacy graphics inflation
- native asset pack v2, 241 random-access resources
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- bounded sprite/wall GFXRM frames
- native projected wall + sprite rasterization
- real `menu.bsp` BSP traversal, camera and visible scene
- wall LRU3: 25 logical requests -> 11 physical loads
- sprite LRU3: 11 logical requests -> 9 physical loads
- native scene framebuffer `ffe0995e`
- original `MENU_MAIN` model/logo/hand/font overlay, faithful layout `86c38260`
- TFT bring-up diagnostics disabled by default; Serial diagnostics retained
- clean game-owned display, PR #27 merge
  `da2de773765f9675c4fe9eea1cbc82cf24b7523c`
- fitted 160x120 `MENU_MAIN`, framebuffer `1afa0223`
- fitted-layout merge on `main`:
  `bf928efb3054b06cd0124acea940c5672cad927a`

## Current validated increment

Branch: `agent/esp32-main-menu-touch-select`

Base `main` SHA:

```text
bf928efb3054b06cd0124acea940c5672cad927a
```

Status: **HARDWARE VALIDATED, DOCUMENTED, READY TO MERGE**.

Objective: add the first real CYD menu input without activating gameplay or any
legacy menu transition yet.

The validated behaviour is deliberately split into two layers:

```text
first tap on another row
    -> hit-test physical touch
    -> update real MenuSystem.selectedIndex
    -> move the real p.bmp hand cursor

second released tap on the same row
    -> emit semantic CONFIRM
    -> do NOT execute MenuSystem_select() yet
```

This distinction matters: the hardware logs prove that double-tap confirmation
works, even though there is intentionally no visible menu transition in this
increment. `CONFIRM action=deferred` is therefore expected success, not a failed
double tap.

`Start Game` is the main reason to keep execution deferred here: the original
`MENU_MAIN` selection path can call `Menu_startGame()` immediately and enter
loading/gameplay code that has not yet been migrated to the constrained native
runtime.

## Touch input boundary

The XPT2046 path is now:

```text
XPT2046
   |
   v
PlatformInput
   |
   +--> calibrated physical 320x240 coordinates
   |
   +--> one semantic tap per press/release cycle
              |
              v
        MENU_MAIN hit-test
              |
              +--> SELECT
              `--> CONFIRM
```

`PlatformInput` no longer inherits the old ~80 ms Serial diagnostic throttle for
semantic input. A tap is delivered immediately on the press edge, then a new tap
is blocked until the panel has been released for 50 ms continuously.

This prevents a held finger from becoming an accidental second tap.

## Hardware-validated hit zones

The zones are derived from the already validated
`native_main_menu_160x120_layout.h` geometry, not duplicated as unrelated touch
magic numbers.

```text
                 logical 160x120        physical CYD 320x240
x for all rows   28..119                 56..239

Start Game       y=67..78                y=134..157
Options          y=79..90                y=158..181
Help/About       y=91..102               y=182..205
Exit             y=103..114              y=206..229
```

Hardware touches landed correctly in those rows, including the previously seen
physical point around `129,215`, which maps to the `Exit` row.

## Touch-ready menu presentation

The PR #28 fitted layout remains the geometry reference:

```text
logo target = 90x62 at 35,2
rows        = 67,79,91,103
layoutFNV   = 47b3656e
```

For movable touch selection, the text is now fixed at its centered position and
only the real hand cursor indicates selection. The earlier fitted layout shifted
the selected text by 2 px; retaining that shift would require repainting glyphs
when the hand moves.

The touch-ready presentation therefore uses a new deterministic composition
signature while preserving the same geometry, model, font and assets.

Progressive hardware hashes:

```text
after scaled logo        = 1e8bcfbb
after Start Game + hand  = c03215ab
after Options            = b2f6a68d
after Help/About         = 994a049d
after Exit / final       = cbc99461
```

The prior fitted-layout `1afa0223` remains a valid historical regression
reference for the selected-text-offset presentation. The new touch-ready initial
frame is:

```text
MENU_MAIN touch-ready framebuffer = cbc99461
```

## Cursor movement without scene rerender

A second 38,400-byte framebuffer is not used. Instead, the input layer stores the
four tiny framebuffer regions that can lie underneath the 13x10 hand cursor:

```text
4 rows * 13 * 10 * RGB565 = 1,040 B
```

A selection change does only:

```text
restore old 13x10 background
set MenuSystem.selectedIndex
draw real p.bmp hand at new row
present framebuffer
```

No BSP rerender and no SD read are needed for cursor movement.

Hardware logs confirmed:

```text
[MENUTOUCH] PREPARED handPatches=1040B rows=4 selectionStyle=hand-only textPosition=fixed
```

## Authoritative hardware validation

Boot/composition boundary:

```text
=== Doom RPG ESP32 MENU_MAIN touch-select layout ===
[MAINTOUCHLAYOUT] Begin sceneFNV=ffe0995e expected=ffe0995e priorFittedFNV=1afa0223 faithfulOriginalFNV=86c38260 heap8=28704 largest8=17396
[MAINTOUCHLAYOUT] Model FNV=bbc2149b items=4 selected=0
[MAINTOUCHLAYOUT] Geometry screen=160x120 logoDst=35,2 90x62 logoBottom=64 itemStart=67 line=12 rows=4 contentBottom=115 layoutFNV=47b3656e expected=47b3656e selectionStyle=hand-only
[MAINTOUCHLAYOUT] framebufferFNV=cbc99461 sceneFNV=ffe0995e priorFittedFNV=1afa0223 composeMs=54 shapeData=0x0 mediaTexels=0x0
[MAINTOUCHLAYOUT] End heap8=28704 largest8=17396 deltaFromStart=0 largestDelta=0
[MENUTOUCH] GATE READY initialSelected=0 firstSameTap=arm secondReleasedSameTap=confirm
[MENUTOUCH] READY physical=320x240 logical=160x120 scale=2 selected=0 initialFNV=cbc99461 patches=1040B releaseDebounce=50ms
```

`composeMs=54` is diagnostic only, not a regression requirement.

### Start Game confirmation detection

The first tap on the already selected boot row only arms confirmation. A second
released tap passes confirmation:

```text
[MENUTOUCH] ARM item=0 tap=1 selected=0 awaitingReleasedSecondTap=yes
[MENUTOUCH] GATE tap=2 CONFIRM-PASS item=0
[MENUTOUCH] CONFIRM item=0 text="Start Game" count=1 framebufferFNV=cbc99461 action=deferred
```

No game load occurred, by design.

### Real selection movement

```text
[MENUTOUCH] GATE tap=4 SELECT-ARM current=0 hit=1
[MENUTOUCH] SELECT 0->1 text="Options   " framebufferFNV=961109a7 previousKnown=961109a7 heap8=28704->28704 largest8=17396->17396
[MENUTOUCH] READY selection=1 selections=1 confirms=1 misses=0 noSceneRerender=yes noSDRead=yes

[MENUTOUCH] GATE tap=5 SELECT-ARM current=1 hit=2
[MENUTOUCH] SELECT 1->2 text="Help/About" framebufferFNV=e4eadfbb previousKnown=e4eadfbb heap8=28704->28704 largest8=17396->17396
```

### Bit-identical cursor restoration

Returning from Help/About to Options reproduced the exact first Options frame:

```text
first Options framebuffer  = 961109a7
return Options framebuffer = 961109a7
```

Hardware log:

```text
[MENUTOUCH] SELECT 2->1 text="Options   " framebufferFNV=961109a7 previousKnown=961109a7 heap8=28704->28704 largest8=17396->17396
```

The same deterministic restoration was observed for Start Game:

```text
Start Game framebuffer = cbc99461
```

and Exit:

```text
Exit framebuffer = 5ff2a5cd
```

## Hardware-validated selection hashes

```text
selected Start Game = cbc99461
selected Options    = 961109a7
selected Help/About = e4eadfbb
selected Exit       = 5ff2a5cd
```

These are useful regression signatures for the hand-only touch-ready menu.

## Confirmation semantics validated

Hardware produced real `CONFIRM-PASS` + `CONFIRM` events for multiple rows:

```text
Options:
[MENUTOUCH] GATE tap=7 CONFIRM-PASS item=1
[MENUTOUCH] CONFIRM item=1 text="Options   " count=2 framebufferFNV=961109a7 action=deferred

Start Game:
[MENUTOUCH] GATE tap=11 CONFIRM-PASS item=0
[MENUTOUCH] CONFIRM item=0 text="Start Game" count=3 framebufferFNV=cbc99461 action=deferred

Exit:
[MENUTOUCH] GATE tap=15 CONFIRM-PASS item=3
[MENUTOUCH] CONFIRM item=3 text="Exit      " count=5 framebufferFNV=5ff2a5cd action=deferred
```

Therefore the double-tap state machine is hardware validated. There is currently
no visual result after `CONFIRM` because action dispatch is deliberately out of
scope for this branch.

## Memory boundary

Touch movement retained exact allocator values in the hardware run:

```text
heap8    = 28704 before / 28704 after
largest8 = 17396 before / 17396 after
```

Every shown selection change reported:

```text
heap delta    = 0
largest delta = 0
noSceneRerender=yes
noSDRead=yes
```

The 1,040-byte cursor background store is static bounded state; there is no
per-tap heap allocation.

`shapeData` and `mediaTexels` remain `NULL`.

## Deterministic signatures

Useful current regression boundaries:

```text
sprite 172 texel FNV              = 0c0a7acd
wall 112 texel FNV                = 92d40704
synthetic projected wall FNV      = ad191f54
real walls framebuffer            = a6d87c4a
viewSprites list FNV              = 962cd657
sprite request FNV                = 4457ac94
walls + sprites framebuffer       = ffe0995e
faithful MENU_MAIN layout         = 86c38260
ESP32 160x120 layout config       = 47b3656e
prior fitted MENU_MAIN            = 1afa0223
touch-ready Start Game selected   = cbc99461
touch-ready Options selected      = 961109a7
touch-ready Help/About selected   = e4eadfbb
touch-ready Exit selected         = 5ff2a5cd
```

## Display ownership remains clean

Normal mode remains:

```text
Serial
  `--> startup/probe/debug/touch diagnostics

TFT
  `--> shared game framebuffer only
```

`DOOMRPG_ESP32_SCREEN_DIAGNOSTICS=0` remains the default. The old bring-up visual
tools can still be temporarily restored with value `1`.

## Hardware visual observation: color/contrast

The fitted geometry and touch selection are visually successful on hardware.
The hand cursor follows the selected row correctly.

Comparison against a J2ME reference capture still shows the CYD presentation
looks noticeably flatter / less saturated / lower contrast. This remains a
separate recorded visual issue. Do not change palette values blindly; investigate
it in a dedicated measured increment.

## Current safe stop boundary

Validated and executed:

- complete engine startup through mappings
- real menu BSP structural data
- native asset-pack access
- zero resident `shapeData`
- zero map-wide `mediaTexels`
- bounded GFXRM wall/sprite frames
- hardware-validated wall and sprite LRU caches
- native projected walls and BSP-sorted sprites
- deterministic native scene `ffe0995e`
- original `MENU_MAIN` model and real UI assets
- faithful original menu composition `86c38260`
- clean TFT diagnostics policy
- ESP32-specific fitted menu geometry
- calibrated physical 320x240 touch hit-testing
- real `MenuSystem.selectedIndex` changes from touch
- independently movable real `p.bmp` hand cursor
- bit-identical 13x10 cursor-background restoration
- one semantic tap per released press cycle
- second released tap on same row detected as `CONFIRM`
- exact allocator restoration during touch movement

Still intentionally out of scope:

- execution of `MenuSystem_select()` from touch `CONFIRM`
- real transitions to Options / Help / Exit menus
- Start Game / gameplay loader activation
- original monolithic bitshape/texel loaders
- textured floor/ceiling planes
- persistent caches in the normal multi-frame runtime
- active normal multi-frame `ST_MENU` loop
- final color/contrast correction
- normal gameplay loop and gameplay control scheme
- audio

## Recommended next increment after merge

The touch frontend itself no longer needs architectural work. The next menu step
should connect **one safe real menu action** to the validated `CONFIRM` event,
rather than enabling all four actions at once.

A sensible first candidate is `Options`: route confirmed item 1 through the real
menu model and render the resulting options menu while keeping `Start Game`
deferred until gameplay loading is ready.

Keep the same discipline:

```text
one confirmed action
-> real original menu transition
-> native/ESP32 presentation
-> hardware test
```

Do not mix the unrelated color/contrast investigation into that input/action
increment.
