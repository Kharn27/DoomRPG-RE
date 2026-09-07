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
main at branch creation = bc9e0435f6ee6ea23a68cb4bdbb18324440cf360
branch = agent/esp32-native-gameplay-hub-touch-ui
base main = bc9e0435f6ee6ea23a68cb4bdbb18324440cf360
hardware-tested code boundary = 515bb4b0122c55348251eee726110ec946a4aeb6
status = GAMEPLAY HUB TOUCH UI + HAND MENU BUTTON PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `515bb4b0...` as new hardware-tested code. The tail
must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `34091004475` / run #177 passed on the
exact tested SHA and produced the firmware artifact. CI is compile/link evidence
only; real-CYD serial logs remain authoritative.

Latest milestone:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)

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

The latest UI frontier is now also hardware-owned:

```text
EspNativeGameplayHubView = 28 B
EspNativeGameplayPlayerState = 52 B
Inventory + Status remain read-only
visible INV / STATUS tabs
visible Inventory touch cards
HUB touch ownership isolated from world touch grid
visible MENU close uses original p.bmp hand
MENU underlay = 32x20 RGB565 = 1280 B
all other HUD pixels fingerprint-protected
MENU close restores underlay and full HUD exactly
world compositor restores exact pre-HUB frame
```

Milestone index for the latest frontier:

- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)

## Native gameplay HUB touch UI result

The HUB is deliberately not a permanent adoption of legacy `MenuSystem`.
Desktop/J2ME remains a behavior/content reference only. The ESP32 owns a compact
native modal UI over the canonical 52 B PlayerState.

World and HUB now have distinct input domains:

```text
WORLD -> invisible gameplay control zones
HUB   -> visible native controls
```

Real-CYD witness on `515bb4b0...`:

```text
Inventory frame = 79bbc30d
Status frame = eea0759d
Inventory again = 79bbc30d
hudProtected = bd7588ae preserved=yes
menuButton = hand asset=p.bmp frame=0
menuZone = 109b46aa
playerFNV = e745fce9 exact=yes
packClosed=yes
mutation=no
turn=no
```

A PASS_TURN touch inside the HUB was absorbed with
`worldDispatch=blocked` / `turnAdvance=no`, confirming that unsupported HUB input
cannot leak into gameplay.

Closing through the visible hand produced:

```text
menuUnderlayRestore=exact
hudBands=6c2aa46f expected=6c2aa46f exact=yes
playerFNV=e745fce9->e745fce9 exact=yes
packClosed=yes
```

The resident gameplay compositor then rebuilt the exact pre-HUB frame
`22397b55`.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)

## RAM at current boundary

The real-CYD session remained flat before, during and after HUB use:

```text
heap=87988
heap8=22256
largest8=14324
hub owner=28 B
player owner=52 B
MENU underlay=1280 B
full framebuffer snapshot=0 B
```

The previous HUB-v2 hardware witness was `89280 / 23548 / 20468`; the new bounded
MENU underlay reduced free heap by 1292 B and the largest 8-bit block is now
14324 B. Treat the current values as canonical before future RAM-heavy work.

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
HUB real weapon/ammo/item labels and richer content layout
HUB weapon selection
HUB consumable confirmation/use + turn consumption
HUB Notebook / Automap / Save / Load / Options / store
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
   `MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`;
3. create the next `agent/*` from that exact SHA;
4. recover the next bounded legacy/UI family against the true source before coding.

Strong next candidate: keep the proven visible touch shell and improve **content
readability** with real Doom RPG weapon/ammo/item names through bounded native
catalog lookups. Keep the HUB read-only for that milestone; weapon mutation,
consumable use and turn consumption remain separate semantic families.

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
