# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port. Repository state wins over chat history. Serial logs from the real classic CYD are the final runtime authority.

## Current Git boundary

```text
main at branch creation = 9b085a9d8ed254edc98463f33f6d1534215326b9
current main = 9b085a9d8ed254edc98463f33f6d1534215326b9
branch = agent/esp32-native-gameplay-changemap-transition
hardware-tested save-v2 resource boundary = f52d3f272e75ed29f68037fd343e40252d2ec6bf
hardware-tested current code boundary = 5a1020fd5d160c111ff09ecb8a480f37ea8d0578
status = REAL-CYD CHECKPOINT V2 RESOURCE OVERLAY + HUB FEEDBACK OWNERSHIP PASS
branch policy = ACTIVE; continue bounded save-v2 persistence owner-by-owner
```

Normal GitHub Actions `esp32-cyd` run #265 / run ID `35196771704` passed on the exact save-v2 resource code boundary. The HUB/action-feedback ownership fix passed CI in run #267 / run ID `35198140562` and is now also real-CYD validated. The prior documentation tail at `48f8bf50c3f59acf2260d4274467bc78080690a8` passed CI run #268 / run ID `35198645805`.

Latest detailed records:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md)

CHANGEMAP production code is also present on this branch but has **not yet received its dedicated real-CYD exit-transition PASS**. Do not conflate checkpoint validation with CHANGEMAP hardware validation.

## Permanent architecture / hard invariants

```text
Doom RPG original data/behavior
 -> ESP32-native parsers/catalogs
 -> compact immutable EspMapRuntime
 -> small explicit mutable owners
 -> native event/script engine
 -> native gameplay
 -> native renderer
```

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
```

Do not reintroduce map-wide legacy texels, pointer-heavy desktop ownership, runtime ZIP dependence for migrated gameplay/map data, or map-wide decompression. `/DoomRPG-ESP32.pak` is the native backing store.

Gameplay/input and rendering stay decoupled. Doom RPG is turn-based; do not optimize `PlatformVideo_present()` prematurely.

## Native asset backing — hardware PASS

Active gameplay storage path:

```text
/DoomRPG-ESP32.pak on microSD
 -> requested-map raw internal-flash slot
 -> 19 KiB resident RAM cache
 -> native gameplay / renderer
```

Preparation API:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

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
indexFNV = 3a51cc4d
payloadFNV = 9ec04e22
```

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

Retained fingerprints:

```text
mapStateFNV = cd99b98e
scriptFNV   = f9e3d9df
lineFNV     = e5e74861
textureFNV  = f1fc1875
automapFNV  = 669b1aa7
topologyFNV = 3f321e43
```

Resident cache baseline:

```text
owner = 23592 B
payload = 19456 B
range records = 288 x 12 B
resident entry slots = 24
large exact range = 2048 B
```

## Current hardware-owned gameplay frontier

Hardware-proven native behavior includes movement/turn/strafe, collision/topology, event-first SELECT, bounded event/script families, dialog, regular doors and dynamic lines, mutable line textures, player state/resources, pickups, hazards, native weapon rendering/control/combat, compact monster state/position/activation/movement/attack families, HUB INV/WPN/STAT, raw-flash backing, bounded checkpoint save/load, resource consumed-overlay persistence, and HUB/world framebuffer ownership gating for transient action feedback.

The player root remains:

```text
EspNativeGameplayPlayerState = 52 B
```

The HUB root remains:

```text
EspNativeGameplayHubView = 28 B
pages = INV | WPN | STAT
world dispatch blocked while HUB active
turn advance disabled while HUB active
```

## Native checkpoint save/load v1 — REAL-CYD PASS

The original one-slot checkpoint established the permanent bounded save path:

```text
/sd/DoomRPG-ESP32.sav
magic = DRPGSAV1
version = 1
recordBytes = 132
atomic write = temp + verify + backup rename + commit rename + verify
```

V1 persists immutable BSP identity, full settled `EspPlayerViewState`, full `EspNativeGameplayPlayerState`, player/runtime fingerprints and CRC32. It never serializes pointers or a desktop object graph.

The final load reprime restores the semantic HUD owners directly after the saved settled view is reconstructed:

```text
[NATIVESAVE] REPRIME-HUD ... refresh=pending clear=ready mutation=owners-only turn=no
```

V1 hardware validation proved exact player/pose rollback and repeated-load stability, but map-local mutable owners were rebuilt fresh.

## Native checkpoint save/load v2 resources — REAL-CYD PASS

V2 preserves the exact proven v1 core and adds only one explicit pointer-free section:

```text
magic = DRPGSAV2
version = 2
recordBytes = 276
v1 read compatibility = retained
write format = v2
EspNativeGameplayPlayerResourcesSnapshot = bounded fixed section
max consumed payload = 128 B / 1024 sprites
Entrance used payload = 43 B / 344 sprites
```

The section persists only the semantic consumed-resource overlay plus explicit identity:

```text
sourceArenaFNV1a
spriteCount
consumedCount
consumedBytes
targetMapId
consumedBits[]
```

Hardware SAVE witness after consuming one Armor Shard:

```text
[NATIVESAVE] SAVE ... version=2 bytes=276 map=1 gameplayLoadMapId=1
             pos=480,1696 angle=128
             playerFNV=548397a5 runtimeFNV=c3882516
             sourceBytes=21823 sourceCrc=623f34e4
             recordCrc=4d3bce59
             resources=1/43B sprites=344
             atomic=temp+backup+rename
             world=resources-restored+others-fresh
```

The user then continued gameplay and consumed additional resources; live player state changed to `playerFNV=549e6620` before LOAD.

Hardware LOAD rebuilt the BSP, restored the saved player root and imported the resource section only after immutable identity validation:

```text
[PLAYERRES] READY map=1 arena=c3882516 sprites=344 consumedBytes=43 ...
[PLAYERRES] RESTORE map=1 arena=c3882516 sprites=344 consumed=1 bytes=43
                 mutation=consumed-overlay-only allocation=owner-bounded
[NATIVESAVE] LOAD ... version=2 bytes=276
             pos=480,1696 angle=128 playerFNV=548397a5
             resources=restored/1/43B
             world=resources-restored+others-fresh
```

The complete native session returned successfully:

```text
[ENGINESESSION] HUD ... hp=30/30 armor=4/20 weapon=2 ammo=8
[ENGINESESSION] SPRITES ...
[MAPFLASH] REUSE HIT ... rebuild=no
[MAPFLASH] ARM ... resident=1
[ENGINECACHE] PRIMED ...
[RESIDENTGAMEPLAY] READY ...
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

### Real-CYD two-direction resource proof

The required persistence contract is hardware-proven:

```text
resource consumed before SAVE -> remains absent after LOAD
resource consumed after SAVE -> reappears after LOAD
```

The user explicitly confirmed this with an Armor Shard and the same behavior with a medkit. This proves the saved consumed bitset is projected through the normal native topology/render path, not merely restored as bookkeeping.

### Current v2 world boundary

Persisted:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
```

Still intentionally fresh / not yet persisted:

```text
script/event mutable state
line open/locked state
line texture variants
automap reveal state
monster mutable state/positions/activation/combat consequences
destructibles
gameplay RNG state
```

Continue save persistence owner-by-owner. Never replace this with a monolithic world/object dump.

## HUB/action-feedback framebuffer ownership — REAL-CYD PASS

The v2 hardware test exposed an unrelated visual ownership race: a pickup top-bar message could expire while HUB owned the framebuffer, leaving a stale `Got ...` fragment over the MENU area and causing a later underlay mismatch/recover path.

The bounded fix is:

```text
8b7a4c04dee1622954f2ea453ca1b15792fbf6fa
  ESP32: pause action feedback while HUB owns framebuffer
5a1020fd5d160c111ff09ecb8a480f37ea8d0578
  ESP32: gate world feedback service behind HUB ownership
CI #267 = SUCCESS
```

The timer remains based on real elapsed time, but world feedback/viewport-flash restore work is blocked while HUB owns the framebuffer. The user reproduced the original pickup-message/HUB sequence on the real CYD and confirmed the stale fragment is gone. Therefore `5a1020fd5d160c111ff09ecb8a480f37ea8d0578` is now a hardware-valid code boundary for this ownership fix.

## CHANGEMAP code boundary — candidate, hardware exit test still pending

Entrance event 1 / tile 69 is owned by the branch transition path:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
```

The candidate supports the show-stats WAIT/ACK handoff, resident teardown, target rebuild/spawn and session configure with fail-closed errors. It remains **candidate** until the user performs the dedicated real-CYD level-exit test.

## Next bounded milestone

Continue checkpoint persistence one explicit owner family at a time.

Preferred next slice:

```text
EspMapScriptState / mutable event-command state
```

Required properties:

- explicit pointer-free snapshot/restore API;
- validate map/runtime identity and exact bounded sizes;
- restore only after fresh resident rebuild;
- preserve command-removed/event-state semantics exactly;
- keep lines, automap, monsters, destructibles and RNG fresh until their own milestones;
- fail closed on incompatible/corrupt sections.

After script/event PASS, candidates are line open/locked + texture variants, automap, monster state/position, then RNG, each as separate milestones.

## Intentionally deferred / incomplete families

```text
save-v2 mutable-world persistence beyond each validated section
CHANGEMAP real-CYD exit validation
pre-arm first-frame/HUD SD startup path
L1 range-record eviction/recycle redesign
audio
pickup sound / got-face
secondary hazard feedback
complete mixed movement resource/hazard ordering
action XP migration
materialized monster drops
corpse-pile trimming
monster movement interpolation
simultaneous multi-monster ordering
special subtype-10 AI
player lethal/death transition
multi-loop weapon/monster mechanics
monster projectiles/messages/sound
rocket/BFG radius damage
familiar weapon slots / hazard redirection
generic type-12 destructible combat
special death consequences
Kronos-specific semantics
password input
EV_GIVEMAP production route
EV_CHECK_KEY production route
HUB Notebook activation
HUB consumable confirmation/use
HUB Automap / Options / store
```

## Development workflow

```text
recover true main + docs
 -> choose one bounded behavior family
 -> recover exact legacy behavior where relevant
 -> design a small permanent native API/owner
 -> keep different families fail-closed
 -> commit + push agent/*
 -> CI esp32-cyd
 -> test on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> merge-ready
```

Never merge into `main` without explicit user request.
