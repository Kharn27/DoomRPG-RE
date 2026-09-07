# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main at branch creation = 9b518a50ec3a977394634121280820853c01089b
branch = agent/esp32-native-gameplay-hub-inventory-list
base main = 9b518a50ec3a977394634121280820853c01089b
hardware-tested code boundary = 20a1c4fe6a08308aee73c1e977316abf358abdb5
status = REAL-CYD GAMEPLAY HUB INVENTORY LIST PASS
branch policy = LOCKED; docs-only tail only
```

`20a1c4fe...` is the exact code boundary exercised on the real classic CYD.
Commits after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `34100083958` / run #202 completed
successfully on this exact SHA and produced the firmware artifact. CI is
compile/link evidence only; hardware serial logs remain authoritative.

Latest detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md)

After merge, read the real GitHub `main` SHA again before creating the next
`agent/*` branch.

## Permanent architecture and hard invariants

```text
Doom RPG original data/behavior
 -> ESP32-native parsers/catalogs
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> native event/script engine
 -> native gameplay
 -> native renderer
```

Hardware / memory invariants:

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
```

Do not reintroduce map-wide legacy texels, desktop pointer-heavy world ownership,
or runtime ZIP dependence for migrated gameplay/map data. `/DoomRPG.zip` remains
transitional startup/reference debt only; native gameplay/map access must not
regress to it.

## Native asset backing — hardware PASS

Active gameplay storage path:

```text
/DoomRPG-ESP32.pak on microSD
 -> generic requested-map raw internal-flash slot
 -> 19 KiB resident RAM cache
 -> native gameplay / renderer
```

Permanent preparation API:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

Classic CYD raw partition layout:

```text
nvs       0x009000  size 0x005000
otadata   0x00e000  size 0x002000
app0      0x010000  size 0x140000  = 1310720 B
spiffs    0x150000  size 0x2A0000  = 2752512 B raw slot
coredump  0x3F0000  size 0x010000
```

The partition named `spiffs` is intentionally not mounted as a filesystem and is
managed through `esp_partition_*`.

Entrance raw-slot witness:

```text
pack = 2457398 B
entries = 241
index = 4820 B
metadata = 12288 B
excluded non-current BSPs = 12 / 203811 B
staged payload = 2248743 B
partition = 2752512 B
headroom = 491481 B
[MAPFLASH] COPY indexFNV=3a51cc4d payloadFNV=9ec04e22 verified=yes
```

Requested-map reuse witness:

```text
[MAPFLASH] REUSE HIT requestedMap=1 current=/intro.bsp cachedMap=1
           sourceIndexFNV=3a51cc4d payloadFNV=9ec04e22
           verifyUs=361875 rebuild=no
```

Active gameplay never silently falls back to SD.

## Entrance canonical witness

```text
resourceMapId = 1
resource = /intro.bsp
name = Entrance
sourceBytes = 21823
crc32 = 623f34e4
sourceFNV = d5cc751f
runtime arena = 14095 B
runtimeFNV = c3882516
resident payload = 17891 B
spawn tile = 904
spawn direction = 64
spawn position = 544,1824
nodes = 223
lines = 480
sprites = 344
events = 93
byteCodes = 265
strings = 94
native topology entities = 220
enemies = 30
destructibles = 13
```

Retained owner fingerprints:

```text
mapStateFNV = cd99b98e
scriptFNV   = f9e3d9df
lineFNV     = e5e74861
textureFNV  = f1fc1875
automapFNV  = 669b1aa7
topologyFNV = 3f321e43
```

## Resident cache baseline retained

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

`PlatformVideo_present()` remains around 34.4 ms and is not the current
optimization target.

## Current hardware-owned gameplay frontier

Validated behavior includes movement/turn/strafe, native collision/topology,
SELECT event-first routing, state/event opcodes already migrated, regular door
animation, mutable line textures, native weapon presentation/combat, shared
52 B PlayerState, pickups/resources, hazard touch, feedback flashes, compact
MonsterState/MonsterPosition, live monster movement/topology relink, active-list
sequencing, raw-flash gameplay backing and requested-map reuse.

The current permanent UI/content frontier adds:

```text
native gameplay HUB owner = 28 B
shared PlayerState owner = 52 B
read-only Inventory + Status pages
visible native INV / STATUS touch tabs
visible MENU close affordance uses original Doom RPG p.bmp hand
bounded MENU underlay = 32x20 RGB565 = 1280 B
all other HUD pixels protected by fingerprint
real EntityDef weapon/item names resolved on demand from native PAK
persistent name storage = 0 B
bounded Inventory projection max = 23 entries
persistent Inventory-list storage = 0 B
one transient projected entry = 31 B
three visible entries = 93 B transient
legacy content order = owned weapons, Notebook, carried items, Credits, keys
three-card viewport = previous / selected / next
SELECT remains fail-closed
world dispatch blocked and turn advance disabled while HUB is active
```

Detailed recent records:

- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md)

## EntityDef content corpus retained

The preceding hardware pass recovered all player weapon and consumable names
through the compact native catalog:

```text
weapon 0  tile 1   Axe
weapon 1  tile 2   Fire Ext
weapon 2  tile 9   Pistol
weapon 3  tile 3   Shotgun
weapon 4  tile 5   Chaingun
weapon 5  tile 4   Super Shotgn
weapon 6  tile 7   Plasma Gun
weapon 7  tile 6   Rocket Lnchr
weapon 8  tile 8   BFG
weapon 9  tile 10  Hellhound
weapon 10 tile 11  Cerberus
weapon 11 tile 12  Demon Wolf
item 25   tile 99  Sm Medkit
item 26   tile 100 Lg Medkit
item 27   tile 101 Soul Sphere
item 28   tile 102 Berserker
item 29   tile 110 Dog Collar
```

Canonical content-catalog fingerprint remains:

```text
catalogFNV = 34d2b7d4
persistentNameBytes = 0
```

## Latest real-CYD HUB Inventory-list witness

The exact hardware-tested code boundary is `20a1c4fe...`.

A strict maximum-list probe projected the complete bounded 23-entry shape using
a local synthetic PlayerState only; canonical gameplay ownership was never
mutated. Hardware emitted all entries in recovered legacy order and closed with:

```text
[HUBLIST] READY entries=23/23 order=legacy-content
          persistentListBytes=0 transientEntryBytes=31
          listFNV=93b6a47c
          packOwnership=preserved-open mutation=no turn=no
```

`listFNV=93b6a47c` is the canonical maximum-list fingerprint.

Fresh Entrance live PlayerState projected exactly three entries:

```text
0 weapon   Pistol   8
1 notebook Notebook --
2 credits  Credits  0
```

Real-CYD list windows and frame fingerprints:

```text
selected 0: prev=Credits current=Pistol   next=Notebook frame=9b93078f
selected 1: prev=Pistol  current=Notebook next=Credits  frame=eb109d85
selected 2: prev=Notebook current=Credits next=Pistol   frame=c1795301
Status frame = eea0759d
hudProtectedFNV = bd7588ae
menuZoneFNV = 109b46aa
playerFNV = e745fce9
```

Each list frame reported:

```text
visibleEntryBytes=93
persistentListBytes=0
packOwnership=preserved-open
mutation=no turn=no
```

The supplied live actions exercised downward selection `0 -> 1 -> 2`. The
bounded modulo wrap and top-card previous action are implemented by the same
index route but are not claimed as separately observed in this hardware excerpt.

Repeated SELECT on the centered entry stayed fail-closed:

```text
[HUB] SELECT-DEFER page=inventory row=0 cause=read-only-milestone
[RESIDENTGAMEPLAY] HUB-INPUT ... status=IGNORED
worldDispatch=blocked
turnAdvance=no
mutation=no
```

## Close / world resume witness

This milestone supplied a complete bit-exact close witness:

```text
[HUB] CLOSE n=1 page=status
      playerFNV=e745fce9->e745fce9 exact=yes
      mutation=no turn=no
      menuUnderlayRestore=exact
      hudBands=6c2aa46f expected=6c2aa46f exact=yes
      packClosed=yes
```

The compositor restored the exact pre-HUB world frame:

```text
pre-HUB world frame  = 22397b55
post-HUB world frame = 22397b55
```

A subsequent real FORWARD action completed from tile 904 to tile 872, proving
normal gameplay ownership resumed after closing the HUB.

## RAM witness at current boundary

The supplied real-CYD session remained flat before, during and after list use:

```text
heap = 87988
heap8 = 22256
largest8 = 14324
hub owner = 28 B
player owner = 52 B
MENU underlay = 1280 B
persistent HUB name bytes = 0 B
persistent Inventory-list bytes = 0 B
visible transient entries = 93 B
full framebuffer snapshot = 0 B
```

These values exactly match the previous content-label boundary: no persistent
heap loss was observed from the Inventory-list layer.

Audio remains deferred. Do not enable it without its own RAM milestone.

## Intentionally deferred families

```text
production SAVEGAME / CHANGEMAP transition ownership
pre-arm first-frame/HUD SD startup path
L1 range-record eviction/recycle redesign
pickup sound playback / got-face presentation
movement/PASS_TURN hazard secondary burn text / pain face / shake / sound
complete mixed movement-tile resource/hazard ordering
action XP migration
materialized monster drops
corpse-pile trimming
monster movement interpolation/animation
simultaneous attack-ready multi-monster ordering
ranged >=217 multi-active expansion beyond current single-candidate boundary
unsupported special calcPath plane corpus
special subtype-10 AI
player lethal/death transition
three-shot / multi-loop monster attack presentation
monster projectile visuals
monster attack message / sound
player-pain face / shake / sound
status-warning presentation
chaingun/plasma multi-loop player mechanics
rocket/BFG radius damage
familiar weapon slots / hazard redirection
generic type-12 destructible combat
special death consequences
Kronos-specific semantics
password input
EV_GIVEMAP production route
EV_CHECK_KEY production route
HUB weapon selection
HUB Notebook activation
HUB consumable confirmation/use + turn consumption
HUB Automap / Save / Load / Options / store
```

Different mechanical families stay fail-closed rather than being enabled through
legacy desktop/J2ME ownership.

## CHANGEMAP remains deferred

Entrance event 1 / tile 69 remains recovered but intentionally not live:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

The storage layer can prepare map 9, but the actual production transition still
needs bounded teardown/load/spawn/state-transfer ownership.

## Next direction after merge

Do **not** continue code on this locked branch.

After the user announces the merge:

1. read actual GitHub `main` and exact SHA;
2. re-read this file, `DOCUMENTATION.md` and
   `MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`;
3. create a fresh `agent/*` from that exact main SHA;
4. recover the next bounded legacy/UI family before coding.

Strong next HUB milestone: **weapon selection only**. Legacy `MENU_ITEMS` routes a
weapon entry to `Player_selectWeapon()`, whose core semantic is deliberately
small: when the id differs it requests a view refresh, then updates the current
weapon id. The ESP32 milestone should translate that into the canonical
PlayerState plus explicit native redraw intent without enabling Notebook or item
use. Consumables remain a separate transaction because they can alter health,
berserker state, familiar/world state, messages, sounds and turns.

## Development workflow

```text
recover true main + docs
 -> choose one bounded behavior FAMILY
 -> recover exact legacy behavior
 -> design small permanent native API/owner
 -> keep genuinely different families fail-closed
 -> commit/push agent/*
 -> test normal esp32-cyd on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> verify tested SHA + docs-only commits
 -> merge-ready
```

Never merge into `main` without explicit user request.
