# ESP32 desktop Hud translation-unit retirement milestone

Date: 2026-10-04

Branch:
`agent/esp32-retire-legacy-combat`

Hardware-tested code boundary:
`9efa082d1aacbc137e1e7fcd726f857c0dbae2d6`

## Goal

Prove that the desktop `src/Hud.c` implementation is no longer a production
dependency on classic CYD before attempting to retire the remaining compact
`Hud_t` compatibility object.

## Change

`src/Hud.c` is excluded from the normal ESP32 build.

A small ESP32-only compatibility translation unit keeps:

- `Hud_init/free` for the temporary 600-byte scalar/message container;
- message queue helpers used by inherited signatures;
- geometry initialization without any bitmap ownership;
- `Hud_drawBarTiles`, `Hud_drawTopBar`, `Hud_drawBottomBar` and
  `Hud_drawEffects` as deliberate no-ops.

The production visible HUD remains `EspNativeGameplayHud`, which renders from
bounded native PAK reads.

## CI

Normal `esp32-cyd` CI #1584: SUCCESS.

```text
RAM:   13.9% (45472 / 327680 B)
Flash: 59.6% (781385 / 1310720 B)
artifact id: 11303743052
artifact sha256: d90cb7dc7e4f18db92c0d3317be493ab2ea85ba6af6920d61da913693148886a
```

Compared with the preceding HUD-bitmap boundary, linked Flash falls by 1384 B.
No local PlatformIO build is claimed.

## Real-CYD acceptance

Cold boot proves the compatibility object is allocation-only and image-free:

```text
[HUDCOMPAT] INIT bytes=600 images=NULL renderer=native-pak-stream
[CORE] Hud used=616 ...
...
[LAYOUT] HUD legacy bitmaps status=0x0 large=0x0 faces=0x0 icons=0x0 attack=0x0 arrow=0x0 owner=native-pak-stream
```

The user then exercises the production UI and gameplay broadly:

- native MENU_MAIN and START;
- complete intro and map transition;
- native hub inventory, weapons and status pages;
- PASS_TURN feedback;
- automap open/close with exact HUD repaint;
- event dialog and standalone weapon-help dialog;
- pickups and world movement;
- monster activation and ordered monster turns;
- monster hit and miss retaliation with damage/dodge messages;
- player monster kill, gib effect and dynamic drop.

Representative HUD ownership witness:

```text
[GAMEPLAYHUD] REPAINT health=30/30 armor=8/20 weapon=1 ammo=7 angle=64
              pixels=7484 reads=70 bytes=6344 ownerMutation=no dirtyConsume=no
```

Representative monster feedback remains live:

```text
[MONSTERRETAL] COMMIT ... message="5 damage!" ... redFlash=b800/500ms ...
[MONSTERRETAL] MISS-COMMIT ... message="Dodged!" textQueued=yes rendered=yes ...
```

Stable gameplay remains at:

```text
heap=127880 heap8=61956 largest8=51188
```

Therefore the desktop HUD translation unit is not required by the production
classic-CYD runtime.

## Boundary

This milestone deliberately does not yet claim `Hud_t` retirement.
The 600-byte object still provides inherited scalar/message fields referenced by
some retained desktop-shaped helpers. The next bounded step is to inventory and
migrate those final field accesses before setting `doomRpg->hud == NULL`.

## Invariants

```text
src/Hud.c excluded from esp32-cyd
legacy HUD rasterization = no-op compatibility only
visible HUD = EspNativeGameplayHud
legacy HUD images = NULL
doomRpg->sound == NULL
doomRpg->combat == NULL
doomRpg->entityDef == NULL
shapeData == NULL
mediaTexels == NULL
runtime assets = DoomRPG-ESP32.pak
```

## Closure

Hardware-tested code boundary:
`9efa082d1aacbc137e1e7fcd726f857c0dbae2d6`.

Post-test closure is documentation-only.
