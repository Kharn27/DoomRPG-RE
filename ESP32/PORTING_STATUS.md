# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port.
Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git boundary — LOCKED milestone

```text
main at branch creation = dbcbf7b52f9aef851503a919a5a2871743a7de20
branch = agent/esp32-native-gameplay-hub-inventory-view
base main = dbcbf7b52f9aef851503a919a5a2871743a7de20
hardware-tested code boundary = bc136c735c9f5ba173a57fcbbb1f5bea6732b3bd
status = REAL-CYD GAMEPLAY HUB V2 INVENTORY + STATUS READ-ONLY PASS
branch policy = LOCKED; docs-only tail only
```

`bc136c73...` is the exact code boundary exercised on the real classic CYD.
Commits after that SHA must remain documentation-only until merge.

Normal GitHub Actions `esp32-cyd` run `34060186783` / run #161 completed
successfully on this exact SHA and produced the firmware artifact. CI is
compile/link evidence only; hardware serial logs remain authoritative.

Latest detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)

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
native gameplay HUB owner = 28 B, no framebuffer snapshot
read-only Inventory + Status pages projected from shared 52 B PlayerState
HUB viewport-only paint = 160x80/y20..99 with top/bottom HUD bands untouched
TURN_LEFT/RIGHT page navigation with world dispatch blocked
Inventory FORWARD/BACK cursor navigation; Status movement input absorbed
MENU close rerenders exact world frame; SELECT remains fail-closed
```

Detailed records include:

- [`MILESTONE_NATIVE_MONSTER_MOVEMENT.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT.md)
- [`MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md`](MILESTONE_NATIVE_MONSTER_MOVEMENT_LIVE.md)
- [`MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md`](MILESTONE_NATIVE_MONSTER_POSTMOVE_ATTACK.md)
- [`MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md`](MILESTONE_NATIVE_PASS_TURN_HAZARD_TOUCH.md)
- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)

## Latest real-CYD gameplay HUB v2 witness

The tested HUB uses the canonical shared `EspNativeGameplayPlayerState` rather
than a second inventory owner:

```text
hub ownerBytes = 28
playerStateBytes = 52
playerFNV = e745fce9
hudBandsFNV = 6c2aa46f
```

Inventory and Status repeatedly painted inside the 160x80 world viewport with
`hudBands=6c2aa46f preserved=yes`, `playerFNV=e745fce9 exact=yes`,
`packClosed=yes`, `mutation=no` and `turn=no`.

Representative deterministic page frames:

```text
Inventory row 0 = 06138e61
Status          = b35c12b9
Inventory row 2 = 71bffc61
Inventory row 1 = 1a817e61
```

`FORWARD` on Status was absorbed with `worldDispatch=blocked`. Closing from the
HUB preserved `e745fce9->e745fce9 exact=yes`, kept HUD bands untouched, and the
normal compositor rebuilt the exact pre-HUB world frame `22397b55`.

The first full-screen prototype had left menu pixels in the top/bottom HUD bands.
That regression is closed permanently by clipping the HUB to `y=20..99`; no
12.8 KiB HUD-band backup and no 38.4 KiB framebuffer backup were introduced.

## Retained real-CYD active sequence witness

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

The same hardware session also confirmed the regression fix on regular doors:
`DOORANIM` ran all four moving/stable frames and SELECT then reached enemy combat
through the open door.

## Subtype 4/13 three-goal owner retained

The bounded legacy `Entity_aiMoveToGoal()` `i=3` owner for subtypes `4/13`
remains present:

```text
first goal = existing committed movement
continuation goals 2/3 = settled player destination
successful continuation RNG = one visit-choice byte per goal
publication = existing live movement transaction
unsupported special calcPath plane crossing = fail closed
multi-loop / three-shot attack family = fail closed
```

The HUB hardware session does not add a new subtype-4/13 witness; do not infer
one merely because the binary contains that owner.

## RAM witness at current boundary

The supplied real-CYD HUB session remained flat before, during and after repeated
page switches, ignored Status movement input, Inventory cursor movement and
return to the world:

```text
heap = 89280
heap8 = 23548
largest8 = 20468
hub owner = 28 B
player owner = 52 B
HUB framebuffer snapshot = 0 B
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
HUB entity/item names and richer visual chrome
HUB weapon selection
HUB consumable confirmation/use + turn consumption
HUB Notebook / Automap / Save / Load / Options / store
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
   `MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`;
3. create a fresh `agent/*` from that exact main SHA;
4. recover the next bounded legacy/UI family before coding.

Strong next milestone: a **visual/readability HUB pass** only. Recover the useful
legacy in-game menu look (dark blue 8 px grid, proper labels/entity names,
selection affordance and compact page chrome) while preserving the 28 B owner,
160x80 viewport-only clipping and read-only semantics. Do not combine this with
item-use mutations or turn consumption.

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
