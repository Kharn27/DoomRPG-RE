# Milestone — ESP32 consolidation: native main-menu Back ownership

Date: 2026-09-30

## Boundary

```text
base main = 9c0498f2b74b1f15888a6789b24582ef4aa55093
branch = agent/esp32-consolidation-main-menu-back-v9
hardware-tested code head = 8d9b6d25f481d76fe4c79bc1af99d59bc314328f
esp32-cyd CI #1149 = SUCCESS
static RAM = 45768 B
flash = 801425 B
firmware.bin = 801792 B
artifact id = 11101750149
artifact sha256 = e355b8f0b41fccb692928d7109dbba51afba9de999abbce582756b61b91b706d
```

Relative to merged main #1145:

```text
RAM static: unchanged at 45768 B
Flash:      801373 -> 801425 B (+52 B)
firmware:   801744 -> 801792 B (+48 B)
```

The small size increase buys an explicit bounded native Back contract instead of
the generic legacy hierarchy router.

## Goal

Retire `MenuSystem_back()` from the linked ESP32 firmware for the pre-game
main-menu domain without broadening the milestone into generic menu routing.

The only hardware-relevant callers in this domain were:

```text
MENU_MAIN_HELP_ABOUT -> MENU_MAIN
MENU_MAIN_OPTIONS    -> MENU_MAIN
```

Both now use one semantic native action:

```text
child-specific touch/frame precondition
 -> DoomRPG_esp32MainMenuReturnToMain()
    -> validate ST_MENU + expected child + oldMenu == MENU_MAIN
    -> play exact legacy Back sound 5042 directly
    -> native_main_menu_model enter MENU_MAIN
    -> opaque native main repaint
    -> main touch re-arm
    -> strict final FNV/model/touch/graphics invariants
```

The action fails closed and uses the already-hardware-proven common MENU_MAIN
recovery path when a child frontend cannot complete its return.

## Linker boundary

Final code head:

```text
MenuSystem_back       ABSENT
MenuSystem_setMenu    PRESENT (445 B)
MenuSystem_playSound  PRESENT
```

`MenuSystem_setMenu()` remains intentionally out of scope because retained
legacy flows still call it directly, including `DoomCanvas_setupmenu()`.

`MenuSystem_playSound()` also remains linked independently of Back, including
through retained `DoomCanvas_setState()` behavior. Therefore this milestone
claims only retirement of the Back router itself.

## HELP -> Back hardware proof

The real classic CYD first exercised native bounded Help parsing and paging:

```text
[MAINMODEL] HELP-PARSE bytes=1405 ... declared=83 parsed=83 ... trailing=207 ... result=valid
[MAINHELP] PAGE-DOWN scroll=0->8
[MAINHELP] PAGE-DOWN scroll=8->16
[MAINHELP] PAGE-UP scroll=16->8
[MAINHELP] PAGE-UP scroll=8->0
```

The page-zero framebuffer returned exactly to its prior FNV
`5f22cf6b`.

The Back transition then executed through the native semantic owner:

```text
[MAINBACK] BEGIN source=help child=2 menu=2 old=1 selected=0 state=2 frame=5f22cf6b
[MAINMODEL] ENTER target=1 type=4 old=-1 items=4 selected=0 ...
[MAINOPAQUE] ... finalFNV=522dc605 ...
[MAINBACK] READY source=help child=2->1 frame=522dc605 touch=rearmed elapsedMs=154 sound=5042 router=native noMenuSystemBack=yes noSetMenu=yes
[MAINHELP] READY back=native mainFNV=522dc605 touch=armed
```

Heap remained stable:

```text
heap8=20916
largest8=10740
```

## OPTIONS -> Back hardware proof

The real classic CYD entered the bounded native Options dashboard, armed the Back
card, and confirmed it:

```text
[MAINOPTIONS] READY explicit MENU_MAIN -> MENU_MAIN_OPTIONS composition
[OPTIONBACK] CONFIRM Back action=native-semantic-return
```

The semantic return then proved the same parent contract:

```text
[MAINBACK] BEGIN source=options child=7 menu=7 old=1 selected=0 state=2 frame=73303639
[MAINMODEL] ENTER target=1 type=4 old=-1 items=4 selected=0 ...
[MAINOPAQUE] ... finalFNV=522dc605 ...
[MAINBACK] READY source=options child=7->1 frame=522dc605 touch=rearmed elapsedMs=154 sound=5042 router=native noMenuSystemBack=yes noSetMenu=yes
[OPTIONBACK] FAST End framebufferFNV=522dc605 expected=522dc605 runtimeFNV=522dc605 menu=1 selected=0 touchActive=1 shapeData=0x0 mediaTexels=0x0
[OPTIONBACK] READY native semantic Back + opaque bounded repaint; no MenuSystem_back, no MENUWALL/MENUSPRITE replay
```

Heap again remained stable at `heap8=20916`, `largest8=10740`.

## Result / next seam

The pre-game main-menu domain no longer requires the generic legacy
`MenuSystem_back()` router. HELP and OPTIONS return explicitly to MENU_MAIN
through permanent native ownership.

The next larger seam is `MenuSystem_setMenu()`, but it must not be removed
blindly. It is still called by retained desktop-derived flows outside this
pre-game Back boundary. The next consolidation milestone should first classify
those remaining callers and select one coherent ownership family rather than
recreating a generic router.

Any commit after
`8d9b6d25f481d76fe4c79bc1af99d59bc314328f` is documentation-only before
merge.
