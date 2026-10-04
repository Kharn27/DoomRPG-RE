# ESP32 legacy HUD bitmap residency retirement milestone

Date: 2026-10-04

Branch:
`agent/esp32-retire-legacy-combat`

Hardware-tested code boundary:
`96813c8333ec07dcfc7343517e6c69764b70f082`

## Goal

Retire the six desktop HUD bitmap allocations from normal ESP32 startup without
yet deleting `Hud_t` itself.

The production HUD renderer is already native and PAK-backed. Keeping a second
set of resident desktop HUD images only duplicated presentation resources.

## Change

The generated ESP32 `DoomCanvas_startup()` no longer invokes
`Hud_startup()`. Instead it installs only the small scalar geometry values
still required by transitional compatibility code:

```text
top bar height = 20
bottom bar height = 20
face cell      = 18x20
icon cell      = 13x13
legacy HUD placement scalars preserved
```

The six inherited `Image_t` owners stay NULL:

```text
imgStatusBar
imgStatusBarLarge
imgHudFaces
imgIconSheet
imgAttArrow
imgStatusArrow
```

The layout probe explicitly checks those pointers remain NULL.

`EspNativeGameplayHud` remains the production owner and reads its sprites
directly from `DoomRPG-ESP32.pak`.

## CI

Normal `esp32-cyd` CI #1582: SUCCESS.

```text
RAM:   13.9% (45472 / 327680 B)
Flash: 59.7% (782769 / 1310720 B)
artifact id: 11304071480
artifact sha256: ca85db10f8a0595a743f68c27ea02f5ccc8b30bd165ea03ffbd58091a90b243e
```

No local PlatformIO build is claimed.

## Real-CYD acceptance

The final build reaches resident MAP_INTRO gameplay and exercises:

- PASS_TURN with live top-bar feedback;
- normal movement and turns;
- automap visibility publication;
- two Armor Shard pickups with live feedback;
- collision against a closed regular door;
- regular door open, traversal and deferred close;
- repeated stable resident heartbeats.

Representative witness:

```text
[PLAYERRES] PREPARE ... defTile=92 type=3 subtype=21 parm=4 action=armor ...
[PLAYERRES] FEEDBACK ... message="Got Armor Shard" ...
...
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
...
[ALIVE] ... heap=127880 heap8=61956 largest8=51188 ...
```

The previous legal-strip hardware run reported:

```text
heap=123632 heap8=57708 largest8=51188
```

The +4248 B free-heap difference is consistent with retiring the six HUD
bitmap allocations and associated allocator/image metadata. It is supporting
hardware evidence, not exact allocator attribution.

## Invariants

```text
legacy Hud_t Image_t bitmap owners == NULL
native gameplay HUD = PAK-backed bounded rendering
doomRpg->sound == NULL
doomRpg->combat == NULL
doomRpg->entityDef == NULL
shapeData == NULL
mediaTexels == NULL
runtime assets = DoomRPG-ESP32.pak
```

## Closure

Hardware-tested code boundary:
`96813c8333ec07dcfc7343517e6c69764b70f082`.

Post-test closure is documentation-only.
