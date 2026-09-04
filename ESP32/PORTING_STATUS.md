# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = a6a3334c2235f14a69b8c8c9acd9b1c3a0485c01
branch = agent/esp32-native-flash-pak-backing
base main = a6a3334c2235f14a69b8c8c9acd9b1c3a0485c01
hardware-tested code boundary = 3fcd255015315e67222de7bcedf63f76220c7820
status = REAL-CYD CURRENT-MAP RAW-FLASH PAK BACKING PASS
branch policy = LOCKED; docs-only tail only
```

`3fcd2550...` is the exact code boundary exercised on the real classic CYD.
Commits after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33862354211` completed successfully on
this exact SHA and uploaded:

```text
doom-rpg-esp32-cyd-3fcd255015315e67222de7bcedf63f76220c7820
artifact id = 9932634573
```

CI is only a compile/link gate. Hardware serial logs remain authoritative.

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
/DoomRPG-ESP32.pak on SD as authoritative source
 -> native parsers/catalog
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

## Native PAK backing hierarchy — hardware PASS

The active gameplay storage path is now:

```text
/DoomRPG-ESP32.pak on microSD
        |
        | load/stage only
        v
raw internal-flash current-map slot
        |
        | active gameplay backing
        v
19 KiB resident RAM cache (L1)
        |
        v
native renderer/gameplay
```

Contract:

```text
- SD remains authoritative source storage.
- One complete current-map gameplay working set is staged before gameplay arms.
- Active gameplay does not silently fall back to SD.
- Original PAK index/offset semantics are retained.
- Access to intentionally excluded other BSPs fails closed.
- Flash metadata/header is committed only after copy + readback verification.
- The existing 19 KiB resident RAM cache remains the L1 policy.
- shapeData == NULL and mediaTexels == NULL remain mandatory.
```

Detailed record:

- [`MILESTONE_NATIVE_MAP_FLASH_BACKING.md`](MILESTONE_NATIVE_MAP_FLASH_BACKING.md)

## Classic CYD flash layout

`esp32-cyd` now uses `ESP32/partitions_cyd_raw_pak.csv` instead of the Arduino
`no_ota.csv` layout:

```text
nvs       0x009000  size 0x005000
otadata   0x00e000  size 0x002000
app0      0x010000  size 0x140000  = 1310720 B
spiffs    0x150000  size 0x2A0000  = 2752512 B
coredump  0x3F0000  size 0x010000
```

The partition named `spiffs` is intentionally **not mounted as a filesystem** in
this milestone. `esp_asset_pack.cpp` owns it as raw flash via `esp_partition_*`.

The tested `firmware.bin` size was 644144 B, leaving 666576 B in the application
partition at this boundary.

A hardware flash of this milestone must include the generated `partitions.bin`.
Flashing only `firmware.bin` while retaining the old table reproduces the old
capacity failure by design.

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

## Current-map flash staging witness

Real-CYD Entrance plan:

```text
pack = 2457398 B
PAK entries = 241
PAK index = 4820 B
metadata = 12288 B
excluded non-current BSPs = 12
excluded BSP bytes = 203811 B
staged payload = 2248743 B
raw partition = 2752512 B
headroom = 491481 B
fits = yes
```

Real-CYD build/verification:

```text
[MAPFLASH] ERASE bytes=2752512 buffer=4096 owner=transient
[MAPFLASH] COPY indexFNV=3a51cc4d payloadFNV=9ec04e22 verified=yes
[MAPFLASH] READY map=1 staged=2248743 metadata=12288 excluded=12/203811
           buildUs=8442586 backing=raw-internal-flash SDGameplayReads=forbidden
[MAPFLASH] ARM map=1 active=1 verified=1 resident=1
```

The approximately 8.44 s staging time is a real load-time cost. It is the
intended tradeoff for removing microSD seek latency from active gameplay.

## Resident RAM-cache baseline retained

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

The 288-record global recycle policy still exists. This milestone deliberately
did not rewrite it.

The important hardware result is that a recycle no longer creates an SD-latency
cliff because misses refill from internal flash.

## Performance result — hardware PASS

The preceding SD measurement established the problem:

```text
common SD physical read ~= 9 ms
miss-heavy 64-call PAK batches = hundreds of ms
VIDEO present = stable ~34.4 ms
fire-room range recycle:
  entries 288/288 -> 23/288
  SPRITEPROFILE ~36.7 ms -> ~241.8 ms
  frame ~335 ms -> ~736 ms
```

The raw-flash hardware run showed:

```text
PAKIO backing = raw-flash
common hit-heavy 64-call batches = ~0.3-1.2 ms
miss-heavy raw-flash batches = commonly ~4-10 ms
raw-flash individual physical-read maxima = commonly below ~0.6 ms
VIDEO present = still ~34.4 ms
```

Startup cache timings:

```text
SMALL-COLD = 228233 us
SMALL-WARM = 178176 us
LARGE-LEARN = 178270 us
LARGE-WARM = 177531 us
```

Earlier SD-backed measurement for comparison:

```text
SMALL-COLD = 2100916 us
SMALL-WARM = 324151 us
LARGE-LEARN = 315502 us
LARGE-WARM = 298062 us
```

The fire-room route retained the L1 recycle but not the old gameplay cliff:

```text
before recycle witness: entries=288/288 SPRITEPROFILE=29824 us
after recycle witness:  entries=78/288  SPRITEPROFILE=36084 us
```

Representative gameplay redraws in the supplied route were broadly about
178-225 ms across movement, doors, hazards and pickups. The user explicitly
reported the fire-room traversal as dramatically smoother and no longer spoiled
by the prior intermittent lag spikes.

Do not optimize `PlatformVideo_present()` from this milestone; it remains a
stable ~34.4 ms fixed transfer and was not the cause of the erratic stalls.

## Current hardware-owned gameplay frontier

Validated behavior retained at the locked boundary includes:

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
native idle weapon rendering + generic attack pose
move-event state mutation with rollback/commit
jammed-door subtype-3 destruction and traversal
generic monster state + type-1 player attack combat
generic player resources / consumed pickup removal
shared 52 B PlayerState + HUD projection
extinguisher ammo consumption + fire removal
pain / corpse / gib presentation
bounded stationary monster retaliation
native PASS_TURN + exact top-bar feedback
compact mutable MonsterPosition owner
legacy-compatible movement planner + RNG reservation/replay
live one-monster movement publication + topology relink
renderer projection of committed moved monster position
generic NEXT and PREV weapon cycling
selected-weapon HUD / first-person redraw without turn advance
live Pistol ammo consumption + generic combat commit
live pickup messages + white pickup flash
adaptive floor/ceiling texture cache under memory pressure
movement-side linked type10/type11 hazard touch into shared PlayerState
live bounded damage text + red viewport flash for movement hazards
live nonlethal monster-retaliation raw damage text + red viewport flash
feedback expiry safely deferred while a native dialog owns the PAK
current-map raw internal-flash PAK backing with no silent gameplay SD fallback
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

## Shared native PlayerState

`EspNativeGameplayPlayerState` remains the single 52 B player-facing owner for:

```text
HP / max HP
armor / max armor
defense / strength / agility / accuracy
XP / level / next-level XP
keys / credits
ammo[6]
inventory[5]
weapon bits / selected weapon
```

Combat, pickups, HUD, ammo, progression, hazards and monster retaliation share
this owner. Do not create per-feature or per-item state islands.

## Hardware functional regression — current boundary

The raw-flash test session retained live gameplay across:

```text
sprites and HUD visible
TURN + MOVE input
SELECT regular door
4-frame door open/close animation
weapon rendering
movement hazard damage
red damage flash + top-bar damage text
health pickup
white pickup flash + pickup text
monster-turn scheduling
```

Representative hardware witnesses:

```text
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[RESIDENTGAMEPLAY] READY ... TURN+MOVE=armed
[ACTION] SELECT ... status=DOOR_OK
[DOORANIM] COMPLETE ... transaction=committed
[HAZARD] COMMIT ... message="3 damage!"
[PLAYERRES] COMMIT ... message="Got Health Vial"
```

No `MAPFLASH MISS` or active-gameplay SD fallback appeared in the supplied trace.

## RAM witness at flash boundary

Before the resident L1 owner:

```text
CACHE_PRE heap8=50976 largest8=42996
```

After:

```text
CACHE_POST heap8=27368 largest8=20468
observedHeap delta = 23608 B
configured owner = 23592 B
```

After startup warmup:

```text
heap8=24708
largest8=20468
```

Later gameplay remained stable around:

```text
heap8=20840
largest8=18420
```

Reserve diagnostic:

```text
audioI2SDMA=16384
audioBuffers=8192
general=32768
total8Target=57344
margin8=0
advisory=REVIEW_HEADROOM
```

This is not a flash-backing failure, but it is a real memory warning. Audio
remains deferred and must not be enabled without a dedicated RAM milestone.

## Intentionally deferred families

```text
persistent validated reuse of an already-staged same-map flash slot
production map-transition flash rebuild/reuse policy
L1 range-record eviction/recycle redesign
PASS_TURN current-tile type10/11 Entity_touched semantics
pickup sound playback / got-face presentation
movement-hazard secondary burn text / pain face / shake / sound
action XP migration: extinguisher +2, jammed door +1
materialized monster drops
corpse-pile trimming
monster movement interpolation/animation
multiple-monster activation/movement ordering
unsupported special calcPath plane corpus
special subtype-10 AI
player lethal/death retaliation transition
monster attack visual / player-pain animation and FX
monster attack sound / actual sound playback
status-warning presentation
chaingun/plasma multi-loop presentation/commit
rocket/BFG radius damage
familiar weapon attack semantics for slots 9..11
generic type-12 destructible combat
special death consequences for subtypes 7, 8, 12, 13
Kronos-specific semantics
password input
SAVEGAME / CHANGEMAP production route
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

Do not force the transition before enough native gameplay exists to complete the
map normally and the flash-slot transition contract is owned.

## Next direction after merge

Do **not** continue code on this locked branch.

After merge:

1. read actual GitHub `main` and exact SHA;
2. re-read this file, `DOCUMENTATION.md` and the latest relevant milestone(s);
3. create a fresh coherent `agent/*` branch from that exact SHA;
4. choose one bounded family from the merged frontier.

Strong storage candidate:

```text
persistent current-map flash-slot reuse
 -> inspect committed header
 -> validate source PAK + map identity
 -> HIT: arm existing verified slot without SD erase/copy
 -> MISS/stale: rebuild with the already-proven staging path
```

That directly attacks the measured ~8.44 s repeated load cost without weakening
the no-SD-gameplay contract.

Other gameplay candidates remain:

```text
monster attack visual / player-pain animation family
generic type-12 destructible combat
player lethal/death transition
multiple-monster activation + movement ordering
monster movement interpolation / animation
PASS_TURN current-tile hazards
action XP migration into PlayerState
materialized monster drops
chaingun/plasma multi-loop mechanics
rocket/BFG radial damage
```

Choose only after re-reading merged `main`.

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
