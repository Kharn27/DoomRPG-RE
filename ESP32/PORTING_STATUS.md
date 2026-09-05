# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main at branch creation = 19c3fd3b6ebd66530e92d24a43ec8f09a9c4f803
branch = agent/esp32-native-monster-attack-visual
base main = 19c3fd3b6ebd66530e92d24a43ec8f09a9c4f803
hardware-tested code boundary = a1df0a5ac031b4a0dad6f4da609dd25c6c450007
status = REAL-CYD SINGLE-LOOP MONSTER ATTACK VISUAL FRAME 1 + FRAME 5 PASS
branch policy = LOCKED; docs-only tail only
```

`a1df0a5a...` is the exact code boundary exercised on the real classic CYD.
Commits after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33958515531` / run #110 completed
successfully on this exact SHA. Job `101286108924` (`PlatformIO esp32-cyd`)
built the classic CYD firmware and uploaded artifacts successfully.

CI is compile/link evidence only. Hardware serial logs remain authoritative.
After merge, read the real GitHub `main` SHA again before creating the next
`agent/*` branch.

## Permanent architecture and hard invariants

```text
A NEW BSP IS NOT A NEW ENGINE.
A NEW MONSTER IS NOT A NEW COMBAT BACKEND.
A NEW PICKUP MUST NOT BECOME A NEW MINI-OWNER.
```

Target production path:

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
/DoomRPG-ESP32.pak on microSD (authoritative source)
 -> generic requested-map raw internal-flash slot
 -> 19 KiB resident RAM cache (L1)
 -> native gameplay / renderer
```

Contract:

```text
- one requested-map working set is staged/verified before gameplay arms;
- existing slot reuse is keyed to the map actually requested;
- active gameplay never silently falls back to SD;
- original PAK offsets/index semantics are retained;
- excluded non-current BSP ranges fail closed;
- slot header is committed only after flash readback verification;
- reuse revalidates source identity + layout + flash FNVs;
- shapeData == NULL and mediaTexels == NULL remain mandatory.
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
[MAPFLASH] READY ... buildUs=8442586
```

Generic requested-map reuse witness:

```text
[MAPFLASH] REUSE HIT requestedMap=1 current=/intro.bsp cachedMap=1
           sourceIndexFNV=3a51cc4d payloadFNV=9ec04e22
           verifyUs=361875 rebuild=no
[MAPFLASH] ARM map=1 active=1 verified=1 reused=1 staged=2248743
           metadata=12288 prepareUs=363258 buildUs=0 resident=1
```

The verified reuse path is about 23.2x faster than the full rebuild witness.
The user reports that walking/testing through the map is now materially smoother
and free of the former large storage stalls.

Detailed records:

- [`MILESTONE_NATIVE_MAP_FLASH_BACKING.md`](MILESTONE_NATIVE_MAP_FLASH_BACKING.md)
- [`MILESTONE_NATIVE_MAP_FLASH_REUSE.md`](MILESTONE_NATIVE_MAP_FLASH_REUSE.md)

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

The 288-record recycle policy still exists, but internal-flash refill prevents it
from becoming the former SD seek cliff. `PlatformVideo_present()` remains around
34.4 ms and is not the current optimization target.

## Current hardware-owned gameplay frontier

Validated behavior includes:

```text
TURN_LEFT / TURN_RIGHT
FORWARD / BACK / STRAFE
native collision/topology
SELECT event-first routing
EV_SHOW / EV_HIDE / EV_UNLOCK
EV_OPENLINE / EV_CLOSELINE
EV_DIALOG / EV_DIALOGNOBACK
EV_FORCEMESSAGE / EV_NOTE
state ops 11 / 19 / 20
regular door open/close animation
mutable line texture variants
native idle weapon rendering + generic player attack pose
move-event state mutation with rollback/commit
jammed-door subtype-3 destruction and traversal
generic compact MonsterState + type-1 player attack combat
generic PlayerState resources / consumed pickup removal
shared 52 B PlayerState + HUD projection
extinguisher ammo consumption + fire removal
pain / corpse / gib presentation
bounded stationary monster retaliation
native PASS_TURN + exact top-bar feedback
compact mutable MonsterPosition owner
legacy-compatible movement planner + RNG reservation/replay
live one-monster movement publication + topology relink
renderer projection of committed moved monster position
generic NEXT / PREV weapon cycling
selected-weapon HUD + first-person redraw without turn advance
live Pistol ammo consumption + generic combat commit
live pickup messages + white pickup flash
adaptive floor/ceiling texture cache under memory pressure
movement-side linked type10/type11 hazard touch into PlayerState
live bounded movement-hazard damage text + red viewport flash
live nonlethal monster-retaliation damage text + red viewport flash
feedback expiry safely deferred while native dialog owns PAK
raw internal-flash gameplay backing with no silent SD fallback
generic requested-map committed-slot reuse with strict rebuild on mismatch
single-loop monster attack visual: primary frame 1 + alternate frame 5
150 ms attack visual lease with guarded render + exact idle expiry
```

Relevant detailed records:

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

## Monster attack visual — hardware PASS

Exact legacy visual contract:

```text
alternateAttack == 0 -> attackFrame = 1
alternateAttack != 0 -> attackFrame = 5
Combat_monsterSeq attack-frame hold = 150 ms
```

Native owner `EspNativeGameplayMonsterAttackVisual` consumes the existing
MonsterTurn attack probe. It owns presentation only and never duplicates combat
math, player damage or RNG commit semantics.

Primary Hellhound witness:

```text
[MONSTERTURN] ATTACK-PROBE ... sprite=179 subtype=1 weapon=12 alt=0 loops=1 ...
[MONSTERATKVIS] ARM probe=1 ... sprite=179 subtype=1 alt=0 loops=1 visual=1 fixedAnim=yes leaseMs=150 ... rngExact=yes ... gameplayMutation=no
[MONSTERRETAL] COMMIT probe=1 ... rollback=closed ...
[MONSTERATKVIS] EXPIRE probe=1 sprite=179 visual=1->idle leaseMs=150 ... rngExact=yes gameplayMutation=no
```

Alternate Hellhound witness:

```text
[MONSTERTURN] ATTACK-PROBE ... sprite=114 subtype=1 weapon=13 alt=1 loops=1 ...
[MONSTERATKVIS] ARM probe=6 ... sprite=114 subtype=1 alt=1 loops=1 visual=5 fixedAnim=yes leaseMs=150 ... rngExact=yes ... gameplayMutation=no
[MONSTERRETAL] COMMIT probe=6 ... rollback=closed ...
[MONSTERATKVIS] EXPIRE probe=6 sprite=114 visual=5->idle leaseMs=150 ... rngExact=yes gameplayMutation=no
```

Zombie subtype 0 also exercised the primary frame-1 path. The user visually
confirmed that monsters now visibly attack.

No `[MONSTERATKVIS] ROLLBACK` or `[MONSTERATKVIS] EXPIRE-RETRY` appeared in the
provided positive witnesses.

## RAM witness at current boundary

Repeated walking, pickups, monster movement, attack visuals and retaliation on
the exact tested build remained stable at:

```text
heap = 86524
heap8 = 20792
largest8 = 18420
shapeData = NULL
mediaTexels = NULL
```

Audio remains deferred. Do not enable it without its own RAM milestone.

## Newly recovered next gameplay boundary

Real-CYD testing exposed a separate AI sequencing gap after movement.

Legacy `Entity_aiMoveToGoal()` is called again when monster interpolation reaches
its destination. For subtypes:

```text
1, 4, 5, 13
```

it uses a one-step post-move goal (`i = 1`) and, if the monster has become
cardinally adjacent (`distance^2 <= 4096`) with an unobstructed trace, calls
`Entity_attack()` in the same monster turn.

This explains the observed Hellhound difference: native movement currently snaps
and commits the destination but does not yet execute the legacy post-move goal,
so a dog can become adjacent and wait for the player's next turn before attacking.

Important negative witness: a ranged Zombie after a player shot produced
`aiRand=254 >= 217`, selected the legacy movement branch, moved successfully, and
did not attack that turn. This is compatible with `Entity_aiThink()` and means the
next implementation must **not** blindly attack after every movement.

Preferred next bounded milestone after merge:

```text
native monster post-move goal / same-turn attack
- recover only exact legacy subtype/adjacency/trace gate;
- trigger only after a committed live move;
- feed a new same-turn attack probe into existing activation/visual/retaliation;
- reuse existing damage/RNG owners;
- no movement interpolation milestone bundled in;
- no multiple-monster ordering expansion bundled in.
```

## Intentionally deferred families

```text
production SAVEGAME / CHANGEMAP transition ownership
pre-arm first-frame/HUD SD startup path
L1 range-record eviction/recycle redesign
PASS_TURN current-tile type10/type11 Entity_touched semantics
pickup sound playback / got-face presentation
movement-hazard secondary burn text / pain face / shake / sound
action XP migration
materialized monster drops
corpse-pile trimming
monster movement interpolation/animation
multiple-live-monster activation/movement ordering
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
familiar weapon slots
generic type-12 destructible combat
special death consequences
Kronos-specific semantics
password input
EV_GIVEMAP production route
EV_CHECK_KEY production route
```

These are mechanical family boundaries, never item-by-item or monster-by-monster
implementation ladders.

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
   `MILESTONE_NATIVE_MONSTER_ATTACK_VISUAL.md`;
3. create a fresh branch from that exact main SHA;
4. preferred next branch: native monster post-move goal / same-turn attack;
5. recover/reuse the exact legacy `Entity_aiMoveToGoal()` semantics only.

## Development workflow

```text
recover true main + docs
 -> choose one bounded behavior FAMILY
 -> recover exact legacy behavior
 -> design small permanent native owner/API
 -> keep genuinely different families fail-closed
 -> commit/push agent/*
 -> test normal esp32-cyd on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> verify post-test commits are docs-only
 -> merge-ready
```

Never merge into `main` without explicit user request.
