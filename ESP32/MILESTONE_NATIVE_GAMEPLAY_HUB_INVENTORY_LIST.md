# Milestone — Native gameplay HUB Inventory list

## Result

**REAL CLASSIC CYD HARDWARE PASS**

This milestone keeps the gameplay HUB strictly read-only while replacing the
three summary cards with a bounded native Inventory entry projection in recovered
legacy content order.

```text
base main = 9b518a50ec3a977394634121280820853c01089b
branch = agent/esp32-native-gameplay-hub-inventory-list
hardware-tested code boundary = 20a1c4fe6a08308aee73c1e977316abf358abdb5
normal esp32-cyd CI = run 34100083958 / #202 = SUCCESS
hardware = ESP32-2432S028R classic CYD, no PSRAM
```

Serial output from the real board is the runtime authority. Commits after the
hardware-tested code SHA are documentation-only.

## Scope

The milestone changes Inventory presentation/navigation only:

- derive a selectable entry count directly from the canonical 52 B PlayerState;
- project one requested entry on demand instead of retaining a list owner;
- preserve recovered legacy content order;
- render a circular previous/current/next window in the existing three-card area;
- use the current HUB selection byte as the entry index;
- keep SELECT fail-closed;
- keep Notebook activation, weapon selection and consumable use disabled;
- keep world dispatch blocked and turn advancement disabled while HUB is active;
- retain the existing Status page, MENU hand underlay and close path.

The permanent HUB owner remains 28 B. There is no persistent Inventory-list or
string allocation.

## Recovered legacy content order

The desktop reference builds `MENU_ITEMS` as:

```text
Back
WEAPONS divider
owned weapons 0..11 in ascending id order
ITEMS divider
Notebook
carried consumables 25..29 in ascending subtype order
OTHER divider
Credits
Green Key
Yellow Key
Blue Key
Red Key
```

The native HUB already owns its own close control and page/card chrome, so
`Back` and the dividers are not duplicated. The permanent projection order is:

```text
owned weapons 0..11
Notebook
carried items 25..29
Credits
Green / Yellow / Blue / Red keys
```

Weapon values follow the original menu contract: Axe shows `--`; weapon ids
1..11 display `ammo[weaponInfo[id].ammoType]`. Item values are carried counts,
Credits shows the credit count, and Notebook/keys show `--`.

## Bounded projection API

The content layer adds allocation-free read-only queries:

```text
EspNativeGameplayHubContent_inventoryEntryCount(player)
EspNativeGameplayHubContent_inventoryEntryAt(player, index, outEntry)
```

One transient entry is:

```text
name[17]
value[12]
kind
sourceId
sizeof(entry) = 31 B on hardware build
```

The maximum possible list is bounded at 23 entries:

```text
12 weapons + Notebook + 5 items + Credits + 4 keys = 23
```

No 23-entry array is stored. The renderer materializes only previous/current/next
entries, for a 93 B transient visible-entry payload.

## Maximum-list hardware probe

The first Inventory paint used a local synthetic PlayerState to exercise the
maximum projection without mutating the real gameplay owner. Names still came
from the hardware-proven native EntityDef catalog and PAK.

The real CYD emitted all 23 entries in exact order:

```text
0  weapon   Axe          --
1  weapon   Fire Ext     10
2  weapon   Pistol       20
3  weapon   Shotgun      30
4  weapon   Chaingun     20
5  weapon   Super Shotgn 30
6  weapon   Plasma Gun   50
7  weapon   Rocket Lnchr 40
8  weapon   BFG          50
9  weapon   Hellhound    60
10 weapon   Cerberus     60
11 weapon   Demon Wolf   60
12 notebook Notebook     --
13 item     Sm Medkit    1
14 item     Lg Medkit    2
15 item     Soul Sphere  3
16 item     Berserker    4
17 item     Dog Collar   5
18 credits  Credits      123
19 key      Green Key    --
20 key      Yellow Key   --
21 key      Blue Key     --
22 key      Red Key      --
```

The strict witness closed with:

```text
[HUBLIST] READY entries=23/23 order=legacy-content
          persistentListBytes=0 transientEntryBytes=31
          listFNV=93b6a47c
          packOwnership=preserved-open mutation=no turn=no
```

`listFNV=93b6a47c` is the canonical maximum-list projection fingerprint for this
source corpus and ordering rule.

## Fresh Entrance live list

The canonical fresh Entrance PlayerState remained:

```text
weapon=2
weapons=004
ammo=00/08/00/00/00/00
items=00/00/00/00/00
keys=00000000
credits=0
playerFNV=e745fce9
```

The live list therefore contained exactly three entries:

```text
0 weapon   Pistol   8
1 notebook Notebook --
2 credits  Credits  0
```

The real board painted these three circular windows:

```text
selected=0 prev=Credits current=Pistol   next=Notebook
selected=1 prev=Pistol  current=Notebook next=Credits
selected=2 prev=Notebook current=Credits next=Pistol
```

Each paint logged:

```text
visibleEntryBytes=93
persistentListBytes=0
packOwnership=preserved-open
mutation=no
turn=no
```

Exact frame fingerprints from the tested session:

```text
Inventory selected 0 = 9b93078f
Inventory selected 1 = eb109d85
Inventory selected 2 = c1795301
Status               = eea0759d
hudProtectedFNV       = bd7588ae
menuZoneFNV           = 109b46aa
playerFNV             = e745fce9
```

The supplied actions exercised downward selection `0 -> 1 -> 2`. The modulo
wrap path and top-card previous action are defined by the same bounded index
logic but were not separately claimed as hardware-observed in this log excerpt.

## Input and mutation invariants

SELECT on the centered Pistol entry was exercised repeatedly and remained
fail-closed:

```text
[HUB] SELECT-DEFER page=inventory row=0 cause=read-only-milestone
[RESIDENTGAMEPLAY] HUB-INPUT ... status=IGNORED
worldDispatch=blocked
turnAdvance=no
mutation=no
```

Down navigation produced redraw-only transitions:

```text
[HUB] CURSOR page=inventory entry=0->1 entries=3 direction=down
[HUB] CURSOR page=inventory entry=1->2 entries=3 direction=down
```

No PlayerState mutation or gameplay turn occurred.

## Status / close / world resume witness

Switching to Status reproduced the previous canonical frame exactly:

```text
Status frame = eea0759d
hudProtected = bd7588ae preserved=yes
menuZone = 109b46aa
playerFNV = e745fce9 exact=yes
```

This test also supplied the complete close witness:

```text
[HUB] CLOSE n=1 page=status
      playerFNV=e745fce9->e745fce9 exact=yes
      mutation=no turn=no
      menuUnderlayRestore=exact
      hudBands=6c2aa46f expected=6c2aa46f exact=yes
      packClosed=yes
```

The world compositor then rebuilt the exact pre-HUB frame:

```text
pre-HUB world frame  = 22397b55
post-HUB world frame = 22397b55
```

A subsequent real FORWARD input completed normally from tile 904 to 872, proving
control returned to gameplay after the modal close path.

## RAM witness

The real-CYD session remained flat before, during and after the HUB test:

```text
heap=87988
heap8=22256
largest8=14324
hub owner=28 B
player owner=52 B
MENU underlay=1280 B
persistent list bytes=0 B
visible transient entries=93 B
full framebuffer snapshot=0 B
```

These free-memory values exactly match the preceding content-label boundary.
No persistent heap regression was observed.

The invariant remains:

```text
shapeData == NULL
mediaTexels == NULL
```

## Storage behavior

Entity names are still resolved on demand from the native PAK through the compact
EntityDef catalog. The content projection preserves the caller's open PAK state,
while the outer HUB paint closes the PAK exactly once before presentation.

Hardware logs showed both:

```text
packOwnership=preserved-open
[HUB] ... packClosed=yes
```

No runtime ZIP dependency was introduced.

## Deferred on purpose

This milestone does **not** enable:

```text
weapon selection / PlayerState weapon mutation
Notebook activation
consumable confirmation/use
turn consumption from item use
Automap / Save / Load / Options / store
```

The natural next bounded semantic family after merge is weapon selection only.
Legacy `Player_selectWeapon()` changes the selected weapon and requests a view
refresh when the id changes; consumable use remains a separate transaction with
much broader side effects and must not be bundled into that milestone.

## Merge boundary

```text
hardware-tested code = 20a1c4fe6a08308aee73c1e977316abf358abdb5
status = PASS
branch policy = LOCKED
post-test commits = documentation only
```

Before the next gameplay milestone, merge this branch, reread the true GitHub
`main` SHA, then branch from that exact commit.
