# Milestone — ESP32 consolidation: retire legacy DoomRPG_Init link anchor

Date: 2026-09-30

## Boundary

```text
base main = 071febee7ec88958286fb74d82cee9ba61083a85
branch = agent/esp32-consolidation-legacy-init-anchor-v10
hardware-tested code head = f9ab3bda2cd9aadee2d1be0fc08d49600b7a9141
esp32-cyd CI #1159 = SUCCESS
static RAM = 45768 B
flash = 797697 B
firmware.bin = 798064 B
artifact id = 11101108652
artifact digest = sha256:cb597250f78ac4fdec896a4a27d8e97ac967cfb86a294567ab02402368cef8c8
firmware sha256 = 0da05325e6549e80fd887e6bab9e398a321022ff0718f47c0303f00671432d43
ELF sha256 = ea56e4757e9ff940a245fd6963d91a9193e44f4cad06463b347866287aefb8c8
```

Relative to the hardware-tested V9 artifact:

```text
RAM static: 45768 -> 45768 B
Flash:      801425 -> 797697 B (-3728 B)
firmware:   801792 -> 798064 B (-3728 B)
```

## Goal

Audit the remaining linked `MenuSystem_setMenu()` seam without replacing it
with a generic native router.

The source tree still contains many historical calls, but source presence is not
the same thing as linked runtime ownership. The milestone therefore classifies
each callsite by its enclosing function and checks the actual ESP32 ELF before
choosing a migration target.

## Audit result

Source-level call families include:

```text
MenuSystem_back / MenuSystem_select
MenuSystem_enterDigit cheat/debug
DoomCanvas_setupmenu
DoomCanvas_dyingState
credits / ingame menu paths
Game_changeMap map stats
Game_executeEvent EV_OPENSTORE
```

The V9 ELF already excludes every enclosing function in that list except
`DoomCanvas_setupmenu()`.

The one remaining linked path was:

```text
main.cpp
 -> DoomRPG_engineLinkAnchor()
 -> &DoomRPG_Init
 -> DoomCanvas_setupmenu()
 -> MenuSystem_setMenu()
```

The anchor's only runtime purpose was to print the address of
`DoomRPG_Init()` during hardware bring-up. It did not execute that initializer.
The active ESP32 engine already constructs the real object graph with
`DoomRPG_initEngineCore()`, then starts layout and retained/native owners
explicitly.

## Change

The code head removes:

- `DoomRPG_engineLinkAnchor()`;
- its declaration from `engine_metrics.h`;
- the startup address print that referenced it.

The diagnostic is replaced with an informational staged-runtime line. No menu
behavior, gameplay behavior, model builder or state router is added.

## ELF result

Exact symbol comparison between the V9 and V10 CI artifacts:

```text
V9:
DoomRPG_Init              597 B
DoomRPG_engineLinkAnchor    8 B
DoomCanvas_setupmenu      132 B
MenuSystem_setMenu        445 B

V10:
DoomRPG_Init              ABSENT
DoomRPG_engineLinkAnchor  ABSENT
DoomCanvas_setupmenu      ABSENT
MenuSystem_setMenu        ABSENT

already absent and still absent:
MenuSystem_back
MenuSystem_select
Menu_select

still present:
Menu_initMenu
MenuSystem_playSound
```

Seventeen global legacy functions disappear from the ELF and no new global
function appears. The rest of the discarded closure is:

```text
CombatEntity_setupEnemy
DoomCanvas_LoadMenuMap
DoomCanvas_unloadMedia
DoomCanvas_updateViewTrue
EntityDef_lookup
EntityMonster_reset
Entity_initspawn
Game_deactivate
Game_linkEntity
Game_loadMapEntities
MenuSystem_moveDir
Player_selectWeapon
Render_setGrayPalettes
```

This is dead-link closure retirement, not a functional rewrite.

## Real-CYD validation

The real classic CYD first exercised the pre-game menu domain.

HELP:

```text
[MAINHELP] PAGE-DOWN scroll=0->8
[MAINHELP] PAGE-UP scroll=8->0
[MAINBACK] READY source=help child=2->1 frame=522dc605 touch=rearmed ... noMenuSystemBack=yes noSetMenu=yes
```

OPTIONS:

```text
[MAINOPTIONS] READY explicit MENU_MAIN -> MENU_MAIN_OPTIONS composition
[MAINBACK] READY source=options child=7->1 frame=522dc605 touch=rearmed ... noMenuSystemBack=yes noSetMenu=yes
[OPTIONBACK] FAST End ... shapeData=0x0 mediaTexels=0x0
```

The menu heap remains stable at:

```text
heap8=20916
largest8=10740
```

START then validates the larger retained runtime path:

```text
[MAINSTART] READY explicit MENU_MAIN start composition -> Menu_startGame(new) -> Player_reset -> ST_INTRO
[INTRODISP] READY ... assets=NULL texts=NULL ... noMapLoad=yes
[NATIVEBOOT] READY map=1 ... genericResident=yes genericSpawn=yes ... shapeData=0x0 mediaTexels=0x0
[ENGINESESSION] READY map=1 ... shapeData=0x0 mediaTexels=0x0
```

Final steady hardware samples:

```text
[ALIVE] uptime=64666 ms heap=92492 heap8=26824 largest8=18420 ...
[ALIVE] uptime=69669 ms heap=92492 heap8=26824 largest8=18420 ...
```

The submitted log excerpt begins after cold boot and therefore does not contain
the new informational `[LEGACYINIT]` line. That marker is not used as the
hardware proof. The ELF establishes the static retirement; the menu + START run
establishes that the active runtime remains healthy without the removed
closure.

## Result

The generic legacy `MenuSystem_setMenu()` seam disappears from the linked
ESP32 firmware without being replaced by another generic router.

The important architectural lesson is that future consolidation audits must
separate:

```text
source callsite
linked reachability
real ESP32 runtime ownership
```

The desktop/J2ME sources remain executable behavioral reference material, but
dead call graphs must not be kept alive merely as historical link tests.

Any commit after
`f9ab3bda2cd9aadee2d1be0fc08d49600b7a9141` is documentation-only before
merge.
