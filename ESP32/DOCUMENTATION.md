# ESP32 documentation map

Recovery and development should start from:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — authoritative tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers;
5. the latest relevant milestone on the active branch.

Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime truth.

## Current locked branch

```text
main at branch creation = 9b518a50ec3a977394634121280820853c01089b
branch = agent/esp32-native-gameplay-hub-inventory-list
base main = 9b518a50ec3a977394634121280820853c01089b
hardware-tested code boundary = 20a1c4fe6a08308aee73c1e977316abf358abdb5
status = GAMEPLAY HUB INVENTORY LIST PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `20a1c4fe...` as new hardware-tested code. The tail
must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `34100083958` / run #202 passed on the
exact tested SHA and produced the firmware artifact. CI is compile/link evidence
only; real-CYD serial logs remain authoritative.

Latest milestone:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md)

## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

GitHub Actions builds this environment through `.github/workflows/esp32-cyd.yml`.
Bring-up diagnostics perturb RAM and are not the production memory canon. Never
claim a local build or hardware pass that did not occur.

The production environment uses:

```text
board_build.partitions = partitions_cyd_raw_pak.csv
```

When installing this layout on a board that previously used `no_ota.csv`, flash
the generated `partitions.bin` as well as `firmware.bin`.

## Hardware / permanent memory rules

```text
classic CYD = ESP32-2432S028R
MCU = ESP32-D0WD-V3 dual core 240 MHz
flash = 4 MB
PSRAM = none
logical framebuffer = 160x120 RGB565 = 38400 B
shapeData == NULL
mediaTexels == NULL
```

Do not recreate map-wide texel ownership or migrate native gameplay/map data back
to ZIP. `/DoomRPG-ESP32.pak` on SD remains the authoritative source archive.

## Current native asset backing

Hardware-proven active gameplay path:

```text
/DoomRPG-ESP32.pak on SD
 -> requested-map identity / staging plan
 -> exact raw-slot HIT or full rebuild on mismatch
 -> raw internal-flash requested-map slot
 -> 19 KiB resident RAM cache
 -> native renderer/gameplay
```

Preparation API:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

Classic CYD raw partition:

```text
nvs       0x009000  0x005000
otadata   0x00e000  0x002000
app0      0x010000  0x140000 = 1310720 B
spiffs    0x150000  0x2A0000 = 2752512 B raw slot
coredump  0x3F0000  0x010000
```

Entrance storage witness:

```text
pack=2457398 B
entries=241
index=4820 B
metadata=12288 B
excluded other BSPs=12 / 203811 B
staged payload=2248743 B
partition=2752512 B
headroom=491481 B
[MAPFLASH] COPY indexFNV=3a51cc4d payloadFNV=9ec04e22 verified=yes
validated reuse ~= 0.363 s
```

## Entrance format witness

```text
map=1
resource=/intro.bsp
sourceBytes=21823
sourceCRC32=623f34e4
sourceFNV=d5cc751f
runtime arena=14095 B
runtimeFNV=c3882516
resident payload=17891 B
spawn tile=904
spawn position=544,1824
spawn direction=64
nodes=223
lines=480
sprites=344
events=93
byteCodes=265
strings=94
native topology entities=220
enemies=30
destructibles=13
```

Canonical retained fingerprints:

```text
mapStateFNV=cd99b98e
scriptFNV=f9e3d9df
lineFNV=e5e74861
textureFNV=f1fc1875
automapFNV=669b1aa7
topologyFNV=3f321e43
```

## Resident cache baseline

```text
owner=23592 B
payload=19456 B
range records=288
range record=12 B
resident entry slots=24
large exact range=2048 B
```

`PlatformVideo_present()` remains around 34.4 ms and is not the current target.

## Current native gameplay frontier

The hardware-owned engine already includes native movement/collision, event-first
SELECT routing, migrated event/state families, regular doors, mutable line
textures, shared PlayerState resources, native weapon/combat presentation,
pickups, hazards, feedback, compact monster state/position, live monster movement
and topology relink, active-list sequencing, raw-flash backing and requested-map
reuse.

The UI/content frontier is now also hardware-owned:

```text
EspNativeGameplayHubView = 28 B
EspNativeGameplayPlayerState = 52 B
Inventory + Status remain read-only
visible INV / STATUS tabs
visible MENU close uses original p.bmp hand
MENU underlay = 32x20 RGB565 = 1280 B
all other HUD pixels fingerprint-protected
real EntityDef weapon/item names are read on demand
persistent HUB name storage = 0 B
Inventory max projection = 23 entries
persistent Inventory-list storage = 0 B
one transient entry = 31 B
three visible entries = 93 B transient
legacy content order is preserved
visible Inventory window = previous/current/next
SELECT remains fail-closed
```

Milestone index for the latest frontier:

- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md)

## Native gameplay HUB content/list result

The HUB remains a compact ESP32-native modal UI over the canonical 52 B
PlayerState. Desktop/J2ME `MenuSystem`, `EntityDef` and `Player` remain behavior
and data references, not runtime ownership.

The compact native EntityDef catalog supports:

```text
tileIndex -> {type, subtype, parm}
(type, subtype) -> tileIndex
```

Historical names are read from `/entities.db` in the native PAK only when needed.
No per-definition names are retained. The canonical catalog fingerprint remains:

```text
catalogFNV=34d2b7d4
persistentNameBytes=0
```

The new Inventory projection mirrors the legacy `MENU_ITEMS` content order while
omitting `Back` and divider rows already represented by native controls:

```text
owned weapons 0..11
Notebook
carried items 25..29
Credits
Green Key
Yellow Key
Blue Key
Red Key
```

Allocation-free APIs:

```text
EspNativeGameplayHubContent_inventoryEntryCount(player)
EspNativeGameplayHubContent_inventoryEntryAt(player, index, outEntry)
```

Maximum bounded shape:

```text
12 weapons + Notebook + 5 items + Credits + 4 keys = 23 entries
one projected entry = 31 B
three visible entries = 93 B
persistentListBytes = 0
```

A local synthetic PlayerState was used only for the strict maximum-shape probe;
the canonical gameplay owner was not mutated. Real-CYD output proved all 23
entries in order and produced:

```text
[HUBLIST] READY entries=23/23 order=legacy-content
          persistentListBytes=0 transientEntryBytes=31
          listFNV=93b6a47c
          packOwnership=preserved-open mutation=no turn=no
```

`listFNV=93b6a47c` is the canonical maximum-list fingerprint.

On the fresh Entrance state the live list was exactly:

```text
0 weapon   Pistol   8
1 notebook Notebook --
2 credits  Credits  0
```

The hardware session observed selection `0 -> 1 -> 2` and these exact frames:

```text
selected 0 = 9b93078f
selected 1 = eb109d85
selected 2 = c1795301
Status     = eea0759d
hudProtected = bd7588ae preserved=yes
menuZone     = 109b46aa
playerFNV    = e745fce9 exact=yes
```

The top-card previous action and modulo wrap are implemented by the same bounded
index route but are not separately claimed as hardware-observed in this supplied
log excerpt.

Repeated SELECT remained `SELECT-DEFER` / `IGNORED` with
`worldDispatch=blocked`, `turnAdvance=no`, and `mutation=no`.

Detailed records:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md)

## Close and resume result

The latest real-CYD test supplied the complete close witness:

```text
menuUnderlayRestore=exact
hudBands=6c2aa46f expected=6c2aa46f exact=yes
playerFNV=e745fce9->e745fce9 exact=yes
packClosed=yes
mutation=no
turn=no
```

The resident gameplay compositor rebuilt the exact pre-HUB frame:

```text
22397b55 -> HUB -> 22397b55
```

A subsequent FORWARD action completed from tile 904 to tile 872, confirming
normal gameplay input/turn ownership resumed after the modal close.

## RAM at current boundary

The real-CYD session remained flat while probing, scrolling, switching page,
closing and returning to gameplay:

```text
heap=87988
heap8=22256
largest8=14324
hub owner=28 B
player owner=52 B
MENU underlay=1280 B
persistent HUB name bytes=0 B
persistent Inventory-list bytes=0 B
visible transient entries=93 B
full framebuffer snapshot=0 B
```

These values exactly match the preceding content-label hardware boundary. Treat
them as canonical before future RAM-heavy work.

Audio remains deferred and requires its own RAM milestone.

## Current intentionally deferred families

```text
production SAVEGAME / CHANGEMAP ownership
pre-arm first-frame/HUD SD startup path
L1 range-record eviction/recycle redesign
pickup sound / got-face
secondary hazard burn/pain/shake/sound
complete mixed movement-tile resource/hazard ordering
action XP migration
materialized monster drops
corpse-pile trimming
monster movement interpolation animation
simultaneous attack-ready multi-monster ordering
ranged >=217 active-list expansion beyond current single candidate
unsupported special calcPath plane corpus
special subtype-10 AI
player lethal/death transition
three-shot / multi-loop monster attack presentation
monster projectiles
monster attack message / sound
player pain face / shake / sound
status-warning presentation
chaingun/plasma multi-loop mechanics
rocket/BFG radius damage
familiar weapon slots / hazard redirection
generic type-12 destructible combat
special death consequences
Kronos-specific semantics
password input
GIVEMAP production route
CHECK_KEY production route
HUB weapon selection
HUB Notebook activation
HUB consumable confirmation/use + turn consumption
HUB Automap / Save / Load / Options / store
```

## CHANGEMAP recovery point

Entrance event 1 / tile 69 remains recovered but intentionally deferred:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

Storage can prepare target map 9, but transition teardown/load/spawn and state
transfer remain their own production milestone.

## After this merge

Do not continue code on this locked branch.

When the user announces the merge:

1. read the true GitHub `main` and exact SHA;
2. re-read `PORTING_STATUS.md`, this file and
   `MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`;
3. create the next `agent/*` from that exact SHA;
4. recover the next bounded legacy/UI family against the true source before coding.

Strong next candidate: **HUB weapon selection only**. The recovered legacy
`Player_selectWeapon(player, i)` has a small core contract: if the weapon id
changes it requests a view refresh, then stores the new weapon id. The ESP32
version should mutate only the canonical PlayerState weapon field plus a bounded
native redraw intent. Notebook and consumable actions remain fail-closed; item
use has broader health/familiar/message/sound/turn semantics and belongs in its
own milestone.

## Development workflow

```text
recover true main + docs
 -> choose one bounded behavior family
 -> recover exact legacy behavior
 -> design a small permanent native API/owner
 -> keep different families fail-closed
 -> commit + push agent/*
 -> test normal esp32-cyd on the real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> verify tested SHA + docs-only commits
 -> merge-ready
```

Never merge into `main` without explicit user request.
