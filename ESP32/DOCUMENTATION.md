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
main at branch creation = 98256de72f2f0d4640b7533122492b8ff1535c8b
branch = agent/esp32-native-monster-postmove-attack
base main = 98256de72f2f0d4640b7533122492b8ff1535c8b
hardware-tested code boundary = f017aff03f93dce7dd66cac91136cc01ad9fe20c
status = one-step monster post-move same-turn attack PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `f017aff0...` as new hardware-tested code. The tail
must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33959809286` / run #119 passed on the
exact tested SHA. Job `101289595476` built the classic CYD firmware and uploaded
artifacts successfully.

CI is compile/link evidence only; real-CYD serial logs remain authoritative.

Latest milestone:

- [`MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md`](MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md)

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
one-step subtype 1/5 post-move goal
same-turn post-move monster attack after committed adjacent clear-trace move
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

## Monster attack visual result

Legacy contract hardware-proven for `NUMSHOTS == 1`:

```text
alternateAttack == 0 -> visual frame 1
alternateAttack != 0 -> visual frame 5
visible lease = 150 ms after successful physical present
expiry = exact idle redraw
```

Real-CYD witnesses on exact SHA `a1df0a5a...` covered primary Hellhound frame 1,
alternate Hellhound frame 5 and Zombie frame 1. The user visually confirmed the
attack pose. No attack-visual rollback or expiry retry appeared in the supplied
positive traces.

## One-step post-move same-turn attack result

The exact legacy split recovered from `Entity_aiMoveToGoal()` is:

```text
subtype 1 / 5  -> i = 1
subtype 4 / 13 -> i = 3
```

The earlier recovery note that treated all four as one-step was incorrect. The
current code intentionally owns only subtype `1/5`; subtype `4/13` remains
fail-closed until all three possible same-turn movement goals are owned.

Permanent composition:

```text
MonsterTurn produces movement opportunity
 -> native movement planner probe
 -> live position/topology commit + renderer projection
 -> EspNativeGameplayMonsterTurn_postMoveGoal()
 -> exact committed-destination adjacency + trace gate
 -> new attack probe, no new scheduled turn
 -> existing MonsterActivation gate
 -> existing MonsterAttackVisual
 -> existing MonsterRetaliation live RNG/player commit
```

Real-CYD Hellhound witness on exact SHA `f017aff0...`:

```text
first move:
[MONSTERMOVELIVE] COMMIT ... sprite=179 tile=750->718 ... rollback=closed
[MONSTERPOSTMOVE] COMPLETE ... distance2=16384 adjacentCardinal=no attack=no

second move:
[MONSTERMOVELIVE] COMMIT ... sprite=179 tile=718->686 ... rollback=closed
[MONSTERPOSTMOVE] ATTACK-PROBE reason=PASS_TURN sprite=179 subtype=1
    weapon=12 alt=0 loops=1 goalStep=1/1 distance2=4096
    adjacentCardinal=yes trace=clear
    playerHP=30->27 armor=8->5
    playerFNV=f58f97ce->f58f97ce
    rng=f71b27b7->f71b27b7
    rngRollback=yes playerExact=yes sameTurn=yes
    movementAlreadyCommitted=yes gameplayMutation=no
[MONSTERACT] DELIVER ... sprite=179 activated=yes
[MONSTERATKVIS] ARM ... sprite=179 visual=1 ...
[MONSTERRETAL] COMMIT ... playerHP=30->27 armor=8->5 ... rollback=closed
[MONSTERATKVIS] EXPIRE ... sprite=179 visual=1->idle ...
```

There is no additional `[MONSTERTURN] SCHEDULE` between the second movement commit
and the post-move attack probe. This is the key same-turn sequencing witness. The
first move is the matching negative witness proving that the hook does not attack
before the exact adjacent destination is reached.

The post-move attack probe restores PlayerState and `Random_t` exactly before
publishing the attack probe; the existing retaliation owner remains the only live
combat RNG/player-damage commit path. The new hook is not invoked for the ranged
`RANGED-AI` movement branch.

Detailed record:

- [`MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md`](MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md)

## RAM at current boundary

The real-CYD session remained stable through movement, post-move attack, attack
visual, retaliation, feedback expiry and later player combat:

```text
heap=86524
heap8=20792
largest8=18420
shapeData=NULL
mediaTexels=NULL
```

Audio remains deferred and requires its own RAM milestone.

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
subtype 4/13 three-goal same-turn movement chain
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
   `MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md`;
3. create the next `agent/*` from that exact SHA;
4. recover the next bounded legacy family against the true source before coding.

A strong candidate is subtype `4/13`'s exact `i=3` same-turn goal chain. It must
own the complete movement/RNG sequence and remain separate from presentation-only
movement interpolation and multiple-monster ordering.

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
