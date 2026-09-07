# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main at branch creation = d6909793860312397edc7aa1b36f995a5ccbb914
branch = agent/esp32-native-gameplay-hub-weapon-select
base main = d6909793860312397edc7aa1b36f995a5ccbb914
hardware-tested code boundary = 1a4ab97ef5cfd509663be28db92d22610bd77cf6
status = REAL-CYD HUB OWNED-WEAPON SELECT PASS
branch policy = LOCKED; docs-only tail only
```

`1a4ab97...` is the exact code boundary exercised on the real classic CYD.
Commits after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `34108679822` / run #215 completed
successfully on this exact SHA and produced artifact
`doom-rpg-esp32-cyd-1a4ab97ef5cfd509663be28db92d22610bd77cf6`.
CI is compile/link evidence only; hardware serial logs remain authoritative.

Latest detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)

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

The native HUB frontier now includes:

```text
native gameplay HUB owner = 28 B
shared PlayerState owner = 52 B
Inventory + Status pages
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
owned-weapon SELECT = live
non-weapon SELECT = fail-closed
world dispatch blocked and turn advance disabled while HUB is active
```

Detailed recent records:

- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)

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

Canonical content fingerprints:

```text
catalogFNV = 34d2b7d4
listFNV = 93b6a47c
persistentNameBytes = 0
persistentListBytes = 0
```

## Latest real-CYD HUB owned-weapon selection witness

The exact hardware-tested code boundary is `1a4ab97...`.

The tested live player had multiple owned weapons and one item:

```text
weapon=1
weapons=006
ammo=10/12/00/00/00/00
items=01/00/00/00/00
keys=00000000
credits=0
playerFNV=a6e115a7
```

The five-entry Inventory included Fire Ext and Pistol. Hardware first observed an
unchanged selection of the current Fire Ext:

```text
[HUBWEAPON] SELECT entry=0 name="Fire Ext"
            weapon=1->1 status=UNCHANGED
            playerFNV=a6e115a7->a6e115a7
            exactOnlyWeapon=yes mutation=no turn=no
```

The decisive changed selection then centered Pistol and invoked SELECT:

```text
[HUBWEAPON] SELECT entry=1 name="Pistol"
            weapon=1->2 status=CHANGED owned=yes
            playerFNV=a6e115a7->16e881bc
            exactOnlyWeapon=yes
            worldRedraw=on-close
            mutation=weapon-only turn=no packClosed=yes
```

This proves the native transaction changed only the current weapon field. It did
not grant a weapon bit, consume ammo, dispatch world behavior or advance a turn.

## Close / world resume witness at weapon-select boundary

The close path correctly accepted the new expected fingerprint:

```text
[HUB] CLOSE n=5 page=inventory
      playerFNV=a6e115a7->16e881bc
      expected=16e881bc exact=yes
      weapon=1->2 sessionMutation=weapon-only
      turn=no
      menuUnderlayRestore=exact
      hudBands=c340d7ba expectedHud=c340d7ba exactHud=yes
      packClosed=yes
```

The normal compositor then rendered the newly selected Pistol:

```text
[WEAPON] DRAW weapon=2 logical=242 actual=610 frame=0 pose=idle
[RESIDENTGAMEPLAY] FRAME reason=HUB-CLOSE frame=d222e76b presented=1
```

So weapon selection survives close and is immediately live in gameplay without a
turn advance.

## RAM witness at current boundary

The supplied real-CYD session stayed flat around the changed selection and close:

```text
heap = 87356
heap8 = 21624
largest8 = 14324
hub owner = 28 B
player owner = 52 B
MENU underlay = 1280 B
persistent HUB name bytes = 0 B
persistent Inventory-list bytes = 0 B
visible transient entries = 93 B
full framebuffer snapshot = 0 B
```

The previous Inventory-list milestone recorded `87988 / 22256 / 14324`. The new
session is 632 B lower in free heap and free 8-bit heap while `largest8` is
unchanged. No persistent HUB owner was added, so do not label this as a leak from
these logs alone; treat `87356 / 21624 / 14324` as the exact current witness and
re-check before RAM-heavy work.

Audio remains deferred. Do not enable it without its own RAM milestone.

## Hardware UX finding

The current semantic implementation passes, but the touch interaction is not yet
ideal for a direct-touch device:

```text
top visible card    -> previous
center visible card -> SELECT current
bottom visible card -> next
```

A visible weapon on the top or bottom card requires one tap to center it and a
second tap on the center card to equip it. The real-CYD tester explicitly found
this unintuitive. This is an interaction-layer issue, not a failed weapon-state
transaction.

Do not modify this locked branch after the hardware PASS. Handle direct one-tap
visible-weapon selection as the next bounded HUB touch milestone.

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
HUB direct-touch weapon-selection UX
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
   `MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`;
3. create a fresh `agent/*` from that exact main SHA;
4. recover the next bounded legacy/UI family before coding.

Strong next HUB milestone: **direct-touch weapon selection UX only**. Keep the
hardware-proven PlayerState transaction unchanged, but make a tap on any visible
weapon card intentionally select/equip that weapon without requiring the separate
center-then-SELECT gesture. Non-weapon cards must remain fail-closed. Preserve the
28 B HUB owner, bounded touch-feedback edit limit, 1280 B MENU underlay, no turn,
no world dispatch while active, and rollback/fingerprint discipline.

Notebook and consumable actions remain separate semantic milestones.

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
