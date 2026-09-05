# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main at branch creation = 63cc8897e98c1ac6bf597234b35fde87cc2f7570
branch = agent/esp32-native-pass-turn-hazard-touch
base main = 63cc8897e98c1ac6bf597234b35fde87cc2f7570
hardware-tested code boundary = e76183adf8245b465d7c398abba88a6c6a86f627
status = REAL-CYD PASS_TURN CURRENT-TILE HAZARD + REENTRANT VIEWFLASH PASS
branch policy = LOCKED; docs-only tail only
```

`e76183ad...` is the exact code boundary exercised on the real classic CYD.
Commits after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33969446333` / run #132 completed
successfully on this exact SHA. Job `101315237040` (`PlatformIO esp32-cyd`)
built the classic CYD firmware and uploaded artifacts successfully.

CI is compile/link evidence only. Hardware serial logs remain authoritative.
After merge, read the real GitHub `main` SHA again before creating the next
`agent/*` branch.

Latest detailed record:

- [`MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md`](MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md)

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
The user reports that walking/testing through the map is materially smoother and
free of the former large storage stalls.

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
PASS_TURN current-tile linked type10/type11 hazard touch
hazard damage feedback supersedes one-slot "Turn passed." display
transactional hazard rollback if MonsterTurn request cannot arm
reentrant red/white viewport flash with preserved original snapshot
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
one-step post-move goal for subtype 1/5
same-turn post-move attack after committed adjacent clear-trace move
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
- [`MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md`](MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md)
- [`MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md`](MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md)

## PASS_TURN current-tile hazard + VIEWFLASH — hardware PASS

Recovered bounded ordering:

```text
Hud_addMessage("Turn passed.")
Game_touchTile(currentTile, false)
Game_advanceTurn()
```

The native touched=false subset processes linked type-10/type-11 hazards on the
settled current tile and intentionally ignores resources. The current one-slot top
bar cannot retain both messages, so hazard damage feedback supersedes the visual
`Turn passed.` string while preserving gameplay ordering.

Representative real-CYD type-10 witness on exact SHA `e76183ad...`:

```text
[HAZARDPASS] COMMIT tile=619 sprite=139 type=10 hazards=1
    rawDamage=1+2 hp=21->18 armor=0->0
    message="3 damage!" passMessage="Turn passed."-legacy-superseded
    flash=red-bb0000/500ms rollback=armed
[ACTIONFEEDBACK] PAINT kind=6 text="3 damage!" ...
[VIEWFLASH] REFRESH color565=b800 snapshot=preserved framebufferFresh=0
[VIEWFLASH] PAINT ... durationMs=500 ...
[PASSTURN] REQUEST ... tileTouch=hazard-committed type10/11=owned
    playerMutation=hazard-owned feedbackPresent=immediate
[MONSTERTURN] SCHEDULE ... reason=PASS_TURN ...
[VIEWFLASH] EXPIRE elapsedMs=504 targetMs=500 color565=b800
    restored=viewport-border-only
```

The user confirmed the red border physically disappears after the final lease,
including the rapid repeated-PASS_TURN case that previously corrupted the restore
snapshot. The old stuck-red behavior was a generic presentation bug: a second
flash snapshotted the already-red border. The fixed VIEWFLASH owner preserves the
original pre-flash snapshot on overlapping refreshes and only resets the lease.

Lethal hazard transition, familiar redirection, secondary burn text, pain face,
shake and sound remain fail-closed/deferred.

## One-step post-move same-turn attack — retained hardware PASS

The true legacy `Entity_aiMoveToGoal()` goal counts are:

```text
subtype 1 / 5  -> i = 1
subtype 4 / 13 -> i = 3
```

The native code intentionally owns the exact `i=1` family only. After a committed
live move, subtype `1/5` can feed a same-turn attack probe into the existing
activation, attack-visual and retaliation owners only from the exact committed
adjacent clear-trace destination.

The previous real-CYD Hellhound witness remains canonical in
[`MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md`](MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md).
Subtype `4/13` remains deferred until the complete three-goal sequence is owned.

## RAM witness at current boundary

The supplied real-CYD session remained stable through repeated hazard damage,
rapid PASS_TURN refreshes, flash expiry, door animation and later movement:

```text
heap = 86524
heap8 = 20792
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
subtype 4/13 three-goal same-turn movement chain
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
   `MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md`;
3. create a fresh `agent/*` from that exact main SHA;
4. recover the next bounded legacy family before coding.

A strong candidate remains the exact subtype `4/13` `i=3` same-turn goal chain,
but re-evaluate it against true `main` and the legacy source before coding. It
must own the complete required movement/RNG sequence rather than approximating
three goals after one published step.

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
