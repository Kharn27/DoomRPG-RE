# Milestone — ESP32 consolidation: native main-menu SELECT ownership

Date: 2026-09-30

## Boundary

```text
base main = 8e80d0c9da4d345b068df24c01505ee54c256717
branch = agent/esp32-consolidation-main-menu-select-v8
hardware-tested code head = cd5f24dd0ff538a85cebc02025de48cb5998401e
esp32-cyd CI #1135/#1136 = SUCCESS
static RAM = 45768 B
flash = 801373 B
firmware.bin = 801744 B
artifact id = 11097355233
artifact sha256 = caa4065b79b3e656fd0c00b37990d3d9c3ffdc5661ee102382562250856538ac
```

Relative to the merged base build (#1093, 45760 B RAM / 807377 B flash,
807744 B firmware.bin), the final reviewed/tested code costs 8 B static RAM,
saves 6004 B linked flash, and saves 6000 B in firmware.bin.

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
the temporary model container. MENU_MAIN and OPTIONS still use
`Menu_initMenu()` for bounded construction, but HELP no longer does: its
`help.txt` model is parsed by an ESP32-native bounded reader. The remaining
legacy construction dependency stays visible instead of being hidden behind
`MenuSystem_select()`.

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

## Review hardening before merge

A pre-merge review found three correctness issues and one structural smell. They
were fixed before declaring this milestone complete.

### Bounded Help parsing

The first native Help entry still delegated to legacy
`Menu_LoadHelpResource()`, whose parser had no file-size contract, no
`MAX_MENUITEMS` bound and unsafe line handling.

The final code reads `help.txt` through the PAK source with explicit bounds:

```text
asset size >= 3 and <= 4096 bytes
declared logical records in 1..MAX_MENUITEMS
every record must find its newline before physical EOF
every rendered line <= 31 characters
all malformed/truncated cases fail closed
temporary asset buffer freed on every path
```

The file format's original logical record count is retained only as a declared
count; it is never trusted as an unchecked memory-access bound. This matters
because valid `help.txt` contains bytes after the 83 logical Help records.

An initial over-parsing implementation correctly failed closed at item 96 on
the real CYD. That failure also provided the first hardware witness for the
common menu recovery path:

```text
[MAINMODEL] HELP-PARSE FAILED line/item bound item=96 len=0
[MAINACTION] FAILED dispatch item=3; recovery=attempt
[MAINRECOVER] READY reason=dispatch-failed menu=1 selected=0 frame=522dc605 touch=rearmed
```

The parser was then corrected to honor the verified logical record count while
bounding every byte read. The corrected head
`cd5f24dd0ff538a85cebc02025de48cb5998401e` was re-tested successfully on the
real classic CYD; Help navigation works again.

### Failure recovery

START/OPTIONS/HELP may disable the main callback before transferring ownership.
A failed transition now routes through one common
`DoomRPG_esp32MainMenuRecover()` owner. Recovery is allowed only while
`ST_MENU` still owns the UI; it refuses to drag a real ST_INTRO/ST_PLAYING
transition back into the menu.

Recovery rebuilds MENU_MAIN, repaints the opaque dashboard and re-arms touch.
The real-CYD Help parser failure above proves the previously dangerous
"visible but inert until reboot" condition is gone for this dispatch boundary.

Secondary Help/Options callback failures also use the same recovery owner.

### Typed LOAD outcomes

Main-menu LOAD no longer collapses all false returns into a recoverable
STAY_MAIN result. Its action reports:

```text
NO_SAVE
RECOVERED
TRANSITIONED
FATAL
```

The semantic dispatcher maps only NO_SAVE/RECOVERED to STAY_MAIN.
TRANSITIONED transfers ownership to gameplay; FATAL enters the common dispatch
recovery path when ST_MENU is still recoverable.

The already hardware-proven valid V9 LOAD success path remains unchanged apart
from this typed return boundary. The NO_SAVE/error-injection branches were not
artificially forced on hardware for this review.

### Presentation ownership cleanup

The repeated menu-local framebuffer hash and graphics-boundary checks were
centralized in `native_main_menu_present`, which also owns common recovery.
The active `native_main_menu_*` sources no longer carry local
`framebufferHash()` or `graphicsBoundaryIsSafe()` copies.

This moves the domain toward:

```text
native_main_menu_model
native_main_menu_present
native_main_menu_input/touch
native_main_menu_actions
```

without forcing a large file split during this milestone.

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
`cd5f24dd0ff538a85cebc02025de48cb5998401e` is documentation-only before
merge.
