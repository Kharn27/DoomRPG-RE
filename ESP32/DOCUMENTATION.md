# ESP32 documentation map

Recovery and development should start from:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — authoritative tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers;
5. the latest relevant milestone on the active branch.

Repository state wins over chat history. Serial logs from the real classic CYD are the final runtime truth.

## Current locked branch

```text
main at branch creation = d6909793860312397edc7aa1b36f995a5ccbb914
branch = agent/esp32-native-gameplay-hub-weapon-select
base main = d6909793860312397edc7aa1b36f995a5ccbb914
hardware-tested code boundary = 308295c4f56b1741040d1bc5e07ce7f66d245a70
status = REAL-CYD HUB WPN GRID + DIRECT OWNED-WEAPON SELECT PASS
branch policy = LOCKED; docs-only tail only
```

Normal GitHub Actions `esp32-cyd` run #234 / run ID `34947947008` passed on the exact tested SHA and produced artifact:

```text
doom-rpg-esp32-cyd-308295c4f56b1741040d1bc5e07ce7f66d245a70
```

Latest milestone:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)

## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

GitHub Actions builds this environment through `.github/workflows/esp32-cyd.yml`. Bring-up diagnostics perturb RAM and are not the production memory canon. Never claim a local build or hardware pass that did not occur.

The production environment uses:

```text
board_build.partitions = partitions_cyd_raw_pak.csv
```

When installing this layout on a board that previously used `no_ota.csv`, flash the generated `partitions.bin` as well as `firmware.bin`.

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

Do not recreate map-wide texel ownership or migrate native gameplay/map data back to ZIP. `/DoomRPG-ESP32.pak` remains the authoritative native asset source.

## Current native asset backing

Hardware-proven active gameplay path:

```text
/DoomRPG-ESP32.pak on SD
 -> requested-map raw internal-flash slot
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

The hardware-owned engine includes native movement/collision, event-first SELECT routing, migrated event/state families, regular doors, mutable line textures, shared PlayerState resources, native weapon/combat presentation, pickups, hazards, feedback, compact monster state/position, monster activation/sequencing, raw-flash backing and requested-map reuse.

Current HUB/UI frontier:

```text
EspNativeGameplayHubView = 28 B
EspNativeGameplayPlayerState = 52 B
pages = INV | WPN | STAT
visible MENU close uses original p.bmp hand
MENU underlay = 32x20 RGB565 = 1280 B
all other HUD pixels are fingerprint-protected
world dispatch blocked while HUB is active
no turn advance while HUB is active
```

### INV

Weapons have been removed from the scrolling Inventory presentation. INV contains Notebook, carried items, Credits and keys. Content labels still come from the native PAK on demand.

```text
persistentNameBytes=0
persistentListBytes=0
one projected entry=31 B
three visible entries=93 B
catalogFNV=34d2b7d4
legacy full-list probe FNV=93b6a47c
```

Non-weapon SELECT remains fail-closed.

### WPN

WPN is a 4x3 direct-touch arsenal grid. Slots 0..8 contain the normal weapons:

```text
Axe | Fire Ext | Pistol | Shotgun | Chaingun | Super Shotgn |
Plasma Gun | Rocket Lnchr | BFG
```

Canonical familiar weapon IDs 9..11 are intentionally not presented as normal arsenal weapons; the last three cells remain blank and non-selectable.

Presentation:

```text
owned -> original sprite in color
unowned -> grayscale
equipped -> yellow border ffe0
```

Icons stream from the native PAK through:

```text
EntityDef.tileIndex -> mediaSpriteIds -> bitshape -> palette/stexels
```

There is no persistent icon cache. The decode workspace is a bounded 2690 B static owner, moved off `loopTask` stack after a real-CYD stack-canary failure.

Pistol uses the real Bullets pickup icon (`tile=83`, `media=318`) and keeps its native `16x11` size; small icons are not upscaled.

Final real-CYD frame summary:

```text
[HUBWGRID] FRAME weapons=9/9 slots=12 blankSlots=3 icons=9 missing=0
           owned=1 equipped=2 selected=2
           familiarIds=9..11/excluded
           pistolIcon=bullets-pickup/native-no-upscale
           scratchBytes=2690 scratchOwner=static
           assetFNV=342db611
```

The user visually confirmed that the three extra cells are blank, only Pistol is highlighted at fresh start, and the Pistol icon is correct.

### Direct weapon selection

Permanent primitive:

```text
EspNativeGameplayPlayerState_selectOwnedWeapon()
```

It requires ownership and changes only `player.weapon`. It never grants a weapon, consumes ammo or advances a turn. Direct grid selection was hardware-proven earlier on this branch in both directions between Fire Ext and Pistol. The final SHA reconfirmed the unchanged Pistol selection and exact close/HUD restore.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)

## RAM at current boundary

Final real-CYD fresh-session witness:

```text
heap=85228
heap8=19496
largest8=14324
hub owner=28 B
player owner=52 B
MENU underlay=1280 B
WPN icon scratch=2690 B static owner
persistent icon cache=0 B
full framebuffer snapshot=0 B
```

`largest8` remains 14324. The lower free-heap value versus earlier branch revisions is expected because the WPN scratch was deliberately moved from stack into static ownership.

Audio remains deferred and requires its own RAM milestone.

## Milestone index for recent frontier

- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)

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
ranged >=217 active-list expansion
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

Storage can prepare target map 9, but transition teardown/load/spawn/state transfer remain their own production milestone.

## After this merge

Do not continue code on this locked branch.

When the user announces the merge:

1. read the true GitHub `main` and exact SHA;
2. re-read `PORTING_STATUS.md`, this file and `MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`;
3. create the next `agent/*` from that exact SHA;
4. pick the next bounded gameplay family from the real repo/legacy source.

The WPN UX is complete enough to stop polishing. Prefer returning to gameplay progression rather than adding more menu decoration.

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
