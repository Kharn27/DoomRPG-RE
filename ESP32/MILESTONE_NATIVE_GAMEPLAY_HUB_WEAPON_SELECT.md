# Native gameplay HUB weapon selection / WPN grid — real-CYD hardware PASS

> Historical hardware boundary: this document records the original three-page
> 4x3 WPN implementation. The current four-page HUB and complete 3x3 arsenal are
> documented in
> [`MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md).

This milestone completes the native HUB weapon-selection UX on the classic CYD. It keeps the narrow canonical mutation (`player.weapon` only) but moves weapons out of the scrolling Inventory list into a dedicated `WPN` page with direct touch selection and original Doom RPG sprite assets.

## Git boundary

```text
main at branch creation = d6909793860312397edc7aa1b36f995a5ccbb914
branch = agent/esp32-native-gameplay-hub-weapon-select
base main = d6909793860312397edc7aa1b36f995a5ccbb914
final hardware-tested code boundary = 308295c4f56b1741040d1bc5e07ce7f66d245a70
status = REAL-CYD HUB WPN GRID + DIRECT OWNED-WEAPON SELECT PASS
branch policy = LOCKED; docs-only tail only
```

Normal GitHub Actions `esp32-cyd` run #234 / run ID `34947947008` completed successfully on the exact final tested SHA and produced artifact:

```text
doom-rpg-esp32-cyd-308295c4f56b1741040d1bc5e07ce7f66d245a70
```

CI is compile/link evidence only; the real-CYD serial logs and visual test remain authoritative.

## Permanent bounded mutation contract

`EspNativeGameplayPlayerState_selectOwnedWeapon()` remains distinct from `adoptWeapon()`:

```text
valid canonical weapon id = 0..11
ownership bit is required
only player.weapon may change
weapons/ammo/inventory/stats/keys/credits remain exact
reselecting the active weapon returns changed=0
no allocation
no turn
```

The HUB owner remains 28 B and PlayerState remains 52 B. No world dispatch occurs while the HUB is active.

## Final HUB layout

The HUB now has three visible pages:

```text
INV | WPN | STAT
```

`INV` contains only non-weapon content (Notebook, carried items, Credits, keys). `WPN` is a 4x3 touch grid. The first nine cells are the conventional arsenal IDs 0..8:

```text
0 Axe
1 Fire Ext
2 Pistol
3 Shotgun
4 Chaingun
5 Super Shotgn
6 Plasma Gun
7 Rocket Lnchr
8 BFG
```

Canonical IDs 9..11 (`Hellhound`, `Cerberus`, `Demon Wolf`) are captured-familiar weapon slots, not normal arsenal presentation. Their three grid cells are intentionally blank and non-selectable.

Presentation rules:

```text
owned weapon      = original sprite in color
unowned weapon    = same sprite in grayscale
equipped weapon   = yellow border (RGB565 ffe0)
blank slots 9..11 = empty and non-selectable
```

Touching an owned weapon cell directly selects/equips it in one gesture. Touching an unowned weapon remains fail-closed through the owned-weapon selection primitive.

## Native icon source / storage contract

The WPN renderer does not allocate map-wide texels and does not use ZIP assets. It reads bounded ranges from `/DoomRPG-ESP32.pak` through the native mappings chain:

```text
EntityDef.tileIndex
 -> mappings.bin mediaSpriteIds[tileIndex]
 -> mediaBitShapeOffsets[mediaSprite]
 -> bitshapes.bin / palettes.bin / stexels.bin
```

The bounded icon decode workspace is an explicit static owner so the Arduino `loopTask` stack is not consumed by the ~2.7 KiB scratch area:

```text
scratchBytes = 2690
scratchOwner = static
persistentIconBytes = 0
```

This fixed a real-CYD stack-canary reset observed when the scratch buffers were local stack objects.

## Pistol presentation

The weapon EntityDef for Pistol does not provide a useful distinct pickup sprite. The WPN page therefore uses the real Bullets pickup as the Pistol icon, resolved from the native EntityDef catalog as ammo type 6 / subtype 1.

Final hardware source witness:

```text
[HUBWGRID] SOURCES catalog=yes bullets=tile/83 familiarIds=9..11 display=blank
[HUBWGRID] ICON weapon=2 weaponTile=9 iconTile=83 media=318
           name="Pistol" icon=ready source=bullets
           sourceSize=16x11 drawSize=16x11 upscale=no
           owned=yes equipped=yes render=color
```

Small sprites are never enlarged. The 16x11 bullets icon is drawn at native 16x11 size; larger weapon sprites are only downscaled to fit their cell.

## Final real-CYD grid witness

On a fresh Entrance start, only Pistol was owned/equipped:

```text
weapon=2
weapons=004
ammo=00/08/00/00/00/00
playerFNV=e745fce9
```

The final candidate emitted:

```text
[HUBWGRID] FRAME weapons=9/9 slots=12 blankSlots=3 icons=9 missing=0
           owned=1 equipped=2 selected=2
           ownedRender=color unavailableRender=gray
           familiarIds=9..11/excluded
           pistolIcon=bullets-pickup/native-no-upscale
           persistentIconBytes=0 scratchBytes=2690 scratchOwner=static
           sourceMaskBytes=1447 sourceTexelBytes=2669
           assetFNV=342db611
           packOwnership=preserved-open mutation=no turn=no
```

The user visually confirmed on the real CYD that:

```text
the three surplus/familiar cells are blank
only the Pistol cell is lit at fresh start
the Pistol uses the correct bullets pickup sprite
the final WPN presentation is acceptable
```

The resulting WPN frame fingerprint at this fresh witness was:

```text
frame=bcc52568
```

## Final exact close / idempotent witness

The final SHA also hardware-tested selecting the already-equipped Pistol:

```text
[HUBWEAPON] GRID-SELECT target=2 weapon=2->2
            status=UNCHANGED owned=yes
            playerFNV=e745fce9->e745fce9
            exactOnlyWeapon=yes mutation=no turn=no
```

Closing the HUB restored the protected HUD exactly and resumed the world without a turn:

```text
[HUB] CLOSE ...
      playerFNV=e745fce9->e745fce9 expected=e745fce9 exact=yes
      weapon=2->2 sessionMutation=no turn=no
      menuUnderlayRestore=exact
      hudBands=6c2aa46f expectedHud=6c2aa46f exactHud=yes
      packClosed=yes
[WEAPON] DRAW weapon=2 ... cache=hit
[RESIDENTGAMEPLAY] FRAME reason=HUB-CLOSE angle=64 frame=22397b55 presented=1
```

## Changed-selection witness retained from this branch

Before the final visual-only WPN refinements, the same branch hardware-tested live direct weapon mutation in both directions with Fire Ext + Pistol owned. Representative transaction:

```text
[HUBWEAPON] GRID-SELECT target=2 weapon=1->2
            status=CHANGED owned=yes
            playerFNV=a6e115a7->16e881bc
            exactOnlyWeapon=yes
            worldRedraw=on-close mutation=weapon-only turn=no
```

The reverse `Pistol -> Fire Ext` path was also observed, followed by exact HUB repaint and gameplay redraw. Subsequent code changes leading to the final boundary were limited to WPN icon sourcing/presentation and exclusion of familiar slots; they did not broaden the PlayerState mutation contract.

## RAM witness

Final fresh-session ALIVE values were stable across WPN open/select/close:

```text
heap=85228
heap8=19496
largest8=14324
hub owner=28 B
player owner=52 B
MENU underlay=1280 B
WPN icon scratch=2690 B static owner
persistent icon cache=0 B
```

Do not infer a leak by comparing this value with earlier branch revisions: the WPN renderer deliberately moved 2690 B of scratch ownership off the `loopTask` stack into static storage after the hardware stack-canary failure. `largest8` remained 14324.

## Deferred families remain deferred

This milestone does not enable Notebook activation, consumable use/confirmation, familiar combat semantics, Automap, Save/Load/Options/store, or any new world/event family. Item use remains separate because it may mutate health, berserker/familiar state, messages, presentation and turn ownership.

After this hardware PASS, code is locked. Any commits after `308295c4f56b1741040d1bc5e07ce7f66d245a70` must be documentation-only until merge.
