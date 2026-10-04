# ESP32 legacy legal-strip residency retirement milestone

Date: 2026-10-04

Branch:
`agent/esp32-retire-legacy-combat`

Hardware-tested code boundary:
`35c310026484f095448ef926b2bba944c8dbb359`

## Goal

Stop allocating the inherited `g.bmp` legal-screen strip on the normal
classic-CYD boot path.

The native opaque main menu is already the production owner. It does not enter
`ST_LEGALS`, so the 128x512 packed image had become dead menu residency.

## Change

The ESP32-generated `DoomCanvas_startup()` no longer calls:

```c
DoomRPG_createImage(doomCanvas->doomRpg, "g.bmp", false,
                    &doomCanvas->imgLegals);
```

The desktop source and `DoomCanvas_legalsState()` remain unchanged as the
behavior/reference implementation. The ESP32 normal runtime simply does not
instantiate that obsolete resource.

## CI

Normal `esp32-cyd` CI #1580: SUCCESS.

```text
RAM:   13.9% (45472 / 327680 B)
Flash: 59.7% (782817 / 1310720 B)
artifact id: 11302983372
artifact sha256: 5b53223c103def2db06d252853751e3d512f7e41c3fb00359eb245fee515d25e
```

No local PlatformIO build is claimed.

## Real-CYD acceptance

Before this cut, after Sound retirement, MENU_MAIN reported:

```text
heap8=65888 largest8=32756
```

After the cut:

```text
[LAYOUT] heap8 used=21828 remaining=117932 largest=73716
...
[MAINOPAQUE] ... heap8=98772 largest8=73716
[ALIVE] ... heap=164696 heap8=98772 largest8=73716 ...
```

That is +32884 B of free 8-bit heap at the menu boundary, consistent with
eliminating the 32768-byte packed legal image and allocator overhead/layout.

START also confirms there is nothing left to release:

```text
[MAINMENU] Runtime cleanup legals=already-free
           heap8=98772->107212 gained=8440
```

The previous START cleanup gained 41324 B because it was freeing the dead
legal strip together with mappings. The new 8440 B corresponds to the
remaining mapping/runtime cleanup rather than a deferred legal-image free.

The same hardware run successfully completes the full native intro, intro
disposal, MAP_INTRO resident load, native HUD/caches and resident gameplay.
Pickups and a door transition are also exercised.

## Invariants

```text
imgLegals bitmap = NULL on normal ESP32 boot
native opaque MENU_MAIN = production menu owner
doomRpg->sound == NULL
doomRpg->combat == NULL
doomRpg->entityDef == NULL
shapeData == NULL
mediaTexels == NULL
runtime assets = DoomRPG-ESP32.pak
```

## Closure

Hardware-tested code boundary:
`35c310026484f095448ef926b2bba944c8dbb359`.

Post-test closure is documentation-only.
