# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port. Repository state wins over chat history. Serial logs from the real classic CYD are the final runtime authority.

## Current Git boundary

```text
main at branch creation = 9b085a9d8ed254edc98463f33f6d1534215326b9
current main = 9b085a9d8ed254edc98463f33f6d1534215326b9
branch = agent/esp32-native-gameplay-changemap-transition
hardware-tested code boundary = 1c2cbe13d6001459eb8679f7262e495a51585476
status = REAL-CYD NATIVE CHECKPOINT SAVE/LOAD V1 PASS
branch policy = ACTIVE; save-v2 world-state work may continue here
```

Normal GitHub Actions `esp32-cyd` run #258 / run ID `35194006759` passed on the exact hardware-tested code SHA.

Latest detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md)

CHANGEMAP production code is also present on this branch but has **not yet received its dedicated real-CYD exit-transition PASS**. Do not conflate the checkpoint PASS with CHANGEMAP hardware validation.

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

Hardware-proven native behavior includes movement/turn/strafe, collision/topology, event-first SELECT, bounded event/script families, dialog, regular doors and dynamic lines, mutable line textures, player state/resources, pickups, hazards, native weapon rendering/control/combat, compact monster state/position/activation/movement/attack families, HUB INV/WPN/STAT, raw-flash backing and the bounded checkpoint described below.

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

One bounded slot is exposed on HUB `STAT`:

```text
/sd/DoomRPG-ESP32.sav
magic = DRPGSAV1
version = 1
recordBytes = 132
atomic write = temp + verify + backup rename + commit rename + verify
```

The v1 record persists immutable BSP identity, full settled `EspPlayerViewState`, full `EspNativeGameplayPlayerState`, player/runtime fingerprints and a CRC32. It never serializes pointers or a desktop object graph.

Canonical hardware SAVE witness after two Armor Shards:

```text
[NATIVESAVE] SAVE ... bytes=132 map=1 gameplayLoadMapId=1
             pos=416,1696 angle=128
             playerFNV=363261d1 runtimeFNV=c3882516
             sourceBytes=21823 sourceCrc=623f34e4
             recordCrc=e2974fa8
             atomic=temp+backup+rename world=fresh-rebuild
```

The final load reprime restores the semantic HUD owners directly after the saved settled view is reconstructed:

```text
[NATIVESAVE] REPRIME-HUD map=1 gameplayLoadMapId=1 angle=128
             refresh=pending clear=ready mutation=owners-only turn=no
```

Real-CYD LOAD then reaches the full native session again:

```text
[NATIVESAVE] LOAD ... pos=416,1696 angle=128 playerFNV=363261d1
[ENGINESESSION] HUD ... armor=8/20 weapon=2 ammo=8
[ENGINESESSION] SPRITES dependencyStatus=10 catalogSprites=46
[MAPFLASH] REUSE HIT ... rebuild=no
[MAPFLASH] ARM ... resident=1
[ENGINECACHE] PRIMED ...
[RESIDENTGAMEPLAY] READY ...
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

Movement, turning, dynamic doors, events and dialog remained live after LOAD.

### Exact rollback witness

After the SAVE the user deliberately acquired Fire Ext, ammunition and a Small Medkit. Live state became:

```text
playerFNV = a6e115a7
weapon = 1
weapons = 0006
ammo = 10/12/00/00/00/00
items = 01/00/00/00/00
```

LOAD restored:

```text
playerFNV = 363261d1
weapon = 2
weapons = 0004
ammo1 = 8
```

and the next weapon-cycle reported no other owned usable weapon. The Fire Ext acquired after SAVE therefore disappeared exactly as expected.

### Repeated-load RAM witness

Two consecutive completed LOAD cycles stabilized at:

```text
heap8 = 18356
largest8 = 11764
```

No monotonic fixed loss per LOAD was observed. Later gameplay initialized extra lazy owners and fragmented the heap further; the following LOAD retained that already-lower boundary rather than consuming a new fixed block. Continue monitoring RAM, but current evidence does not show a systematic checkpoint-load leak.

### V1 world limitation

`world=fresh-rebuild` is intentional and literal. V1 restores pose + player root but rebuilds mutable map-local world owners from the immutable BSP. It does not yet persist:

```text
resource consumed bits
script/event mutable state
line open/locked state and texture variants
automap reveal state
monster mutable state/positions/activation
destructibles
gameplay RNG state
```

Consequently a pickup consumed before SAVE can physically reappear after LOAD even though the player benefit saved before LOAD is retained.

## CHANGEMAP code boundary — candidate, hardware exit test still pending

Entrance event 1 / tile 69 is owned by the branch transition path:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
```

The candidate supports the show-stats WAIT/ACK handoff, resident teardown, target rebuild/spawn and session configure with fail-closed errors. It remains **candidate** until the user performs the dedicated real-CYD level-exit test.

## Next bounded milestone

Continue save persistence owner-by-owner; do not create a monolithic save dump.

Preferred next slice:

```text
EspNativeGameplayPlayerResources consumed bitset
Entrance: 344 sprites -> 43 B consumed bitset
```

Required design:

- explicit bounded snapshot/restore API;
- versioned save record/section, no raw pointers;
- validate target map + immutable runtime identity + sprite count;
- restore only after fresh resident rebuild;
- keep unrelated mutable-world families explicitly fresh;
- preserve fail-closed behavior.

Required real-CYD proof:

1. consume a pickup before SAVE;
2. SAVE;
3. consume another pickup after SAVE;
4. LOAD;
5. pre-SAVE pickup stays absent;
6. post-SAVE pickup reappears;
7. exact saved player state/pose still return;
8. `shapeData == NULL`, `mediaTexels == NULL`.

After this bounded PASS, add further world owners one family at a time (script/event, lines/textures, automap, monsters, RNG, etc.).

## Intentionally deferred / incomplete families

```text
full mutable-world persistence beyond each validated save-v2 section
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
