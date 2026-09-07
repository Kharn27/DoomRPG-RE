# Native gameplay HUB weapon selection — real-CYD hardware PASS

This milestone adds one bounded live mutation to the native gameplay HUB: selecting an already-owned weapon from the Inventory list updates only the canonical 52 B `EspNativeGameplayPlayerState.weapon` field. Notebook, items, credits, keys and Status remain fail-closed. No turn is consumed and no world dispatch occurs while the HUB is active.

## Git boundary

```text
main at branch creation = d6909793860312397edc7aa1b36f995a5ccbb914
branch = agent/esp32-native-gameplay-hub-weapon-select
base main = d6909793860312397edc7aa1b36f995a5ccbb914
hardware-tested code boundary = 1a4ab97ef5cfd509663be28db92d22610bd77cf6
status = REAL-CYD HUB OWNED-WEAPON SELECT PASS
branch policy = LOCKED; docs-only tail only
```

Normal GitHub Actions `esp32-cyd` run `34108679822` / run #215 completed successfully on the exact tested SHA and produced artifact `doom-rpg-esp32-cyd-1a4ab97ef5cfd509663be28db92d22610bd77cf6`.

## Permanent bounded API

`EspNativeGameplayPlayerState_selectOwnedWeapon()` is distinct from `EspNativeGameplayPlayerState_adoptWeapon()`.

Selection contract:

```text
valid id = 0..11
weapon bit must already be owned in player.weapons
only player.weapon may change
weapons/ammo/inventory/stats/keys/credits stay exact
reselecting current weapon returns success with changed=0
no allocation
no turn
```

The HUB keeps the permanent owner at 28 B by using the former reserved byte as `weaponAtOpen`; there is no new persistent allocation. The 32x20 / 1280 B MENU underlay remains unchanged.

## Real-CYD changed-selection witness

The tested player state had multiple owned weapons:

```text
weapon=1
weapons=006
ammo=10/12/00/00/00/00
items=01/00/00/00/00
keys=00000000
credits=0
playerFNV=a6e115a7
```

The live Inventory projected five entries, including Fire Ext and Pistol:

```text
selected 0: current=Fire Ext value=10 next=Pistol value=12
selected 1: prev=Fire Ext current=Pistol next=Notebook
```

The user centered Pistol then invoked SELECT. Hardware emitted:

```text
[HUBWEAPON] SELECT entry=1 name="Pistol"
            weapon=1->2
            status=CHANGED
            owned=yes
            playerFNV=a6e115a7->16e881bc
            exactOnlyWeapon=yes
            worldRedraw=on-close
            mutation=weapon-only
            turn=no
            packClosed=yes
```

Resident routing stayed modal:

```text
[RESIDENTGAMEPLAY] HUB-INPUT ... action=SELECT status=OK
worldDispatch=blocked
turnAdvance=no
```

## Close / world presentation witness

The close path accepted the new canonical player fingerprint rather than requiring the open-time fingerprint:

```text
[HUB] CLOSE ...
      playerFNV=a6e115a7->16e881bc
      expected=16e881bc exact=yes
      weapon=1->2
      sessionMutation=weapon-only
      turn=no
      menuUnderlayRestore=exact
      hudBands=c340d7ba expectedHud=c340d7ba exactHud=yes
      packClosed=yes
```

The normal compositor then rendered the newly selected weapon:

```text
[WEAPON] DRAW weapon=2 logical=242 actual=610 frame=0 pose=idle
[RESIDENTGAMEPLAY] FRAME reason=HUB-CLOSE ... frame=d222e76b ... presented=1
```

This proves the selection survives HUB close and becomes the live gameplay weapon without consuming a turn.

## Unchanged / fail-closed behavior

A SELECT on the already-active Fire Ext was also observed earlier in the same real-CYD session:

```text
[HUBWEAPON] SELECT entry=0 name="Fire Ext" weapon=1->1
            status=UNCHANGED
            playerFNV=a6e115a7->a6e115a7
            exactOnlyWeapon=yes
            mutation=no turn=no
```

Notebook/items/credits/keys and Status remain unsupported SELECT targets and must continue to fail closed. Weapon selection never grants ownership and never consumes ammo.

## RAM witness

The supplied real-CYD session remained flat before and after the changed selection and close:

```text
heap=87356
heap8=21624
largest8=14324
hub owner=28 B
player owner=52 B
MENU underlay=1280 B
```

Compared with the preceding Inventory-list milestone (`87988 / 22256 / 14324`), free heap and 8-bit heap are each 632 B lower while `largest8` is unchanged. This milestone does not add a persistent HUB owner, so treat the new values as the exact witness for this tested firmware/session rather than claiming a leak. Re-check before future RAM-heavy work.

## UX finding from hardware use

The semantics pass, but the current three-card touch model is not ideal for direct touch:

```text
top card    -> move previous
center card -> SELECT current
bottom card -> move next
```

Touching a visible weapon on the top or bottom card only centers it; a second tap on the center card is required to equip it. The user explicitly found this unintuitive.

Do not change this hardware-tested branch after lock. The next bounded milestone should improve only the HUB touch interaction so a direct tap on a visible weapon can select/equip it in one intentional gesture while keeping non-weapon entries fail-closed, preserving no-turn semantics, bounded touch feedback, the 28 B HUB owner and the canonical PlayerState.

## Deferred families remain deferred

This milestone does not enable Notebook activation, consumable use/confirmation, keys/credits actions, Automap, Save/Load/Options/store, or any new world/event family. Item use remains separate because it can mutate health, berserker/familiar state, messages, presentation and turn ownership.
