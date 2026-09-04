# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main = 9bd45cc0eb790a7b0894774426f996ee7ae6ce72
branch = agent/esp32-native-map-flash-reuse
base main = 9bd45cc0eb790a7b0894774426f996ee7ae6ce72
hardware-tested code boundary = 8dd0a06f293b801e9afe3097bf19c57d3e1037b7
status = REAL-CYD GENERIC REQUESTED-MAP RAW-FLASH SLOT REUSE PASS
branch policy = LOCKED; docs-only tail only
```

`8dd0a06f...` is the exact code boundary exercised on the real classic CYD.
Commits after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33867680174` completed successfully on
this exact SHA and uploaded:

```text
doom-rpg-esp32-cyd-8dd0a06f293b801e9afe3097bf19c57d3e1037b7
artifact id = 9934627355
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

The active gameplay storage path is:

```text
/DoomRPG-ESP32.pak on microSD
        |
        | load / source identity
        v
raw internal-flash requested-map slot
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
- One complete requested-map gameplay working set is present before gameplay arms.
- Existing slot reuse is keyed to the map actually requested by the runtime.
- Entrance is a witness only, never a storage special case.
- Active gameplay does not silently fall back to SD.
- Original PAK index/offset semantics are retained.
- Access to intentionally excluded other BSPs fails closed.
- Flash metadata/header is committed only after copy + readback verification.
- Reuse revalidates source identity, requested-world layout and flash FNVs.
- The existing 19 KiB resident RAM cache remains the L1 policy.
- shapeData == NULL and mediaTexels == NULL remain mandatory.
```

Detailed records:

- [`MILESTONE_NATIVE_MAP_FLASH_BACKING.md`](MILESTONE_NATIVE_MAP_FLASH_BACKING.md)
- [`MILESTONE_NATIVE_MAP_FLASH_REUSE.md`](MILESTONE_NATIVE_MAP_FLASH_REUSE.md)

## Classic CYD flash layout

`esp32-cyd` uses `ESP32/partitions_cyd_raw_pak.csv`:

```text
nvs       0x009000  size 0x005000
otadata   0x00e000  size 0x002000
app0      0x010000  size 0x140000  = 1310720 B
spiffs    0x150000  size 0x2A0000  = 2752512 B
coredump  0x3F0000  size 0x010000
```

The partition named `spiffs` is intentionally **not mounted as a filesystem**.
`esp_asset_pack.cpp` owns it as raw flash via `esp_partition_*`.

The flash-backing milestone tested `firmware.bin = 644144 B`, leaving 666576 B
inside the 1.25 MiB application partition at that boundary.

A board migrating from the old `no_ota.csv` layout must receive the generated
`partitions.bin`; flashing only `firmware.bin` leaves the old partition table.

## Entrance canonical witness

Entrance remains the canonical map-format witness, not a generic runtime special
case:

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

Real-CYD Entrance plan from the preceding backing milestone:

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

Hardware build/verification:

```text
[MAPFLASH] ERASE bytes=2752512 buffer=4096 owner=transient
[MAPFLASH] COPY indexFNV=3a51cc4d payloadFNV=9ec04e22 verified=yes
[MAPFLASH] READY map=1 staged=2248743 metadata=12288 excluded=12/203811
           buildUs=8442586 backing=raw-internal-flash SDGameplayReads=forbidden
```

The ~8.44 s staging cost is the deliberate fallback cost for a missing/stale or
different-world slot.

## Generic requested-map flash reuse — hardware PASS

The permanent preparation API is now:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

The caller supplies the world actually being loaded. At the tested bootstrap,
`EspPlayerView.targetMapId` is passed directly.

A reuse HIT requires agreement on:

```text
requested targetMapId + BSP name hash
source PAK byte size / index offset / data offset / entry count
source PAK index FNV-1a
payload flash offset + staged byte count
excluded BSP byte/count + exact excluded-span topology
flash index readback FNV
flash payload readback FNV
```

Any mismatch returns to the already-proven complete staging path. A future save
pointing at another level must therefore prepare that level; a cached Entrance
slot cannot be accepted for it.

Real-CYD reuse witness on exact tested SHA:

```text
[MAPFLASH] REUSE HIT requestedMap=1 current=/intro.bsp cachedMap=1
           sourceIndexFNV=3a51cc4d payloadFNV=9ec04e22
           verifyUs=361875 rebuild=no
[MAPFLASH] ARM map=1 active=1 verified=1 reused=1 staged=2248743
           metadata=12288 prepareUs=363258 buildUs=0 resident=1
```

Critical negative witnesses:

```text
no [MAPFLASH] ERASE
no [MAPFLASH] COPY
no [MAPFLASH] MISS
```

Load-time comparison:

```text
full rebuild = 8442586 us ~= 8.44 s
reuse prepare = 363258 us ~= 0.363 s
reuse verify  = 361875 us ~= 0.362 s
reuse buildUs = 0
```

The validated reuse path is about **23.2x faster** than the preceding full
rebuild witness while still rereading and FNV-validating the staged flash
payload before accepting it.

## Resident RAM-cache baseline retained

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

The 288-record global recycle policy still exists. Internal-flash refill prevents
that recycle from becoming the former SD seek cliff.

## Performance result — hardware PASS

Earlier SD-backed fire-room witness:

```text
entries 288/288 -> 23/288
SPRITEPROFILE ~36.7 ms -> ~241.8 ms
frame ~335 ms -> ~736 ms
VIDEO present ~34.4 ms
```

Raw-flash backing witness:

```text
entries=288/288 SPRITEPROFILE=29824 us
entries=78/288  SPRITEPROFILE=36084 us
```

The reuse run preserved the same active-gameplay behavior:

```text
PAKIO backing = raw-flash
SMALL-COLD  = 228559 us
SMALL-WARM  = 178358 us
LARGE-LEARN = 178381 us
LARGE-WARM  = 177638 us
SPRITEPROFILE representative ~= 30-37 ms
VIDEO present ~= 34.4 ms
```

Do not optimize `PlatformVideo_present()` from this result; the stable LCD
transfer is not the source of the former intermittent storage stalls.

## Remaining pre-arm startup storage debt

Before `MAPFLASH ARM`, the current bootstrap still renders some first-frame/HUD
assets directly from SD. The reuse hardware trace still contains pre-arm PAKIO
batches of several hundred milliseconds.

That is a separate startup-order/performance boundary. It is not a failure of
requested-map slot reuse and should not be mixed into this locked milestone.

## Current hardware-owned gameplay frontier

Validated behavior retained at this boundary includes:

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
raw internal-flash gameplay backing with no silent gameplay SD fallback
generic requested-map committed-slot reuse with strict rebuild on mismatch
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

## RAM witness at current boundary

The reuse path did not add a persistent RAM owner:

```text
CACHE_PRE  heap8=50976 largest8=42996
CACHE_POST heap8=27368 largest8=20468
observed cache delta = 23608 B
configured resident owner = 23592 B
warmup heap8=24708 largest8=20468
```

The tested session again reached:

```text
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

The provisional audio reserve diagnostic still reports `margin8=0`. Audio
remains deferred and must not be enabled without a dedicated RAM milestone.

## Intentionally deferred families

```text
production SAVEGAME / CHANGEMAP transition ownership
pre-arm first-frame/HUD SD startup path
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

The storage side is now generic enough to prepare `targetMapId=9` and rebuild if
the committed slot belongs to another world. The actual transition still needs
its own bounded ownership for teardown/load/spawn/state transfer before CHANGEMAP
can become production-live.

## Next direction after merge

Do **not** continue code on this locked branch.

After merge:

1. read actual GitHub `main` and exact SHA;
2. re-read this file, `DOCUMENTATION.md` and the latest relevant milestone(s);
3. create a fresh coherent `agent/*` branch from that exact SHA;
4. choose one bounded family from the merged frontier.

Strong gameplay candidates:

```text
monster attack visual / player-pain animation and FX
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

Given the current user-visible gaps, combat presentation / missing attack sprites
is a strong next direction, but choose only after re-reading merged `main` and
recovering the exact legacy family behavior.

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
