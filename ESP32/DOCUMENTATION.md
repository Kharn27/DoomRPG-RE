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
main at branch creation = dbcbf7b52f9aef851503a919a5a2871743a7de20
branch = agent/esp32-native-gameplay-hub-inventory-view
base main = dbcbf7b52f9aef851503a919a5a2871743a7de20
hardware-tested code boundary = bc136c735c9f5ba173a57fcbbb1f5bea6732b3bd
status = GAMEPLAY HUB V2 INVENTORY + STATUS READ-ONLY PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `bc136c73...` as new hardware-tested code. The tail
must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `34060186783` / run #161 passed on the
exact tested SHA and produced the firmware artifact. CI is compile/link evidence
only; real-CYD serial logs remain authoritative.

Latest milestone:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)

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
The current firmware still has transitional `/DoomRPG.zip` startup/reference
debt; removal remains part of the native migration.

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

The raw slot is a single-world backing store, but selection is generic. Entrance
is only a hardware witness. Active gameplay never silently falls back to SD.

Detailed storage milestones:

- [`MILESTONE_NATIVE_MAP_FLASH_BACKING.md`](MILESTONE_NATIVE_MAP_FLASH_BACKING.md)
- [`MILESTONE_NATIVE_MAP_FLASH_REUSE.md`](MILESTONE_NATIVE_MAP_FLASH_REUSE.md)

Classic CYD raw partition:

```text
nvs       0x009000  0x005000
otadata   0x00e000  0x002000
app0      0x010000  0x140000 = 1310720 B
spiffs    0x150000  0x2A0000 = 2752512 B raw slot
coredump  0x3F0000  0x010000
```

`spiffs` is intentionally not mounted as SPIFFS; it is raw storage managed via
`esp_partition_*`.

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

Hardware-owned behavior includes:

```text
movement / turn / strafe + native collision
compact sprite topology
SELECT event-first routing
SHOW / HIDE / UNLOCK
OPENLINE / CLOSELINE + regular door animation
DIALOG / DIALOGNOBACK
FORCEMESSAGE / NOTE
state ops 11 / 19 / 20
mutable line texture variants
native player weapon idle + attack presentation
jammed-door subtype-3 destruction + traversal
compact MonsterState initialization
generic type-1 player attack combat
pain / corpse / overkill-gib presentation
shared 52 B PlayerState + XP/progression/resources/HUD
consumed pickup world removal
extinguisher ammo consumption + fire removal
stationary MonsterTurn scheduling + LOS
live nonlethal monster retaliation
native PASS_TURN feedback
PASS_TURN current-tile linked type10/type11 hazards
transaction-safe PASS_TURN hazard rollback
reentrant red/white viewport flash snapshot ownership
transaction-safe gameplay RNG replay
compact MonsterPosition
legacy-compatible bounded monster movement planner
live movement + topology relink + renderer projection
NEXT / PREV weapon cycling
live Pistol ammo consumption
pickup messages + white viewport flash
movement hazards + red viewport flash
monster retaliation text + red viewport flash
safe feedback expiry during native dialogs
raw-flash gameplay backing + generic requested-map reuse
single-loop monster attack visuals: frame 1 primary + frame 5 alternate
one-step subtype 1/5 post-move goal
same-turn post-move monster attack after committed adjacent clear-trace move
renderer-visible enemy activation persisted in first-activation order
multiple active no-immediate-attack monsters delivered in one MonsterTurn
per-member movement publication before the next active member is planned
dead active members skipped by later delivery
open-door attack passthrough preserved with regular four-frame line animation
28 B native gameplay HUB owner with no framebuffer snapshot
read-only Inventory + Status projection from the shared 52 B PlayerState
HUB writes only world viewport y=20..99 and fingerprints untouched HUD bands
TURN_LEFT/RIGHT page navigation with all HUB input isolated from world dispatch
Inventory cursor via FORWARD/BACK; Status movement input absorbed
exact world-frame reconstruction on MENU close
```

Milestone index for this frontier:

- [`MILESTONE_NATIVE_JAMMED_DOOR.md`](MILESTONE_NATIVE_JAMMED_DOOR.md)
- [`MILESTONE_NATIVE_MONSTER_COMBAT.md`](MILESTONE_NATIVE_MONSTER_COMBAT.md)
- [`MILESTONE_NATIVE_PLAYER_RESOURCES.md`](MILESTONE_NATIVE_PLAYER_RESOURCES.md)
- [`MILESTONE_NATIVE_MONSTER_TURN.md`](MILESTONE_NATIVE_MONSTER_TURN.md)
- [`MILESTONE_NATIVE_PASS_TURN.md`](MILESTONE_NATIVE_PASS_TURN.md)
- [`MILESTONE_NATIVE_WEAPON_CONTROL.md`](MILESTONE_NATIVE_WEAPON_CONTROL.md)
- [`MILESTONE_NATIVE_MONSTER_MOVEMENT.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT.md)
- [`MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md)
- [`MILESTONE_NATIVE_PICKUP_FEEDBACK.md`](MILESTONE_NATIVE_PICKUP_FEEDBACK.md)
- [`MILESTONE_NATIVE_HAZARD_TOUCH.md`](MILESTONE_NATIVE_HAZARD_TOUCH.md)
- [`MILESTONE_NATIVE_MONSTER_PAIN_FEEDBACK.md`](MILESTONE_NATIVE_MONSTER_PAIN_FEEDBACK.md)
- [`MILESTONE_NATIVE_MAP_FLASH_BACKING.md`](MILESTONE_NATIVE_MAP_FLASH_BACKING.md)
- [`MILESTONE_NATIVE_MAP_FLASH_REUSE.md`](MILESTONE_NATIVE_MAP_FLASH_REUSE.md)
- [`MILESTONE_NATIVE_MONSTER_ATTACK_VISUAL.md`](MILESTONE_NATIVE_MONSTER_ATTACK_VISUAL.md)
- [`MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md`](MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md)
- [`MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md`](MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md)
- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)

## Native gameplay HUB v2 result

The HUB is deliberately a compact native shell, not permanent adoption of legacy
`MenuSystem`. The tested ownership is:

```text
EspNativeGameplayPlayerState = 52 B canonical mutable player state
EspNativeGameplayHubView = 28 B modal UI state
HUB framebuffer backup = 0 B
pages = Inventory, Status
```

Inventory exposes current weapon/owned bitmask, six ammo counters, five inventory
slots, keys, credits, level and XP. Status mirrors the useful player portion of
legacy `Menu_fillStatus()` using the already-owned compact player values:
health/max, armor/max, level/current XP/next XP, defense, strength, agility,
accuracy, credits and keys.

The first prototype revealed an important compositor boundary. Gameplay rendering
preserves the native HUD bands at `y=0..19` and `y=100..119`; therefore a HUB that
painted all 120 rows left menu traces after close. The permanent solution is
viewport-only composition:

```text
HUB writable region = 160x80 / y=20..99
HUD bands = untouched and FNV-verified before/after every HUB paint
world close = normal resident gameplay rerender
```

Real-CYD witness on `bc136c73...`:

```text
playerFNV=e745fce9 exact=yes
hudBands=6c2aa46f preserved=yes
Inventory row0 frame=06138e61
Status frame=b35c12b9
Inventory row2 frame=71bffc61
Inventory row1 frame=1a817e61
close world frame=22397b55 (same as pre-HUB baseline)
packClosed=yes
mutation=no
turn=no
```

`FORWARD` on Status was absorbed rather than falling into player movement. Page
switches and cursor movement were deterministic, and MENU close restored the
world with no HUD residue.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)

## Retained active-monster sequencing result

Legacy sprite rendering activates a monster when an admitted sprite reaches the
sprite-object renderer. The native equivalent observes activation at the
BSP-visible admission point and records first-activation order without consuming
RNG.

For the no-immediate-attack family, the active sequence closes one full live
movement transaction per member before the next member is planned:

```text
plan -> RNG commit -> MonsterPosition -> topology relink -> renderer publication
```

The real-CYD witness used subtype-1 Hellhounds `89` and `114`. The first dog
committed its move, then the second dog planned and committed from the updated
world state. After dog 89 was killed by the existing generic combat backend,
subsequent delivery contained only dog 114.

The same earlier hardware session verified that composition wrappers did not
break regular doors or enemy SELECT passthrough: the door still emitted all four
`DOORANIM` frames and enemy combat remained reachable through the opened line.

Detailed record:

- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)

## Subtype 4/13 three-goal owner retained

The bounded owner for the exact legacy `i=3` movement family remains present:

```text
subtype 4 / 13 -> three same-turn movement goals max
first goal -> existing movement publisher
continuation goals -> player settled destination
successful continuation -> one visit-choice RNG byte
unsupported special calcPath plane -> fail closed
three-shot / multi-loop attack family -> fail closed
```

The HUB hardware log does not itself create a new subtype-4/13 witness. Keep that
distinction when recovering the next branch.

## RAM at current boundary

The supplied real-CYD HUB session remained stable through repeated page switches,
ignored Status movement input, Inventory cursor movement and return to gameplay:

```text
heap=89280
heap8=23548
largest8=20468
hub owner=28 B
player owner=52 B
HUB framebuffer snapshot=0 B
shapeData=NULL
mediaTexels=NULL
```

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
HUB visual chrome + entity/item names
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

Storage can generically prepare target map 9, but transition teardown/load/spawn
and state transfer remain their own production milestone.

## After this merge

Do not continue code on this locked branch.

When the user announces the merge:

1. read the true GitHub `main` and exact SHA;
2. re-read `PORTING_STATUS.md`, this file and
   `MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`;
3. create the next `agent/*` from that exact SHA;
4. recover the next bounded legacy/UI family against the true source before coding.

Strong next candidate: a **visual/readability HUB pass** only. The useful legacy
in-game look is already recovered: black viewport background, dark-blue `0x050A4A`
8 px grid, white text, compact 12 px rows and a hand/selection affordance. The
next branch can reproduce that character with bounded native drawing and add
real Doom RPG entity/item labels, while keeping the 28 B HUB owner,
`y=20..99` clipping and read-only semantics. Item-use mutation must remain a
separate milestone.

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
