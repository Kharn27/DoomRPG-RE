# Milestone — ESP32 consolidation: native main-menu SELECT ownership

Date: 2026-09-30

## Boundary

```text
base main = 8e80d0c9da4d345b068df24c01505ee54c256717
branch = agent/esp32-consolidation-main-menu-select-v8
hardware-tested code head = cc0e14f7cc9e57c6d9617e715622675fda6de0fb
esp32-cyd CI #1102 = SUCCESS
static RAM = 45768 B
flash = 799997 B
firmware.bin = 800368 B
artifact id = 11095462691
artifact sha256 = c8265e77d2065ddce55dbc39a8cce5441c96f12812b8bbcfbe7b7c2288f36497
```

Relative to the merged base build (#1093, 45760 B RAM / 807377 B flash),
the final tested code costs 8 B static RAM and saves 7380 B flash.

## Goal

Retire the generic legacy main-menu SELECT dispatcher from the ESP32 runtime
without replacing it with another hidden compatibility layer.

The permanent user-visible main menu is four semantic actions:

```text
START   -> new game only
LOAD    -> native checkpoint resume
OPTIONS -> bounded options dashboard
HELP    -> bounded help viewer
```

The desktop/J2ME `MenuSystem_select() -> Menu_select() -> MenuSystem_setMenu()`
chain is no longer the owner of those selections.

## Native composition

The branch introduces explicit semantic composition:

```text
finger-first 2x2 touch gate
 -> DoomRPG_esp32MainMenuDispatchConfirmed()
    -> START
    -> LOAD
    -> OPTIONS
    -> HELP
```

A small transitional `native_main_menu_model` still uses `MenuSystem_t` as
the temporary model container and still calls `Menu_initMenu()` for bounded
menu construction. That dependency remains visible instead of being hidden
behind `MenuSystem_select()`.

The final ELF contains:

```text
Menu_select        ABSENT
MenuSystem_select  ABSENT
```

while the next seams remain explicit:

```text
MenuSystem_back     present
MenuSystem_setMenu  present
Menu_initMenu       present
Menu_startGame      present
```

## START dead-code cleanup

The original consolidation candidate retained a legacy save precheck around
START. Source review after hardware testing showed that the check used
`Game_checkConfigVersion()`, which looks for the desktop save quartet:

```text
Config
Player
Player2
World
```

The ESP32-native checkpoint is `/DoomRPG-ESP32.sav` (currently V9), so this
branch could not represent the actual LOAD path. It was dead legacy routing.

Commit `cc0e14f7cc9e57c6d9617e715622675fda6de0fb` removes that branch.
START now always executes a new game and LOAD is the sole checkpoint-resume
owner.

Hardware witness:

```text
[MAINSTART] Route=new-game-only savePresence=ignored loadOwner=dedicated-LOAD-card
[MAINSTART] READY explicit MENU_MAIN start composition -> Menu_startGame(new) -> Player_reset -> ST_INTRO
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

The real CYD then completed intro disposal, Entrance load, raw-flash staging,
cache priming and resident gameplay normally.

## OPTIONS proof

Real-CYD OPTIONS selection enters the bounded native model and shared dashboard
without legacy scene replay:

```text
[MAINACTION] DISPATCH action=2
[MAINMODEL] ENTER target=7 type=7 old=1 items=4 ... dispatcher=native
[MAINOPTIONS] READY explicit MENU_MAIN -> MENU_MAIN_OPTIONS composition
```

BACK remains intentionally transitional for the next milestone:

```text
[OPTIONBACK] CONFIRM Back action=MenuSystem_back+opaque-repaint
[MAINOPAQUE] READY finger-first MENU_MAIN painted without BSP/wall/sprite replay
```

The menu heap remained stable through the tested cycle.

## HELP proof

The first Help candidate exposed only the first visible page. Source comparison
with legacy `MENUTYPE_HELP` showed that Help is scrollable, so the final branch
implements bounded native paging rather than accepting the regression.

The hardware-tested owner shows eight lines at a time and a permanent
finger-first footer:

```text
BACK | UP | DOWN
```

Real-CYD proof:

```text
[MAINHELP] PAINT lines=83 visible=8 range=0..7 maxScroll=75
[MAINHELP] PAGE-DOWN scroll=0->8
[MAINHELP] PAINT ... range=8..15
[MAINHELP] PAGE-UP scroll=8->0
[MAINHELP] PAINT ... range=0..7
[MAINHELP] READY back=MenuSystem_back-transitional ... touch=armed
```

Returning to page zero reproduced the exact original page framebuffer FNV, and
BACK restored the exact opaque main-menu framebuffer before rearming touch.

Visual polish of the Help screen is deliberately deferred; ownership and
navigation are now native and bounded.

## LOAD proof

The dedicated LOAD card continues to own checkpoint resume. The final hardware
run loaded a real V9 Sector 1 checkpoint, rebuilt the compact map runtime,
restored player/resources/script/lines/action/crate/automap/monster topology and
position owners, reprime-cached the session, and published gameplay only after
loading release.

Key witness:

```text
[NATIVESAVE] READABLE-V9 ... result=valid
[NATIVESAVE] LOAD ... version=9 ... world=...restored-exact session=reprime-pending
[ENGINESESSION] RESUME checkpoint=restored ...
[ENGINESESSION] RESUME-VISIBLE map=2 ...
[ENGINESESSION] READY map=2 ... shapeData=0x0 mediaTexels=0x0
```

The exact final supplied run exercises a valid checkpoint. The no-save fallback
was not separately re-exercised in this final consolidation trace; its existing
dedicated LOAD implementation was not replaced by START.

## Result / next seam

The four visible main-menu cards now dispatch by explicit native semantic
ownership. Generic legacy SELECT is gone from the firmware.

The next bounded consolidation target is `MenuSystem_back()`. OPTIONS and HELP
still use that one explicit transitional return seam. Removing it should also
allow a fresh audit of whether `MenuSystem_setMenu()` can disappear or shrink
from the linked ESP32 image.

Any commit after
`cc0e14f7cc9e57c6d9617e715622675fda6de0fb` is documentation-only before
merge.
