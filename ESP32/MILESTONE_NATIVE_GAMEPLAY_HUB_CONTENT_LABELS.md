# Milestone — Native gameplay HUB content labels

## Result

**REAL CLASSIC CYD HARDWARE PASS**

This milestone keeps the proven gameplay HUB strictly read-only while replacing
prototype numeric Inventory content with original Doom RPG weapon/item names and
ammo labels resolved through bounded native metadata.

```text
base main = 711f391ea3d319012f919e4ecae0d4b00e9177d1
branch = agent/esp32-native-gameplay-hub-content-labels
hardware-tested code boundary = 5d8cf35816c279f8968430474d072d2419cc2bea
normal esp32-cyd CI = run 34097802996 / #192 = SUCCESS
hardware = ESP32-2432S028R classic CYD, no PSRAM
```

Serial output from the real board is the runtime authority. Commits after the
hardware-tested code SHA are documentation-only.

## Scope

The milestone intentionally changes presentation content only:

- Inventory cards render real Doom RPG weapon names from `/entities.db`;
- ammo labels use the legacy player-weapon ammo mapping;
- the first carried consumable renders its real EntityDef name and count;
- no weapon selection is enabled;
- no item use/confirmation is enabled;
- SELECT remains fail-closed/read-only;
- no player/world mutation and no turn advance are introduced;
- existing touch hitboxes, MENU hand underlay, HUD protection and close path are
  unchanged.

The HUB owner remains 28 B and the canonical PlayerState remains 52 B.

## Permanent native API added

The compact entity definition catalog already retained sorted
`{tileIndex,type,subtype,parm}` metadata and source indices for on-demand names.
This milestone adds the reverse query needed by UI/content code:

```text
(type, subtype) -> tileIndex
```

The lookup preserves legacy `EntityDef_find()` semantics: the first matching
record in original `/entities.db` source order wins.

Names are still read on demand from the native PAK. No per-definition names are
retained in resident RAM.

The HUB content layer owns only transient stack data for the current paint. The
hardware log reports:

```text
persistentNameBytes=0
transientBytes=40
packOwnership=preserved-open
```

## Exact entity-definition corpus recovered on hardware

The first Inventory paint performs a strict one-shot corpus witness for all 12
player weapon definitions and all five consumable definitions.

Weapons:

```text
subtype  tile  name          ammoType  ammoUsage
0        1     Axe           0         0
1        2     Fire Ext      0         1
2        9     Pistol        1         1
3        3     Shotgun       2         1
4        5     Chaingun      1         3
5        4     Super Shotgn  2         2
6        7     Plasma Gun    4         3
7        6     Rocket Lnchr  3         1
8        8     BFG           4         15
9        10    Hellhound     5         0
10       11    Cerberus      5         0
11       12    Demon Wolf    5         0
```

Consumables:

```text
subtype  tile  name
25       99    Sm Medkit
26       100   Lg Medkit
27       101   Soul Sphere
28       102   Berserker
29       110   Dog Collar
```

The complete hardware witness closed with:

```text
[HUBCONTENT] READY weapons=12/12 items=5/5
             names=pak-on-demand persistentNameBytes=0
             catalogFNV=34d2b7d4
             packOwnership=preserved-open mutation=no turn=no
```

This `catalogFNV=34d2b7d4` is now the canonical content-catalog fingerprint for
this exact source corpus and lookup rule.

## Inventory rendering witness

The tested player state was the canonical fresh Entrance state:

```text
weapon=2
weapons=004
ammo=00/08/00/00/00/00
items=00/00/00/00/00
keys=00000000
credits=0
playerFNV=e745fce9
```

The new native content layer resolved and painted:

```text
weapon=2 name="Pistol"
ammoType=1 ammoLabel="Bullets" ammo=8 usage=1
item=none count=0
persistentNameBytes=0
```

Representative frame log:

```text
[HUBCONTENT] FRAME weapon=2 name="Pistol"
             ammoType=1 ammoLabel="Bullets" ammo=8 usage=1
             item=none count=0 selected=0
             transientBytes=40 persistentNameBytes=0
             fontReads=27 fontBytes=21666
             packOwnership=preserved-open mutation=no turn=no
```

The same content remained exact while the Inventory selection moved through all
three existing cards:

```text
selected row 0 frame = 6b7713d7
selected row 1 frame = a264f40b
selected row 2 frame = 1db33153
```

The touch path remained the existing semantic route:

```text
row 0 SELECT -> SELECT-DEFER / IGNORED
row 0 -> row 1 -> row 2 -> REDRAWN only
worldDispatch=blocked
turnAdvance=no
mutation=no
```

## HUD / page invariants retained

Every supplied HUB paint retained the prior protected HUD and MENU fingerprints:

```text
hudProtectedFNV = bd7588ae preserved=yes
menuZoneFNV = 109b46aa
menuButton = hand asset=p.bmp frame=0
MENU underlay = 1280 B
playerFNV = e745fce9 exact=yes
packClosed=yes after page paint
```

Switching to Status produced the previously canonical Status frame:

```text
Status frame = eea0759d
```

The user also confirmed that closing the HUB returned cleanly to gameplay. The
supplied excerpt did not include the final `[HUB] CLOSE` fingerprint line, so no
new close fingerprint is invented here; the close implementation itself is
unchanged from the previous hardware-locked touch-UI milestone.

## RAM witness

The real-CYD session remained flat while opening, selecting rows and switching to
Status:

```text
heap=87988
heap8=22256
largest8=14324
```

These values exactly match the preceding HUB touch-UI hardware boundary. The new
content-label implementation therefore introduced no observed persistent heap
loss in this session.

The presentation still uses no full-frame snapshot and keeps:

```text
shapeData == NULL
mediaTexels == NULL
```

## Storage behavior

Name/font reads use the already-proven native PAK/raw-flash path. The content
helper preserves the caller's open PAK ownership while painting, then the outer
HUB paint closes the PAK exactly once as before.

Hardware logs explicitly showed:

```text
packOwnership=preserved-open
[HUB] ... packClosed=yes
```

No runtime ZIP dependency was added.

## Deferred on purpose

This milestone does **not** enable:

```text
weapon selection
weapon mutation
consumable selection beyond read-only first-present display
consumable confirmation/use
turn consumption from HUB item use
Notebook / Automap / Save / Load / Options / store
```

Those remain separate bounded semantic milestones.

## Merge boundary

```text
hardware-tested code = 5d8cf35816c279f8968430474d072d2419cc2bea
status = PASS
branch policy = LOCKED
post-test commits = documentation only
```

Before any next gameplay milestone, merge this branch, reread the true GitHub
`main` SHA, then branch from that exact commit.
