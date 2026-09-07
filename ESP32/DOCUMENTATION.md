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
main at branch creation = d6909793860312397edc7aa1b36f995a5ccbb914
branch = agent/esp32-native-gameplay-hub-weapon-select
base main = d6909793860312397edc7aa1b36f995a5ccbb914
hardware-tested code boundary = 1a4ab97ef5cfd509663be28db92d22610bd77cf6
status = REAL-CYD HUB OWNED-WEAPON SELECT PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `1a4ab97...` as new hardware-tested code. The tail
must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `34108679822` / run #215 passed on the
exact tested SHA and produced artifact
`doom-rpg-esp32-cyd-1a4ab97ef5cfd509663be28db92d22610bd77cf6`.
CI is compile/link evidence only; real-CYD serial logs remain authoritative.

Latest milestone:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)

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

The HUB/UI frontier is now:

```text
EspNativeGameplayHubView = 28 B
EspNativeGameplayPlayerState = 52 B
Inventory + Status pages
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
owned-weapon SELECT is live
non-weapon SELECT stays fail-closed
no world dispatch / no turn while HUB active
```

Milestone index for the latest frontier:

- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_CONTENT_LABELS.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)

## Native HUB content / list / weapon-selection result

The compact native EntityDef catalog supports:

```text
tileIndex -> {type, subtype, parm}
(type, subtype) -> tileIndex
```

Historical names are read from `/entities.db` in the native PAK only when needed.
No per-definition names are retained.

Canonical content/list fingerprints:

```text
catalogFNV=34d2b7d4
listFNV=93b6a47c
persistentNameBytes=0
persistentListBytes=0
```

The Inventory projection mirrors legacy `MENU_ITEMS` content order while omitting
Back/divider rows already represented by native controls:

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

Allocation-free projection APIs:

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

The new live mutation is deliberately narrow. The permanent primitive
`EspNativeGameplayPlayerState_selectOwnedWeapon()`:

```text
accepts only weapon ids 0..11
requires the corresponding ownership bit
changes only player.weapon
never grants ownership
never consumes ammo
returns changed=0 for reselecting the active weapon
allocates nothing
consumes no turn
```

It is intentionally separate from `adoptWeapon()`, which also sets an ownership
bit and would be incorrect for menu selection.

## Real-CYD weapon selection result

The tested live state contained Fire Ext + Pistol:

```text
weapon=1
weapons=006
ammo=10/12/00/00/00/00
items=01/00/00/00/00
playerFNV=a6e115a7
```

An unchanged select of Fire Ext was observed first. The decisive changed select
was:

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

The HUB close validated the new current fingerprint and restored the protected
HUD exactly:

```text
playerFNV=a6e115a7->16e881bc
expected=16e881bc exact=yes
weapon=1->2
sessionMutation=weapon-only
menuUnderlayRestore=exact
hudBands=c340d7ba expectedHud=c340d7ba exactHud=yes
packClosed=yes
```

The normal compositor then emitted:

```text
[WEAPON] DRAW weapon=2 ... pose=idle
[RESIDENTGAMEPLAY] FRAME reason=HUB-CLOSE frame=d222e76b presented=1
```

This proves the selected Pistol became the live gameplay weapon with no turn
advance and no fallthrough to world dispatch.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)

## UX finding from the hardware test

The semantic transaction passes, but the present three-card touch interaction is
not sufficiently direct:

```text
top card    -> previous
center card -> SELECT current
bottom card -> next
```

Touching a visible weapon on the top or bottom card merely centers it. The user
then has to tap the centered card a second time to equip it. This felt
unintuitive on the real CYD.

Treat this as the next interaction-layer milestone, not as a failure of the
PlayerState selection transaction. Keep this tested branch locked.

## RAM at current boundary

The real-CYD session stayed flat through changed weapon selection and close:

```text
heap=87356
heap8=21624
largest8=14324
hub owner=28 B
player owner=52 B
MENU underlay=1280 B
persistent HUB name bytes=0 B
persistent Inventory-list bytes=0 B
visible transient entries=93 B
full framebuffer snapshot=0 B
```

The preceding Inventory-list milestone recorded `87988 / 22256 / 14324`; the
current witness is 632 B lower in free heap/8-bit heap with unchanged largest8.
No persistent HUB owner was added, so do not infer a leak from that delta alone.
Re-check it before future RAM-heavy work.

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
HUB direct-touch weapon-selection UX
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
   `MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`;
3. create the next `agent/*` from that exact SHA;
4. recover the next bounded legacy/UI family against the true source before coding.

Strong next candidate: **direct-touch weapon selection UX only**. Reuse the
hardware-proven owned-weapon transaction unchanged, but make a tap on a visible
weapon card equip that weapon in one intentional touch instead of requiring
center-then-select. Non-weapon entries must remain fail-closed. Preserve the 28 B
HUB owner, 52 B PlayerState, bounded touch-feedback edit budget, 1280 B MENU
underlay, no turn and no world dispatch while the HUB is active.

Notebook and consumable semantics remain separate milestones.

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
