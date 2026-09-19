# Doom RPG ESP32 CYD porting status

Authoritative recovery/status file for the classic ESP32-2432S028R port. Repository state wins over chat history. Serial logs from the real classic CYD are the final runtime authority.

## Current Git boundary

```text
main at branch creation = 9b085a9d8ed254edc98463f33f6d1534215326b9
current main = 9b085a9d8ed254edc98463f33f6d1534215326b9
branch = agent/esp32-native-checkpoint-v4-lines
hardware-tested save-v2 resource boundary = f52d3f272e75ed29f68037fd343e40252d2ec6bf
hardware-tested save-v3 script boundary = fd206c5238ac2db62939d100bf3d08ac39081c69
hardware-tested save-v4 line boundary = 2efb9634ffc1c2fb433c4c3340ff9c722c5c7b4d
hardware-tested current code boundary = 2efb9634ffc1c2fb433c4c3340ff9c722c5c7b4d
status = REAL-CYD CHECKPOINT V4 RESOURCE + SCRIPT + LINE STATE PASS
branch policy = ACTIVE; V4 hardware PASS, docs-only tail, merge-ready after docs
```

Normal GitHub Actions `esp32-cyd` run #265 / run ID `35196771704` passed on the exact save-v2 resource code boundary. The HUB/action-feedback ownership fix passed CI in run #267 / run ID `35198140562` and is also real-CYD validated. The save-v3 script persistence boundary `fd206c5238ac2db62939d100bf3d08ac39081c69` passed GitHub Actions `esp32-cyd` run #275 / run ID `35199788280` and is real-CYD validated in both rollback and persistence directions. The save-v4 line boundary `2efb9634ffc1c2fb433c4c3340ff9c722c5c7b4d` passed GitHub Actions `esp32-cyd` run #293 / run ID `35344853078` and is real-CYD validated, including the HUB stack fix and soldier-door unlock persistence.

Latest detailed records:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md)
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

Retained fresh-map fingerprints:

```text
mapStateFNV = cd99b98e
scriptFNV   = f9e3d9df
lineFNV     = e5e74861
textureFNV  = f1fc1875
automapFNV  = 669b1aa7
topologyFNV = 3f321e43
```

The save-v3 hardware mirror test also captured a deliberately mutated script snapshot with `scriptFNV=f9e59e9f`; this is a checkpoint-state fingerprint, not a replacement for the canonical fresh-map `f9e3d9df` above.

Resident cache baseline:

```text
owner = 23592 B
payload = 19456 B
range records = 288 x 12 B
resident entry slots = 24
large exact range = 2048 B
```

## Current hardware-owned gameplay frontier

Hardware-proven native behavior includes movement/turn/strafe, collision/topology, event-first SELECT, bounded event/script families, dialog, regular doors and dynamic lines, mutable line textures, player state/resources, pickups, hazards, native weapon rendering/control/combat, compact monster state/position/activation/movement/attack families, HUB INV/WPN/STAT, raw-flash backing, bounded checkpoint save/load, resource consumed-overlay persistence, mutable script/event-state persistence, mutable line open/locked + texture-variant persistence, and HUB/world framebuffer ownership gating for transient action feedback.

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

Hardware validation proved both directions:

```text
resource consumed before SAVE -> remains absent after LOAD
resource consumed after SAVE -> reappears after LOAD
```

The proof was exercised with Armor Shards and a Small Medkit through the normal native topology/render path, not merely as restored bookkeeping.

## Native checkpoint save/load v3 script — REAL-CYD PASS

V3 keeps the v1 core and v2 resource section and appends exactly one compact `EspMapScriptStateSnapshot`:

```text
magic = DRPGSAV3
version = 3
recordBytes = 808
v1/v2 read compatibility = retained
write format = v3
script snapshot max payload = 512 B
```

The script section persists pointer-free semantic bytes only:

```text
sourceArenaFNV1a
eventCount
byteCodeCount
eventStateBytes
removedCommandBytes
storageBytes
storage[] = packed event states + removed-command bits
```

Entrance uses:

```text
events = 93
byteCodes = 265
script storage = 81 B
```

The snapshot/restore API validates runtime identity, exact counts/sizes and unused tail bytes, restores only into the freshly rebuilt native script owner, and verifies the restored semantic fingerprint before session configure. Corrupt or incompatible sections fail closed.

The final hardware mirror SAVE captured already-mutated event state and three consumed resources:

```text
[NATIVESAVE] SAVE ... version=3 bytes=808
             pos=160,1504 angle=64
             playerFNV=549e6620 runtimeFNV=c3882516
             recordCrc=dc35a833
             resources=3/43B sprites=344
             script=93/265/81B scriptFNV=f9e59e9f
             atomic=temp+backup+rename
             world=resources+script-restored+others-fresh
```

Gameplay then diverged after SAVE by consuming a Bullet Clip and Fire Ext; live state reached `playerFNV=a6e115a7`, weapon 1, weapons `0006`, ammo0=10, ammo1=12 and five consumed resources.

LOAD restored the exact checkpoint:

```text
[PLAYERRES] READY ... playerFNV=549e6620
[PLAYERRES] RESTORE ... consumed=3 bytes=43
[NATIVESAVE] LOAD ... version=3 bytes=808
             pos=160,1504 angle=64
             playerFNV=549e6620
             resources=restored/3/43B
             script=restored/93/265/81B/f9e59e9f
             world=resources+script-restored+others-fresh
[ENGINESESSION] HUD ... hp=30/30 armor=8/20 weapon=2 ammo=8
```

The strongest semantic witness came immediately after LOAD: event 79, which had been completed **before** SAVE, remained ineligible instead of reopening:

```text
[MOVEEVENT] EXIT-PREFLIGHT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
[MOVEEVENT] EXIT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
```

Together with the earlier opposite-direction test where post-SAVE dialog/event mutations were rolled back and became executable again, V3 proves:

```text
script/event mutation after SAVE -> rolled back by LOAD
script/event mutation before SAVE -> preserved by LOAD
```

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md)

### Current v3 world boundary

Persisted:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
```

Still intentionally fresh / not yet persisted:

```text
line open/locked state
line texture variants
automap reveal state
monster mutable state/positions/activation/combat consequences
destructibles
gameplay RNG state
```

Continue save persistence owner-by-owner. Never replace this with a monolithic world/object dump.

Long-session hardware steady point observed during the V3 validation:

```text
heap8 = 17724
largest8 = 8692
```

This remained stable across the final LOAD/replay segment. Fragmentation/headroom remains below the advisory target and should stay on the review list, but the run does not demonstrate a new per-LOAD leak.

## Native checkpoint save/load v4 lines — REAL-CYD PASS

V4 keeps the proven v1/v2/v3 semantic sections and appends one compact line-family snapshot:

```text
magic = DRPGSAV4
version = 4
recordBytes = 1212
v1/v2/v3 read compatibility = retained
write format = v4
line count max = 1024
line bitset max = 128 B
Entrance lines = 480
Entrance bitset bytes = 60
```

The section persists only:

```text
open bit per line
locked bit per line
mutable locked/unlocked texture-10 variant bit
runtime identity + exact line count/size
line-state and texture-state fingerprints
```

The real-CYD load first rebuilt canonical Entrance line owners at `locked=7`, `texture10=0`, then restored the saved soldier-door state:

```text
[MAPLINECHECKPOINT] RESTORE ... lines=480 bytes=60
                    open=0 locked=6 texture10=1
                    lineFNV=69334d90 textureFNV=bda09634
[NATIVESAVE] LOAD ... version=4 bytes=1212
             resources=restored/5/43B
             script=restored/93/265/81B/26f291e3
             lines=restored/480/60B/open0/locked6/tex101/69334d90/bda09634
```

After LOAD, line 352 opened normally with `locked=0` without replaying the soldier unlock script:

```text
[ACTION] DOOR line=352 opcode=15 status=OK open=0->1 locked=0 ...
[DOORANIM] COMPLETE ... transaction=committed
```

The first V4 hardware attempt also exposed a `loopTask` stack-canary reset when entering STAT. The final code boundary moved the large read workspace out of the HUB stack, removed large full-record CRC/verification copies, and passed both CI and the real-CYD INV -> STAT -> LOAD sequence without reset.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md)

### Current v4 world boundary

Persisted:

```text
settled player pose
EspNativeGameplayPlayerState
EspNativeGameplayPlayerResources consumed overlay
EspMapScriptState event states + removed-command bits
EspMapLineState open/locked state
EspMapLineTextureState locked/unlocked texture variants
```

Still intentionally fresh / not yet persisted:

```text
automap reveal state
monster mutable state/positions/activation/combat consequences
destructibles
gameplay RNG state
```

Post-LOAD session invariants remained intact:

```text
shapeData == NULL
mediaTexels == NULL
heap8 = 14076
largest8 = 6644
```

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

The timer remains based on real elapsed time, but world feedback/viewport-flash restore work is blocked while HUB owns the framebuffer. The user reproduced the original pickup-message/HUB sequence on the real CYD and confirmed the stale fragment is gone. Therefore `5a1020fd5d160c111ff09ecb8a480f37ea8d0578` is a hardware-valid code boundary for this ownership fix.

## CHANGEMAP code boundary — candidate, hardware exit test still pending

Entrance event 1 / tile 69 is owned by the branch transition path:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
```

The candidate supports the show-stats WAIT/ACK handoff, resident teardown, target rebuild/spawn and session configure with fail-closed errors. It remains **candidate** until the user performs the dedicated real-CYD level-exit test.

## Next bounded milestone

Checkpoint V4 is now hardware-proven and this branch is at a docs-only tail. Do not broaden persistence again before a new bounded milestone is chosen from the real repo/legacy behavior.

Two correctness/polish gaps observed against the J2ME reference are now high-priority candidates for the next branch after merge:

- rotation in place must **not** advance a gameplay/monster turn; current native logs still schedule `MONSTERTURN reason=ROTATE`;
- player attack feedback still lacks the legacy damage message and enemy-hit blood-pixel feedback.

Treat those as separate bounded behavior milestones unless legacy recovery shows they share one permanent owner. The CHANGEMAP production candidate also remains without its dedicated real-CYD level-exit PASS and must not be called validated until that test occurs.

## Intentionally deferred / incomplete families

```text
save-v4 mutable-world persistence beyond each validated section
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
