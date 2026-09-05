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
main at branch creation = 19c3fd3b6ebd66530e92d24a43ec8f09a9c4f803
branch = agent/esp32-native-monster-attack-visual
base main = 19c3fd3b6ebd66530e92d24a43ec8f09a9c4f803
hardware-tested code boundary = a1df0a5ac031b4a0dad6f4da609dd25c6c450007
status = single-loop monster attack visual frame 1 + frame 5 PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `a1df0a5a...` as new hardware-tested code. The tail
must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33958515531` / run #110 passed on the
exact tested SHA. Job `101286108924` built the classic CYD firmware and uploaded
artifacts successfully.

CI is compile/link evidence only; real-CYD serial logs remain authoritative.

Latest milestone:

- [`MILESTONE_NATIVE_MONSTER_ATTACK_VISUAL.md`](MILESTONE_NATIVE_MONSTER_ATTACK_VISUAL.md)

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
full rebuild ~= 8.44 s
validated reuse ~= 0.363 s
```

The user reports the map is now materially smoother to walk and test, without the
former large SD-backed stalls.

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

The global range-record recycle still exists, but raw internal-flash refill has
removed the former SD seek cliff. `PlatformVideo_present()` remains around
34.4 ms and is not the current target.

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
jammed-door subtype-3 Axe destruction + traversal
compact MonsterState initialization
generic type-1 player attack combat
pain / corpse / overkill-gib presentation
shared 52 B PlayerState + XP/progression/resources/HUD
consumed pickup world removal
extinguisher ammo consumption + fire removal
stationary MonsterTurn scheduling + LOS
live nonlethal monster retaliation
native PASS_TURN feedback
transaction-safe gameplay RNG replay
compact MonsterPosition
legacy-compatible bounded monster movement planner
live one-monster movement + topology relink + renderer projection
NEXT / PREV weapon cycling
live Pistol ammo consumption
pickup messages + white viewport flash
movement hazards + red viewport flash
monster retaliation text + red viewport flash
safe feedback expiry during native dialogs
raw-flash gameplay backing + generic requested-map reuse
single-loop monster attack visuals: frame 1 primary + frame 5 alternate
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

## Monster attack visual result

Legacy contract recovered and now hardware-proven for `NUMSHOTS == 1`:

```text
alternateAttack == 0 -> visual frame 1
alternateAttack != 0 -> visual frame 5
visible lease = 150 ms after successful physical present
expiry = exact idle redraw
```

Real-CYD witnesses on exact SHA `a1df0a5a...`:

```text
Hellhound sprite=179 subtype=1 weapon=12 alt=0 loops=1
 -> MONSTERATKVIS ARM visual=1
 -> MONSTERRETAL COMMIT
 -> MONSTERATKVIS EXPIRE visual=1->idle

Hellhound sprite=114 subtype=1 weapon=13 alt=1 loops=1
 -> MONSTERATKVIS ARM visual=5
 -> MONSTERRETAL COMMIT
 -> MONSTERATKVIS EXPIRE visual=5->idle

Zombie sprite=106 subtype=0 weapon=2 alt=0 loops=1
 -> frame-1 path also exercised
```

The user visually confirmed the monster attack pose. No attack-visual rollback or
expiry retry appeared in the supplied positive traces.

## RAM at current boundary

The long real-CYD session remained stable at:

```text
heap=86524
heap8=20792
largest8=18420
shapeData=NULL
mediaTexels=NULL
```

Audio remains deferred and requires its own RAM milestone.

## Next recovered gameplay boundary

The latest real-CYD test exposed a separate, precise AI sequencing gap.

Legacy `Entity_aiMoveToGoal()` is called again when interpolation reaches the
monster's committed destination. For subtypes `1`, `4`, `5`, `13`, its post-move
step can immediately call `Entity_attack()` when the monster is cardinally
adjacent (`distance^2 <= 4096`) with a clear trace.

The current native movement path commits/snap-projects the destination but stops
there. A Hellhound can therefore move adjacent and wait for another player turn
before attacking.

A ranged Zombie negative witness is equally important: after the player shot it,
`aiRand=254 >= 217` selected the legacy movement branch; the Zombie moved and did
not attack that same turn. That behavior is compatible with legacy
`Entity_aiThink()`. The next implementation must not turn every successful move
into an attack.

Preferred next milestone after merge:

```text
native post-move goal / same-turn monster attack
 -> exact legacy subtype gate 1/4/5/13
 -> committed destination adjacency/cardinal test
 -> exact trace gate
 -> new same-turn attack probe
 -> existing attack visual + retaliation owners consume it
 -> no duplicated combat math
 -> movement interpolation itself remains separate
 -> multiple-monster ordering remains separate
```

## Current intentionally deferred families

```text
production SAVEGAME / CHANGEMAP ownership
pre-arm first-frame/HUD SD startup path
L1 range-record eviction/recycle redesign
PASS_TURN current-tile hazard touch
pickup sound / got-face
secondary movement-hazard burn/pain/shake/sound
action XP migration
materialized monster drops
corpse-pile trimming
monster movement interpolation animation
multiple-live-monster ordering
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
familiar weapon slots
generic type-12 destructible combat
special death consequences
Kronos-specific semantics
password input
GIVEMAP production route
CHECK_KEY production route
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
   `MILESTONE_NATIVE_MONSTER_ATTACK_VISUAL.md`;
3. create the next `agent/*` from that exact SHA;
4. preferred next milestone: post-move goal / same-turn monster attack.

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
