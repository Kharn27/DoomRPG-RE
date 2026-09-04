# ESP32 documentation map

Recovery and development should start from:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — authoritative tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers;
5. latest relevant milestone/source on the active branch.

Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime truth.

## Current locked branch

```text
main = 9bd45cc0eb790a7b0894774426f996ee7ae6ce72
branch = agent/esp32-native-map-flash-reuse
base main = 9bd45cc0eb790a7b0894774426f996ee7ae6ce72
hardware-tested code boundary = 8dd0a06f293b801e9afe3097bf19c57d3e1037b7
status = generic requested-map raw-flash slot reuse PASS
branch policy = LOCKED; docs-only tail only
```

Do not treat commits after `8dd0a06f...` as new hardware-tested code. The tail
must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `33867680174` passed on the exact tested
SHA and uploaded:

```text
doom-rpg-esp32-cyd-8dd0a06f293b801e9afe3097bf19c57d3e1037b7
artifact id = 9934627355
```

CI is compile/link evidence only; real-CYD serial logs remain authoritative.

## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

GitHub Actions builds this environment through `.github/workflows/esp32-cyd.yml`
and uploads firmware artifacts. Bring-up diagnostics perturb RAM and are not the
production memory canon. Never claim a local build or hardware pass that did not
occur.

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

The hardware-proven active gameplay path is:

```text
/DoomRPG-ESP32.pak on SD
 -> derive identity/working-set plan for requested targetMapId
 -> exact existing raw-slot HIT, or full rebuild on MISS/stale
 -> raw internal-flash requested-map slot
 -> 19 KiB resident RAM cache
 -> native renderer/gameplay
```

The raw slot is single-world backing storage, but the prepare policy is generic:
`EspAssetPack_mapFlashPrepare(targetMapId)` receives the world the runtime is
actually trying to load. Entrance is only a hardware witness.

A HIT requires source PAK/index identity, requested map id/hash, exact excluded
BSP span topology and flash index/payload FNV validation. A slot for another
world or stale source data rebuilds through the already-proven staging path.
Active gameplay still never silently falls back to SD.

Detailed milestones:

- [`MILESTONE_NATIVE_MAP_FLASH_BACKING.md`](MILESTONE_NATIVE_MAP_FLASH_BACKING.md)
- [`MILESTONE_NATIVE_MAP_FLASH_REUSE.md`](MILESTONE_NATIVE_MAP_FLASH_REUSE.md)

## Classic CYD flash layout

`ESP32/partitions_cyd_raw_pak.csv`:

```text
nvs       0x009000  0x005000
otadata   0x00e000  0x002000
app0      0x010000  0x140000  = 1310720 B
spiffs    0x150000  0x2A0000  = 2752512 B raw slot
coredump  0x3F0000  0x010000
```

The partition named `spiffs` is intentionally not mounted as SPIFFS; it is raw
storage owned through `esp_partition_*`.

Tested firmware size at the preceding flash-backing boundary:

```text
firmware.bin = 644144 B
app0 margin = 666576 B
```

## Entrance format witness

```text
map=1
resource=/intro.bsp
runtimeFNV=c3882516
sourceCRC32=623f34e4
pack=2457398 B
entries=241
index=4820 B
metadata=12288 B
excluded other BSPs=12 / 203811 B
staged payload=2248743 B
partition=2752512 B
headroom=491481 B
```

The first raw-flash build on the real CYD recorded:

```text
[MAPFLASH] COPY indexFNV=3a51cc4d payloadFNV=9ec04e22 verified=yes
[MAPFLASH] READY ... buildUs=8442586 backing=raw-internal-flash
```

That ~8.44 s rebuild remains the correct fallback for a missing/stale/different
requested-world slot.

## Requested-map reuse witness — real CYD

On exact tested SHA `8dd0a06f...`:

```text
[MAPFLASH] REUSE HIT requestedMap=1 current=/intro.bsp cachedMap=1
           sourceIndexFNV=3a51cc4d payloadFNV=9ec04e22
           verifyUs=361875 rebuild=no
[MAPFLASH] ARM map=1 active=1 verified=1 reused=1 staged=2248743
           metadata=12288 prepareUs=363258 buildUs=0 resident=1
```

No `MAPFLASH ERASE`, `MAPFLASH COPY` or `MAPFLASH MISS` appeared in the supplied
trace.

Load-time comparison:

```text
full rebuild = 8442586 us ~= 8.44 s
validated reuse = 363258 us ~= 0.363 s
flash verify = 361875 us ~= 0.362 s
```

The reuse path is about 23.2x faster than the preceding rebuild witness while
still rereading and FNV-validating the staged flash payload.

## Selected resident-cache baseline

```text
owner = 23592 B
payload = 19456 B (19 KiB)
range records = 288
range record = 12 B
resident entry slots = 24
large exact range = 2048 B
```

The 288-record global recycle still exists. It is no longer an SD seek cliff once
the requested-map raw-flash backing is armed.

## Hardware performance result

The preceding SD-backed fire-room witness showed:

```text
entries 288/288 -> 23/288
SPRITEPROFILE ~36.7 ms -> ~241.8 ms
frame ~335 ms -> ~736 ms
VIDEO present ~34.4 ms
```

Raw-flash backing removed that cliff without changing the L1 policy. The reuse
run preserved the same active-gameplay timing class:

```text
SMALL-COLD  = 228559 us
SMALL-WARM  = 178358 us
LARGE-LEARN = 178381 us
LARGE-WARM  = 177638 us
representative SPRITEPROFILE ~= 30-37 ms
PAKIO backing=raw-flash after ARM
```

`PlatformVideo_present()` remains a stable ~34.4 ms and is not the target of this
storage work.

## Remaining startup storage debt

Before raw-flash ARM, the current bootstrap still performs first-frame/HUD work
against SD. The reuse run still showed several pre-arm PAKIO batches of hundreds
of milliseconds.

That is a separate startup-order/performance boundary and is not part of the
locked requested-map reuse milestone.

## Current native gameplay frontier

Hardware-owned behavior includes:

```text
movement / turn / strafe
native collision + compact sprite topology
event-first SELECT routing
SHOW / HIDE / UNLOCK
OPENLINE / CLOSELINE
DIALOG / DIALOGNOBACK
FORCEMESSAGE / NOTE
state ops 11 / 19 / 20
regular door animation + mutable line texture variants
native idle weapon rendering + attack frame presentation
jammed-door subtype-3 Axe destruction + traversal
generic compact monster-state initialization
generic type-1 player attack hit/miss/crit/HP/armor math
pain / corpse / overkill-gib presentation
shared 52 B PlayerState + XP/progression state
generic type 3/4/5/6/16 pickup/resource engine
consumed-pickup world removal
HUD projection from PlayerState
extinguisher ammo consumption + fire removal transaction
stationary monster-turn scheduling + LOS recovery
live nonlethal monster retaliation
native PASS_TURN + exact top-bar feedback
transaction-safe gameplay RNG replay across refill boundary
compact mutable MonsterPosition owner
legacy-compatible bounded movement planner
live one-monster movement commit + topology relink
renderer projection of committed moved monster position
generic NEXT + PREV weapon cycling
live selected-weapon HUD/first-person redraw without turn advance
live Pistol ammo consumption + generic monster combat commit
live generic pickup messages + white viewport-border flash
adaptive native plane cache under memory pressure
movement-side linked type10/type11 hazard damage through shared PlayerState
live bounded damage text + red viewport-border hazard flash
safe feedback expiry while a native dialog owns the PAK
raw-flash gameplay backing with no silent SD fallback
generic requested-map committed-slot reuse with strict rebuild on mismatch
```

Relevant milestone records:

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

## Shared PlayerState

`EspNativeGameplayPlayerState` remains the single 52 B player-facing owner for:

```text
HP / max HP
armor / max armor
defense / strength / agility / accuracy
XP / level / next-level XP
keys / credits
ammo[6]
inventory[5]
weapon ownership / selected weapon
```

Player attacks, resources, HUD projection, hazards and monster retaliation all use
this same owner.

## Current RAM warning

The requested-map reuse path adds no persistent gameplay RAM owner. The existing
resident L1 remains the main asset owner:

```text
CACHE_PRE  heap8=50976 largest8=42996
CACHE_POST heap8=27368 largest8=20468
warmup     heap8=24708 largest8=20468
```

The reserve diagnostic still reports `margin8=0` against the provisional audio
target. Audio remains explicitly deferred.

## Current intentionally deferred families

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
multiple-monster activation/movement ordering
unsupported special calcPath plane corpus
special subtype-10 AI
player lethal/death transition
monster attack visual / player-pain animation and FX
monster attack sound / actual sound playback
status-warning presentation
chaingun/plasma multi-loop mechanics
rocket/BFG radius damage
familiar weapon attack semantics
generic type-12 destructible combat
special death consequences
Kronos-specific semantics
password input
GIVEMAP production route
CHECK_KEY production route
```

These are mechanical family boundaries, not individual monster/item TODOs.

## CHANGEMAP recovery point

Entrance event 1 / tile 69 remains recovered but intentionally deferred:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
OPENLINE -> third eligible command
```

The storage layer can now generically prepare target map 9 and rebuild if the
committed slot belongs to another map. The actual transition still needs a
bounded production owner for teardown/load/spawn/state transfer.

## After this merge

Do not continue code on this locked branch.

When the merge is announced:

1. read the true GitHub `main` and exact SHA;
2. re-read `PORTING_STATUS.md`, this file and latest relevant milestone(s);
3. create a fresh coherent `agent/*` branch from that SHA;
4. choose the next bounded family from the actual merged frontier.

Strong gameplay candidates now include monster attack visual/player-pain
animation and missing combat presentation assets, generic type-12 destructible
combat, lethal/death transition, multiple-monster ordering and movement
interpolation. Choose only after re-reading merged `main` and recovering exact
legacy behavior for one coherent family.

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

After a merge announcement, re-read actual GitHub `main`, record its exact SHA,
and create the next `agent/*` branch from that SHA. Never merge `main` without an
explicit user request.
