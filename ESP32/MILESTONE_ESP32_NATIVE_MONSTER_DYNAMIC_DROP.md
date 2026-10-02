# ESP32 native monster dynamic drop milestone

Date: 2026-10-02

Branch: `agent/esp32-levelup-drop-consequences`

Hardware-tested code boundary:
`61b156415829a4f4bc70f76d1ba751effedc29eb`

## Scope

This milestone restores the live monster-drop consequence after lethal native
combat without reintroducing mutable legacy entities or BSP sprite mutation.

Permanent ownership:

- `EspNativeGameplayMonsterDrop`: 8 rotating slots, 144 B total owner.
- Drop identity is resolved from the existing compact entity-definition catalog.
- Drop position is copied from the native monster-position owner.
- The renderer projects active untaken slots directly from the bounded owner.
- `EspNativeGameplayPlayerResources` merges dynamic drops with normal static
  pickups and applies both through shared `EspNativeGameplayPlayerState`.
- Consuming a dynamic drop marks only its owner slot taken.
- SAVE/LOAD persistence for those slots is intentionally deferred.

Legacy behavior preserved:

- the already-consumed lethal-combat drop RNG word is reused;
- subtype 1, 12 and 13 never drop;
- low byte below 51 is a legitimate no-drop;
- subtype-specific ammo/health/armor/credit selection follows
  `Entity_spawnDropItem()`;
- the historical rotating pool remains bounded to 8 live slots.

## Hardware-discovered renderer boundary

The first real-CYD drop was visible, but entering its tile failed the MOVE frame:

```text
[TURNFRAME] DIAG fail=SPRITES ...
[RESIDENTGAMEPLAY] RENDER-FAILED reason=MOVE
[MOVEEVENT] ROLLBACK ...
```

The dynamic billboard was being projected while located at the committed camera
tile. Commit `61b156415829a4f4bc70f76d1ba751effedc29eb` makes that one
intermediate frame cull explicit. The resource transaction still owns actual
removal immediately after MOVE commit.

## Real-CYD PASS

Zombie sprite 157 / subtype 0 produced a live armor drop:

```text
[MONSTERDROP] COMMIT roll=d8fb13bb slot=0 overwriteVisible=0 type=3 subtype=21 def=92 tile=268 pos=800,544 visible=1 next=1 ownerBytes=144 persistence=map-session-live/save-deferred
[MONSTERCOMBAT] COMMIT seq=207 ... dropRoll=value/d8fb13bb dropMaterialize=live ...
```

The player then entered exactly that tile:

```text
[MONSTERDROP] RENDER-CULL slot=0 tile=268 reason=player-tile pickup=pending-after-commit
[RESIDENTGAMEPLAY] MOVE n=88 seq=208 action=FORWARD tile=267->268 ... committed=yes
```

Resource service found the dynamic Armor Shard and an existing static Health
Vial on the same tile:

```text
[PLAYERRES] PREPARE tile=268 sprite=65535 defTile=92 type=3 subtype=21 parm=4 action=armor value=1->5 slot=none/0 worldRemove=dynamic-drop-slot rollback=armed
[PLAYERRES] PREPARE tile=268 sprite=148 defTile=91 type=3 subtype=20 parm=4 action=health value=26->30 slot=none/0 worldRemove=hidden-overlay rollback=armed
[PLAYERRES] COMMIT tile=268 candidates=2 consumed=2 totalConsumed=14 ... hp=30/30 armor=5/20 ...
[PLAYERRES] FEEDBACK tile=268 message="Got Armor Shard" sourceDefTile=92 ... additionalMessages=1-deferred
```

This is the final hardware proof for the live drop path:

`lethal combat -> legacy RNG resolution -> bounded materialization -> visible world drop -> safe player-tile render boundary -> dynamic pickup -> shared PlayerState mutation`.

Earlier kills on this branch also returned
`dropMaterialize=none` for legitimate no-drop outcomes, proving the path does
not fabricate an item when the legacy rule says none.

## Build

GitHub Actions `ESP32 CYD Build` run #1524: SUCCESS.

```text
RAM:   13.8% (used 45352 bytes from 327680 bytes)
Flash: 59.2% (used 776233 bytes from 1310720 bytes)
```

No local PlatformIO build is claimed.

## Explicitly not validated here

- checkpoint persistence of live dynamic drops;
- level-up popup/modal behavior from the sibling change on this branch;
- deferred pickup secondary-message presentation.

Those remain separate future boundaries.
