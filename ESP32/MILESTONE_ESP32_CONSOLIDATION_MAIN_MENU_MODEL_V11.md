# ESP32 consolidation milestone — native main-menu model ownership V11

Date: 2026-09-30

## Boundary

```text
repo = Kharn27/DoomRPG-RE
base main = 071febee7ec88958286fb74d82cee9ba61083a85
branch = agent/esp32-consolidation-legacy-init-anchor-v10
source-consolidation hardware PASS = e5c52be0ed24c57df65477a1bed4efbf914d8320
final hardware-tested code head = 8cd0b019ebcb80491a27c2ed0bad3cf57f9ad47e
CI #1175 = SUCCESS
artifact id = 11110371227
artifact digest = sha256:31e9f777f42f22917fde7a72421c7e216b285a1cdf941949540722532af178c3
firmware.bin = 782624 B
firmware sha256 = d66e4a1eac3f7911046916de9aa89dcb9a7d4c8e944f9424f7747534c3f181d0
firmware.elf sha256 = 00d14a306c726d44737beca1a14aaab395ecd1aa09c0d0d7c7d92b3b84c86d71
static RAM = 45224 B
linked Flash = 782265 B
```

## Goal

Retire broad legacy pre-game menu model construction without inventing another
generic router, while also reducing the fragmented main-menu source topology.

The permanent boundary remains semantic:

```text
MENU_MAIN
 -> START
 -> LOAD
 -> OPTIONS
 -> HELP
```

## Source consolidation

Before the model extraction, the hardware-tested cleanup
`e5c52be0ed24c57df65477a1bed4efbf914d8320` reduced
`native_main_menu_*.c` from 13 source files to 7, with no new file.

Mechanical moves:

- LOAD implementation -> `native_main_menu_actions.c`
- tap gate / PlatformInput wrapper -> `native_main_menu_touch.c`
- presentation/recovery + native-scene bridge -> `native_main_menu_touch_layout.c`
- obsolete pre-touch layout source removed
- historical overlay probe source removed

CI #1171 remained essentially binary-neutral:

```text
RAM = 45768 B
Flash = 797689 B
firmware.bin = 798048 B
```

Real-CYD HELP, OPTIONS and complete START then passed on this boundary.

## Native fixed-model extraction

At `8cd0b019...`, the three active ESP32 callers of `Menu_initMenu()` are
replaced by explicit construction in the already-existing
`native_main_menu_model.c`.

Supported models are deliberately bounded:

- `MENU_MAIN`: exact legacy four-record shape, followed by the existing
  finger-first adaptation to START / LOAD / OPTIONS / HELP;
- `MENU_MAIN_CONTINUE`: retained fixed three-record compatibility model;
- `MENU_MAIN_OPTIONS`: retained fixed Back / Video / Input / Sound model;
- `MENU_MAIN_HELP_ABOUT`: existing bounded `help.txt` parser over PAK.

There is no `native_initMenu(id)` replacement switch.

The final correction also ensures `native_main_menu_model.h` is visible before
all external builder calls. The earlier experimental version had accidentally
relied on an implicit declaration in an overlay probe; that version was never
accepted as merge-ready.

## Final ELF

Direct final-ELF `nm -S --size-sort`:

```text
Menu_initMenu                         ABSENT
Menu_LoadHelpResource                 ABSENT
Menu_startGame                        0x49 = 73 B
DoomRPG_esp32MainMenuModelBuildMain   0xbb = 187 B
DoomRPG_esp32MainMenuModelEnter       0x45f = 1119 B
```

The discarded closure includes the broad menu switch and unrelated helpers such
as `Menu_fillStatus`, `Menu_setStore`, `Menu_setNotes`,
`MenuSystem_buildDivider`, `Menu_setYesNo`, plus the 544 B static
`vendingMenuTable`.

Important linker lesson: a function listed in `firmware.map` at address
`0x00000000` can belong to a garbage-collected input section. Final
`firmware.elf` symbol tables are authoritative for reachability.

## Resource delta

Against the immediately preceding hardware-tested source-consolidation head:

```text
                 e5c52be       8cd0b019       delta
static RAM       45768 B        45224 B        -544 B
Flash           797689 B       782265 B      -15424 B
firmware.bin    798048 B       782624 B      -15424 B
```

The developer's local PlatformIO upload build reported 45224 B RAM,
782281 B Flash and a 782640 B image, an absolute 16-byte Flash/image difference
from CI. The CI delta and final-ELF symbol result remain exact.

## Real classic-CYD proof

Cold boot observes the V10 retirement marker directly:

```text
[LEGACYINIT] STAGED runtime=ESP32-core/layout/startup legacy-DoomRPG_Init-anchor=retired
```

Normal menu boot:

```text
[MAINOPAQUE] DASHBOARD modelFNV=292c7f95 ... finalFNV=522dc605 ... heap8=21460 largest8=10740
[BOOT] NORMAL READY mainMenuFNV=522dc605 heap8=21460 largest8=10740 shapeData=0x0 mediaTexels=0x0
```

HELP:

```text
[MAINMODEL] ... builder=native-bounded-help
[MAINHELP] PAINT ... framebufferFNV=5f22cf6b
[MAINHELP] PAGE-DOWN ... framebufferFNV=d0788359
[MAINMODEL] ENTER target=1 ... builder=native-fixed-main
[MAINBACK] READY source=help ... frame=522dc605 ... noSetMenu=yes
```

OPTIONS:

```text
[MAINMODEL] ENTER target=7 ... builder=native-fixed-options
[MAINOPTIONS] Model ... modelFNV=e1ef01f7
[MAINOPTIONS] framebufferFNV=162d3999 ... shapeData=0x0 mediaTexels=0x0
[MAINMODEL] ENTER target=1 ... builder=native-fixed-main
[OPTIONBACK] FAST End framebufferFNV=522dc605 ... shapeData=0x0 mediaTexels=0x0
```

START:

```text
[MAINSTART] Begin ... heap8=21460 largest8=10740 shapeData=0x0 mediaTexels=0x0
[MAINSTART] READY ... Menu_startGame(new) -> Player_reset -> ST_INTRO
[INTRODISP] READY ... heap8=43104->76876 recovered=33772 ... noMapLoad=yes
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93076 heap8=27368 largest8=18420 ...
```

The +544 B free heap at both menu and final gameplay relative to the preceding
hardware PASS exactly matches the 544 B static-RAM reduction.

## Result

PASS on real classic CYD.

The pre-game menu no longer needs the broad desktop `Menu_initMenu()` factory,
and the linker removes its unrelated legacy closure. The main-menu source domain
is also materially less fragmented.

Post-test changes must remain documentation-only before merge.
