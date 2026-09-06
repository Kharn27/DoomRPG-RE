# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main at branch creation = e6aae5d3a0d3c6564a3f4147b3be71c422dda5d3
branch = agent/esp32-native-monster-three-goal-turn
base main = e6aae5d3a0d3c6564a3f4147b3be71c422dda5d3
hardware-tested code boundary = 8c3bee5ebba013fead9273f10f1a1c3bd015eeda
status = REAL-CYD MULTI-ACTIVE MONSTER SEQUENCE + DOOR/ATTACK REGRESSION PASS
branch policy = LOCKED; docs-only tail only
```

`8c3bee5e...` is the exact code boundary exercised on the real classic CYD.
Commits after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `34047884638` / run #149 completed
successfully on this exact SHA and produced the firmware artifact. CI is
compile/link evidence only; hardware serial logs remain authoritative.

Latest detailed record:

- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)

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

Generic requested-map reuse witness:

```text
[MAPFLASH] REUSE HIT requestedMap=1 current=/intro.bsp cachedMap=1
           sourceIndexFNV=3a51cc4d payloadFNV=9ec04e22
           verifyUs=361875 rebuild=no
```

Active gameplay never silently falls back to SD. Slot reuse remains keyed to the
requested map and revalidates source identity, layout and flash fingerprints.

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
PASS_TURN current-tile linked type10/type11 hazard touch
reentrant red/white viewport flash
compact mutable MonsterPosition owner
legacy-compatible movement planner + RNG reservation/replay
live monster movement publication + topology relink
renderer projection of committed moved monster position
generic NEXT / PREV weapon cycling
live Pistol ammo consumption + generic combat commit
live pickup messages + white pickup flash
adaptive floor/ceiling texture cache under memory pressure
movement-side linked type10/type11 hazard touch
live bounded movement-hazard damage text + red viewport flash
live nonlethal monster-retaliation damage text + red viewport flash
feedback expiry safely deferred while native dialog owns PAK
raw internal-flash gameplay backing with no silent SD fallback
generic requested-map committed-slot reuse with strict rebuild on mismatch
single-loop monster attack visual: primary frame 1 + alternate frame 5
150 ms attack visual lease with guarded render + exact idle expiry
one-step post-move goal for subtype 1/5
same-turn post-move attack after committed adjacent clear-trace move
renderer-visible monster activation persisted in first-activation order
multiple active no-immediate-attack monsters sequenced in one MonsterTurn
per-member movement publication before planning the next active monster
dead active-list members skipped by later delivery
open-door SELECT attack passthrough preserved while 4-frame slide animation remains live
```

Detailed records include:

- [`MILESTONE_NATIVE_MONSTER_MOVEMENT.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT.md)
- [`MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md)
- [`MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md`](MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md)
- [`MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md`](MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md)
- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)

## Latest real-CYD active sequence witness

Two subtype-1 Hellhounds, sprites `89` and `114`, were activated by BSP-render
visibility. Activation only changed the map-session activation bit/order and did
not consume gameplay RNG.

During one PASS_TURN the hardware log showed:

```text
sprite 89  tile 263 -> 264  rngCalls=1  randomCommitted=yes
 -> publication=committed-before-next
sprite 114 tile 233 -> 265  rngCalls=1  randomCommitted=yes
 -> publication=committed-before-next
[MONSTERACTIVESEQ] COMPLETE ... delivered=2 ... ordered=yes publication=per-member
```

The next turn repeated the ordered publication from the updated positions.
After the player killed sprite 89 (`hp=6->0`, `alive=1->0`), the same player
attack turn and later PASS_TURNs delivered movement only for sprite 114. This
confirms that the active sequence observes current mutable MonsterState rather
than replaying a stale activation snapshot.

The same final hardware session also confirmed the regression fix on regular
doors: `DOORANIM` ran all four moving/stable frames and SELECT then reached enemy
combat through the open door.

## Subtype 4/13 three-goal owner on this branch

The branch contains the bounded legacy `Entity_aiMoveToGoal()` `i=3` owner for
subtypes `4/13`:

```text
first goal = existing committed movement
continuation goals 2/3 = settled player destination
successful continuation RNG = one visit-choice byte per goal
publication = existing live movement transaction
unsupported special calcPath plane crossing = fail closed
multi-loop / three-shot attack family = fail closed
```

The final regression session represented in the latest milestone exercises the
subtype-1 multi-active sequence, door animation and combat passthrough. It does
not by itself add a new subtype-4/13 hardware witness; do not infer one merely
because the binary contains that owner.

## RAM witness at current boundary

The supplied real-CYD session remained stable through repeated movement,
door animation, combat, death publication and later PASS_TURNs:

```text
heap = 86252
heap8 = 20520
largest8 = 18420
shapeData = NULL
mediaTexels = NULL
```

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
   `MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`;
3. create a fresh `agent/*` from that exact main SHA;
4. recover the next bounded legacy family before coding.

The next milestone should be chosen against the true merged code and legacy
source. Strong candidates are the remaining simultaneous attack-ready active-list
ordering or the separate multi-loop monster attack family, but neither should be
started on this locked branch.

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
