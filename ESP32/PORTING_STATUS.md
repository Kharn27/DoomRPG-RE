# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port. Repository state wins over chat history. Serial logs from the real classic CYD are the final runtime authority.

## Git boundary — LOCKED milestone

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

Latest detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)

After merge, read the true GitHub `main` SHA again before creating the next `agent/*` branch.

## Permanent architecture / hard invariants

```text
Doom RPG original data/behavior
 -> ESP32-native parsers/catalogs
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> native event/script engine
 -> native gameplay
 -> native renderer
```

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
```

Do not reintroduce map-wide legacy texels, pointer-heavy desktop ownership, runtime ZIP dependence for migrated gameplay/map data, or map-wide decompression. `/DoomRPG-ESP32.pak` is the native backing store.

## Native asset backing — hardware PASS

Active gameplay storage path:

```text
/DoomRPG-ESP32.pak on microSD
 -> requested-map raw internal-flash slot
 -> 19 KiB resident RAM cache
 -> native gameplay / renderer
```

Preparation API:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

Classic CYD raw partition:

```text
nvs       0x009000  size 0x005000
otadata   0x00e000  size 0x002000
app0      0x010000  size 0x140000  = 1310720 B
spiffs    0x150000  size 0x2A0000  = 2752512 B raw slot
coredump  0x3F0000  size 0x010000
```

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

Retained fingerprints:

```text
mapStateFNV = cd99b98e
scriptFNV   = f9e3d9df
lineFNV     = e5e74861
textureFNV  = f1fc1875
automapFNV  = 669b1aa7
topologyFNV = 3f321e43
```

Resident cache baseline:

```text
owner = 23592 B
payload = 19456 B
range records = 288 x 12 B
resident entry slots = 24
large exact range = 2048 B
```

`PlatformVideo_present()` remains around 34.4 ms and is not the current optimization target.

## Current hardware-owned gameplay frontier

Validated native behavior includes movement/turn/strafe, collision/topology, event-first SELECT routing, migrated state/event families, regular door animation, mutable line textures, native weapon rendering/combat, shared PlayerState, pickups/resources, hazards, touch feedback, compact MonsterState/MonsterPosition, monster activation/sequencing/RNG/dead skip, raw-flash gameplay backing and native HUB interaction.

Current HUB architecture:

```text
EspNativeGameplayHubView = 28 B
EspNativeGameplayPlayerState = 52 B
pages = INV | WPN | STAT
MENU close = original p.bmp hand
MENU underlay = 32x20 RGB565 = 1280 B
all other HUD pixels fingerprint-protected
world dispatch blocked while HUB active
turn advance disabled while HUB active
```

### Inventory page

Weapons are no longer mixed into the scrolling Inventory presentation. INV projects only non-weapon content:

```text
Notebook
carried items 25..29
Credits
Green / Yellow / Blue / Red keys
```

Names remain on-demand from the native PAK; no persistent name list is retained.

```text
one transient projected entry = 31 B
three visible entries = 93 B
persistentListBytes = 0
persistentNameBytes = 0
catalogFNV = 34d2b7d4
legacy full-list probe FNV = 93b6a47c
```

Non-weapon SELECT remains fail-closed.

### Dedicated WPN page — final hardware PASS

The WPN page is a 4x3 direct-touch grid. The first nine slots are the normal arsenal:

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

Canonical weapon IDs 9..11 are familiar forms (`Hellhound`, `Cerberus`, `Demon Wolf`). They are intentionally excluded from this normal arsenal presentation; the final row retains three blank, non-selectable cells.

Rendering contract:

```text
owned = original sprite in color
unowned = grayscale
active/equipped = yellow border ffe0
slots 9..11 = blank
```

Weapon sprites are streamed by bounded ranges from the native PAK:

```text
EntityDef.tileIndex
 -> mappings.bin mediaSpriteIds
 -> mediaBitShapeOffsets
 -> bitshapes.bin / palettes.bin / stexels.bin
```

No ZIP fallback and no persistent icon cache are used.

Pistol uses the real Bullets pickup icon because its weapon EntityDef does not provide a useful unique presentation sprite:

```text
bullets tile = 83
media = 318
sourceSize = 16x11
drawSize = 16x11
upscale = no
```

Large sprites are only downscaled to fit; small sprites are kept at native size.

Final real-CYD WPN witness:

```text
[HUBWGRID] FRAME weapons=9/9 slots=12 blankSlots=3 icons=9 missing=0
           owned=1 equipped=2 selected=2
           ownedRender=color unavailableRender=gray
           familiarIds=9..11/excluded
           pistolIcon=bullets-pickup/native-no-upscale
           persistentIconBytes=0
           scratchBytes=2690 scratchOwner=static
           sourceMaskBytes=1447 sourceTexelBytes=2669
           assetFNV=342db611
           packOwnership=preserved-open mutation=no turn=no
```

The user visually confirmed on the real CYD that the three surplus cells are blank, only Pistol is lit on fresh start, and the Pistol icon is correct.

Fresh final WPN frame:

```text
frame = bcc52568
```

### Direct owned-weapon selection

Permanent primitive:

```text
EspNativeGameplayPlayerState_selectOwnedWeapon(weapon, &changed)
```

Contract:

```text
requires ownership bit
changes only player.weapon
never grants ownership
never consumes ammo
same weapon => changed=0
allocation-free
no turn
```

Direct grid selection in both directions was hardware-proven earlier on this branch with Fire Ext + Pistol owned. Representative changed witness:

```text
[HUBWEAPON] GRID-SELECT target=2 weapon=1->2
            status=CHANGED owned=yes
            playerFNV=a6e115a7->16e881bc
            exactOnlyWeapon=yes
            worldRedraw=on-close mutation=weapon-only turn=no
```

The final SHA hardware-reconfirmed the unchanged Pistol select and exact close:

```text
[HUBWEAPON] GRID-SELECT target=2 weapon=2->2 status=UNCHANGED
            playerFNV=e745fce9->e745fce9 exactOnlyWeapon=yes
            mutation=no turn=no
[HUB] CLOSE ... exact=yes weapon=2->2 sessionMutation=no
      menuUnderlayRestore=exact hudBands=6c2aa46f exactHud=yes
      packClosed=yes
[WEAPON] DRAW weapon=2 ...
```

## WPN stack-canary fix / scratch ownership

An earlier real-CYD build reset on entry to WPN with:

```text
Stack canary watchpoint triggered (loopTask)
```

Cause: ~2690 B icon decode workspace on the nested HUB paint stack. Final design moves this bounded scratch to a static renderer owner:

```text
scratchBytes = 2690
scratchOwner = static
persistentIconBytes = 0
```

No map-wide texel cache was introduced.

## Final RAM witness

Final fresh-session values remained flat through WPN open/select/close:

```text
heap = 85228
heap8 = 19496
largest8 = 14324
hub owner = 28 B
player owner = 52 B
MENU underlay = 1280 B
WPN icon scratch = 2690 B static owner
persistent icon cache = 0 B
full framebuffer snapshot = 0 B
```

Do not compare earlier branch heap values as a leak signal: WPN deliberately moved its bounded scratch from stack to static ownership. `largest8` remained 14324.

Audio remains deferred and must keep its own RAM milestone.

## EntityDef content corpus retained

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

## Intentionally deferred families

```text
production SAVEGAME / CHANGEMAP transition ownership
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
ranged >=217 multi-active expansion
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
HUB Notebook activation
HUB consumable confirmation/use + turn consumption
HUB Automap / Save / Load / Options / store
```

## CHANGEMAP remains deferred

Entrance event 1 / tile 69 remains recovered but intentionally not live:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

The storage layer can prepare map 9, but production teardown/load/spawn/state-transfer ownership remains separate.

## Next direction after merge

Do not continue code on this locked branch.

After the user announces the merge:

1. read actual GitHub `main` and exact SHA;
2. re-read this file, `DOCUMENTATION.md` and `MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`;
3. create a fresh `agent/*` from that exact main SHA;
4. choose the next bounded production family from the real repo/legacy source.

The HUB weapon UX is now complete enough to stop polishing. Prefer returning to gameplay progression. Strong next candidates are one coherent deferred family such as production map transition ownership, Notebook activation, or another bounded gameplay/event dependency selected after recovery from the merged `main`.

## Development workflow

```text
recover true main + docs
 -> choose one bounded behavior family
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
