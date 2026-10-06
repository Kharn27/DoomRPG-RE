# ESP32 legacy Hud object retirement milestone

Date: 2026-10-06

Branch:
`agent/esp32-retire-legacy-combat`

Base main:
`72ec1b2307685dc83b344db8584b0646cde27e47`

Hardware-tested code boundary:
`8d0cd7443d2524829128f6ee26b1c19244bd6dbd`

## Goal

Retire the remaining inherited `Hud_t` compatibility object from the normal
classic-CYD runtime after the desktop Hud translation unit and resident HUD
bitmaps had already been removed.

Visible HUD composition remains owned by `EspNativeGameplayHud`. This step
removes the final object allocation and stale direct `Hud_t` field
dependencies that survived only through desktop-shaped helpers.

## Final ownership change

At the hardware-tested boundary:

- `DoomRPG_initEngineCore()` no longer allocates `Hud_t`;
- `doomRpg->hud` remains `NULL` by construction;
- the generated ESP32 `DoomCanvas.c`, `Game.c` and `Player.c` retire the
  remaining direct legacy Hud state/reset accesses;
- native gameplay HUD, top-bar feedback, dialogs, action feedback and
  view-flash paths own visible UI state;
- `src/Hud.c` remains excluded from the normal ESP32 build;
- no compatibility `Hud_t` allocation is reintroduced.

A post-retirement source cleanup removed one stale Hud dependency from the
native main-menu model. That correction is included in the tested boundary
above.

## Real-CYD acceptance

The real classic CYD boots the tested head with:

```text
[CORE] Sound retired object=NULL owner=native-audio-intent playback=deferred
[CORE] Hud retired object=NULL owner=native-gameplay-hud+status-feedback
[CORE] EntityDef retired object=NULL owner=native-entitydef-catalog
[CORE] Combat retired object=NULL owners=native-combat+weapon+monster-turn
[CORE] ParticleSystem retired object=NULL owner=native-gibfx
[CORE] READY objects=6 ...
```

MENU_MAIN -> START -> full intro -> bounded disposal -> MAP_INTRO resident
bootstrap succeeds. The exact first frame remains:

```text
[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 ...
```

and the resident session preserves:

```text
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

The same hardware session then exercises the UI and gameplay paths that had
historically touched Hud state:

- multi-page event dialogs with opcode-19 resume and opcode-26 no-back dialog;
- automap open/close followed by native HUD repaint;
- regular and non-regular door handling;
- extinguisher fire-clear action and semantic player-attack turn publication;
- note append and dialog resume;
- resource pickup and standalone weapon-help dialog;
- secret discovery feedback;
- monster activation and ordered two-member monster turns;
- committed monster retaliation with red view flash and damage message;
- committed monster movement/topology relink;
- player monster attack, kill, gib effect, XP and dynamic drop;
- dynamic Armor Shard pickup from the killed monster;
- additional weapon pickup and weapon-help dialog.

Representative committed retaliation:

```text
[MONSTERRETAL] COMMIT ... playerHP=30->27 armor=8->5 ...
                         message="6 damage!" redFlash=b800/500ms ...
[MONSTERRETAL] COMMIT ... playerHP=27->24 armor=5->2 ...
                         message="6 damage!" redFlash=b800/500ms ...
```

Representative player kill:

```text
[MONSTERCOMBAT] COMMIT seq=102 sprite=220 subtype=0 hp=5->0 armor=3->1
alive=1->0 ... xp=5-applied ... rollback=closed
[MONSTERDROP] COMMIT ... type=3 subtype=21 def=92 tile=658 ...
[PLAYERRES] COMMIT tile=658 ... armor=6/20 ... message=pickup-live ...
```

The ordered monster sequence also remains live:

```text
[MONSTERACTIVESEQ] COMPLETE turn=35 reason=1 activeCount=2 delivered=2 ...
[MONSTERACTIVESEQ] COMPLETE turn=36 reason=3 activeCount=2 delivered=1 ...
```

Stable ALIVE samples after lazy dialog/note/combat owners are resident:

```text
heap=123612 heap8=57688 largest8=51188
```

No crash, legacy Hud allocation, or UI fallback is observed.

## Local build witness

The user's normal `esp32-cyd` PlatformIO build at the tested boundary reports:

```text
RAM:   13.9% (45464 / 327680 B)
Flash: 59.6% (781017 / 1310720 B)
```

The branch has no completed GitHub Actions status attached to
`8d0cd7443d2524829128f6ee26b1c19244bd6dbd`; therefore no CI result is
claimed for this exact head.

## Architecture invariants

```text
doomRpg->hud == NULL
doomRpg->sound == NULL
doomRpg->combat == NULL
doomRpg->entityDef == NULL
shapeData == NULL
mediaTexels == NULL
visible HUD = EspNativeGameplayHud
runtime assets = DoomRPG-ESP32.pak
no runtime ZIP fallback
```

## Closure

Hardware-tested code boundary:
`8d0cd7443d2524829128f6ee26b1c19244bd6dbd`.

Post-test changes are documentation-only.
