# ESP32 consolidation milestone — production menu.bsp runtime retirement V14

Date: 2026-09-30

## Boundary

```text
repo = Kharn27/DoomRPG-RE
base main = 9aa3c3a62bb7639d884d036acc0baf54be88541e
branch = agent/esp32-consolidation-menu-bsp-runtime-retirement-v14
hardware-tested code head = dd4161a40b28d2ed9c88370b2f8281b9045f2980
code commits =
  a53025affaf2481079c601af59cf7931a34dfbcc
  dd4161a40b28d2ed9c88370b2f8281b9045f2980
CI #1197 = SUCCESS
artifact id = 11120007067
artifact digest = sha256:59e22d286fc5d80e6c1ecaca6ed3783b1726298b3dc606fef47ace01d87891c7
firmware.bin = 773456 B
firmware sha256 = 36a17cba2152a7bbdfaeb15ee2535453abaac45522afe59df41baa79116758a1
firmware.elf = 12270224 B
firmware.elf sha256 = 2e381df4b49e6e217bf6c6d52a64570b6fe1503418e1a88188cccd2f4dad9174
static RAM = 45128 B
linked Flash = 773089 B
active linker wraps = 52
```

## Goal

Remove the historical `menu.bsp` runtime from normal production startup now
that the visible pre-game UI is an opaque native dashboard with native semantic
actions.

Keep the old menu BSP structure/render chain available as a bring-up regression
suite without allowing it to define production ownership.

## Pre-V14 production shape

Normal `esp32-cyd` still performed:

```text
config + mappings
 -> DoomRPG_probeMenuBspHeader
 -> Render_beginLoadMap(MAP_MENU)
 -> Render_beginLoadMapData
 -> parse nodes / lines / sprites / events / strings / blockmap
 -> seventh DoomCanvas_updateLoadingBar callback
 -> __wrap_DoomCanvas_updateLoadingBar
 -> longjmp before legacy bitshapes/texels
 -> opaque native MENU_MAIN repaint
```

The 3D menu runtime had no navigation or presentation role left. The normal
path explicitly skipped all later historical menu-scene probes, yet still paid
for the structural runtime before immediately covering it with the opaque menu.

## V14 production composition

Normal `esp32-cyd` now performs:

```text
config + immutable mappings
 -> DoomRPG_esp32MainMenuModelBuildMain
 -> DoomRPG_esp32RepaintOpaqueMainMenu
 -> main-menu touch/action owners
```

No menu BSP map structures are created.

The explicit `esp32-cyd-bringup` profile retains the historical diagnostics
and their four compatibility linker flags:

```text
Render_beginLoadMap
DoomCanvas_updateLoadingBar
DoomRPG_probeNativeMenuSpriteFrame
DoomRPG_probeNativeMainMenuOverlay
```

Those flags are not part of the normal environment.

## CI and linker result

Against merged main `9aa3c3a62bb7639d884d036acc0baf54be88541e`:

```text
                         merged main     V14           delta
static RAM               45224 B         45128 B        -96 B
linked Flash            782009 B        773089 B      -8920 B
firmware.bin            782368 B        773456 B      -8912 B
active __wrap_*              55             52            -3
```

The three active wrappers removed by linker reachability are:

```text
__wrap_DoomCanvas_updateLoadingBar
__wrap_Render_beginLoadMap
__wrap_longjmp
```

The final ELF also contains none of:

```text
DoomRPG_probeMenuBspHeader
DoomRPG_probeMenuMapRuntimeStructures
DoomCanvas_updateLoadingBar
Render_beginLoadMap
Render_beginLoadMapData
__wrap_DoomCanvas_updateLoadingBar
__wrap_Render_beginLoadMap
__wrap_longjmp
```

The permanent menu owners remain present:

```text
DoomRPG_esp32MainMenuModelBuildMain
DoomRPG_esp32RepaintOpaqueMainMenu
```

## Real classic-CYD proof

### Cold boot

The normal startup reaches config/mappings without entering a BSP loader:

```text
[CONFIGMAP] READY config path exercised and mappings resident
[CONFIGMAP] mappingPayload=8376 heap8=35656 largest8=23540
[CONFIGMAP] Render_beginLoadMap / BSP still NOT executed
```

The native model/painter then becomes the first menu owner:

```text
[MAINOPAQUE] DASHBOARD modelFNV=292c7f95 layoutFNV=00077e67
logoFNV=0ac1f9c6 finalFNV=522dc605 composeMs=63
heap8=35656 largest8=23540
[MAINBOOT] READY owner=native-opaque menuBspRuntime=skipped
legacyMapStructures=not-created frame=522dc605
[ALIVE] ... heap=101580 heap8=35656 largest8=23540 ...
MAPPINGS=ready MENU=ready
```

The normal trace contains no `[MENUBSP]` or `[MAPSTRUCT]` stage.

### Native menu navigation

OPTIONS enters its fixed native model and returns to the exact main framebuffer:

```text
[MAINOPTIONS] ... heap8=35656 largest8=23540 shapeData=0x0 mediaTexels=0x0
[MAINBACK] READY source=options ... frame=522dc605 ...
[OPTIONBACK] FAST End ... runtimeFNV=522dc605 ... shapeData=0x0 mediaTexels=0x0
```

HELP parses its bounded PAK model and also returns to the exact main frame:

```text
[MAINHELP] READY menu=2 ... frame=5f22cf6b ...
[MAINBACK] READY source=help ... frame=522dc605 touch=rearmed ...
```

No scene rerender or menu-map reload occurs.

### START and intro

START begins with the menu heap still at the new direct-boot boundary:

```text
[MAINSTART] Begin ... heap8=35656 largest8=23540
shapeData=0x0 mediaTexels=0x0
```

The cleanup confirms there was no menu world to free:

```text
[MAINMENU] Runtime cleanup ... heap8=35656->76972 gained=41316
nodes=0x0 lines=0x0 mapSprites=0x0 mappings=0x0/0x0
shapeData=0x0 mediaTexels=0x0
```

The gain here is retained menu/legal/mapping ownership cleanup; the retired
menu BSP node/line/sprite runtime is already absent before START.

The unchanged bounded intro route remains valid. This hardware run used the
supported semantic skip of page-1 animation and still reached the same final
exit boundary:

```text
[INTROIN] CONTINUE storyPage=0->1 ...
[INTROIN] SKIP-ANIM storyPage=1->2 ...
[INTROIN] FINAL-CONTINUE ... fullTextPresented=yes
[INTROCLK] PARK reason=intro-exit-ready ...
[INTRODISP] READY ... heap8=43200->76972 recovered=33772 ...
assets=NULL texts=NULL clip=off noMapLoad=yes
```

### Entrance resident gameplay

Native startup takes over directly after disposal:

```text
[NATIVEBOOT] LOADING-TAKEOVER map=1 source=intro-disposed owner=transition-presentation
[NATIVEBOOT] RESIDENT map=1 file=/intro.bsp ... arena=14095 ...
[NATIVEBOOT] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

The generic session reaches visible gameplay:

```text
[ENGINESESSION] READY map=1 angle=64 residentCache=yes largeCache=yes
touch=invisible-120ms TURN+MOVE=armed shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap=93388 heap8=27464 largest8=18420 ...
[ALIVE] ... heap=93388 heap8=27464 largest8=18420 ...
```

A following FORWARD action commits a real move:

```text
[RESIDENTGAMEPLAY] MOVE n=1 seq=1 action=FORWARD
tile=904->872 delta=0,-64 pos=544,1760 ... committed=yes
[MONSTERTURN] COMPLETE reason=MOVE ... mutation=no
[ALIVE] ... heap=93388 heap8=27464 largest8=18420 ...
```

## Result

PASS on the real classic CYD.

The hardware-tested code boundary is
`dd4161a40b28d2ed9c88370b2f8281b9045f2980`. All commits after that
boundary for V14 closure are documentation-only.

Production menu ownership no longer requires `menu.bsp`, its legacy structural
loader, its loading-bar interception, or its `longjmp` escape. The old suite
survives only where it belongs: explicit bring-up diagnostics.
