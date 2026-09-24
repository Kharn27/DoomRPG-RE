# ESP32 documentation map

Recovery and development must start from:

1. current GitHub `main` and its exact SHA;
2. [`PORTING_STATUS.md`](PORTING_STATUS.md) — authoritative tested/candidate boundary;
3. [`ARCHITECTURE.md`](ARCHITECTURE.md) — permanent native engine design;
4. this file — build/layout/recovery pointers;
5. the latest relevant milestone on the active branch.

Repository state wins over chat history. Serial logs from the real classic CYD are the final runtime truth.

## Current active branch

```text
current main = da397ce44dcd75114ca4d031be29b5e7d87e7d5a
branch = agent/esp32-touch-feedback-facing-race
hardware-tested code head = 6ab5d25216b52f096563a95749f1dbd8b33712dd
event43 original hardware-tested code head = 48accf900d486d6633dd83a7568f781458e7685d
esp32-cyd CI #693 = SUCCESS
static RAM = 45736 B
flash = 764741 B
hardware status = rebased boot + SYS LOAD + cold main-menu LOAD + initial RNG seed PASS
status = merge-ready
```
```

Current rebased integration:

```text
main HUB = INV | WPN | STAT | SYS
WPN = complete 3x3 normal arsenal
SYS = two-step SAVE/LOAD/NO SAVE
touch feedback = 640 compact 4-byte edits + per-pixel ownership restore
agent checkpoint = V8 monster-state extension retained
event43 = bounded EV_SHOW x4 MOVE chain retained
CHECK_KEY = native PlayerState key gate retained
Codex fix = SHOW rollback lease released after pending dialog finalizes
boot fix = compact feedback owner restores menu.bsp contiguous heap
LOAD fix = session replacement bypasses ordinary HUB-close HUD exactness gate
RNG fix = core creation seeds the inherited 128-byte Random_t table once
```

Current continuation also has a real-CYD pass for native EV_CHECK_KEY on the Entrance Yellow Door. Opcode 41 uses the shared PlayerState key bitmask, reports Need Yellow Key for selector 1 / mask 0x02, queues bounded top-bar feedback, and pauses before the following OPENLINE with zero world/script mutation when the key is absent.

Entrance tile 377 / event 43 is no longer a progression blocker. Hardware proved the exact bounded MOVE chain EV_SHOW x4 on ENTER followed later by EV_CLOSELINE 102 on EXIT. The SHOW journal is a single 192-byte static owner; the public MOVE result is 76 bytes and carries no batch-sized stack payload. Preflight applies+reverse-rolls-back the four SHOWs before MOVE commit, and the rendered destination frame closes the rollback lease only after success. Exact serial witnesses are in PORTING_STATUS.md and MILESTONE_NATIVE_MOVE_SHOW_BATCH_EVENT43.md.

Current hardware-proven addition on this branch:

```text
native facing-entity top-bar label
 -> compact 34-byte current-target owner
 -> no legacy Entity_t pointer
 -> no resident table of entity names
 -> on-demand /entities.db name read through DoomRPG-ESP32.pak
 -> recovered short forward trace: +31-unit origin, 3 tile steps
 -> sprite + line-entity targets
 -> type-9 blocker remains label-hidden
 -> topbar fallback below timed feedback and statBarMessage
```

Real-CYD witnesses:

```text
Civilian : sprite target, distance=1
Computer : line target, distance=2 then 1
Door     : line target, distance=3
none     : exact label clear after rotation
HUB close: current Civilian target retained and repainted
```

Pure TURN retargeting remains non-turn gameplay:

```text
[MONSTERTURN] ROTATE-NO-TURN ... legacyAdvance=no
```

The supplied hardware run remained alive with:

```text
heap=81936
heap8=16384
largest8=11764
```

Latest relevant milestones:

- [`MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_REDESIGN.md)
- [MILESTONE_NATIVE_MOVE_SHOW_BATCH_EVENT43.md](MILESTONE_NATIVE_MOVE_SHOW_BATCH_EVENT43.md)
- [MILESTONE_NATIVE_CHECK_KEY_YELLOW_DOOR.md](MILESTONE_NATIVE_CHECK_KEY_YELLOW_DOOR.md)
- [MILESTONE_NATIVE_FACING_LABEL.md](MILESTONE_NATIVE_FACING_LABEL.md)

Previously merged relevant milestones remain:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V7_AUTOMAP.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V7_AUTOMAP.md)
- [`MILESTONE_NATIVE_AUTOMAP.md`](MILESTONE_NATIVE_AUTOMAP.md)
- [`MILESTONE_NATIVE_PICKUP_FEEDBACK.md`](MILESTONE_NATIVE_PICKUP_FEEDBACK.md)
- [`MILESTONE_MAIN_MENU_LOAD.md`](MILESTONE_MAIN_MENU_LOAD.md)
- [`MILESTONE_NATIVE_RESIDENT_GAMEPLAY_POLISH.md`](MILESTONE_NATIVE_RESIDENT_GAMEPLAY_POLISH.md)

### Next milestone

After merge, re-read the exact new `main` SHA and create the next `agent/*`
branch from it. The next major gameplay candidate remains the native
**CHANGEMAP / Entrance level-exit transition**.

Current rebased hardware proof includes successful boot, LOAD from SYS, LOAD
directly from the cold main menu, and a non-zero first post-LOAD crate outcome
(`first=99 -> type3/subtype21 Armor Shard`) after fixing the calloc-zero RNG
table. The historical event43 PASS remains anchored to `48accf9`; no fresh
post-rebase event43 serial witness is claimed. The Codex SHOW-exit +
ENTER-dialog lease fix remains CI-valid but not directly hardware-reached.
## Build environment

Normal hardware reference:

```text
pio run -e esp32-cyd
```

GitHub Actions builds this environment through `.github/workflows/esp32-cyd.yml`. Bring-up diagnostics perturb RAM and are not the production memory canon. Never claim a local build or hardware pass that did not occur.

The production environment uses:

```text
board_build.partitions = partitions_cyd_raw_pak.csv
```

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

Do not recreate map-wide texel ownership, pointer-heavy desktop world graphs or native runtime ZIP dependence. `/DoomRPG-ESP32.pak` remains the native asset source/backing store.

## Current native asset backing

Hardware-proven active gameplay path:

```text
/DoomRPG-ESP32.pak on SD
 -> requested-map raw internal-flash slot
 -> 19 KiB resident RAM cache
 -> native renderer/gameplay
```

Preparation API:

```text
EspAssetPack_mapFlashPrepare(targetMapId)
```

Entrance storage witness:

```text
pack=2457398 B
entries=241
index=4820 B
metadata=12288 B
excluded other BSPs=12 / 203811 B
staged payload=2248743 B
partition=2752512 B
headroom=491481 B
indexFNV=3a51cc4d
payloadFNV=9ec04e22
```

## Entrance canonical format witness

```text
map=1
resource=/intro.bsp
sourceBytes=21823
sourceCRC32=623f34e4
sourceFNV=d5cc751f
runtime arena=14095 B
runtimeFNV=c3882516
resident payload=17891 B
spawn tile=904
spawn position=544,1824
spawn direction=64
nodes=223
lines=480
sprites=344
events=93
byteCodes=265
strings=94
native topology entities=220
enemies=30
destructibles=13
```

Canonical fresh-map fingerprints:

```text
mapStateFNV=cd99b98e
scriptFNV=f9e3d9df
lineFNV=e5e74861
textureFNV=f1fc1875
automapFNV=669b1aa7
topologyFNV=3f321e43
```

The V3 hardware mirror checkpoint deliberately captured a mutated script owner at `scriptFNV=f9e59e9f`; that checkpoint fingerprint must not replace the fresh-map canonical `f9e3d9df` above.

Resident cache baseline:

```text
owner=23592 B
payload=19456 B
range records=288 x 12 B
resident entry slots=24
large exact range=2048 B
```

## Current native gameplay frontier

The real-CYD-owned engine includes native movement/collision, rotation-in-place without gameplay/monster turn advancement, event-first SELECT, bounded event/script execution, dialog, dynamic doors/lines including pure multi-line SELECT door batches, mutable line textures, shared PlayerState, pickups/resources, hazards, native weapon rendering/control/combat, type-12/subtype-2 crate combat with exact transform RNG and transformed-pickup projection, monster state/position/activation/movement/attack families, raw-flash requested-map backing, the four-page HUB `INV/WPN/STAT/SYS`, bounded checkpoint save/load from both HUB and the main menu, resource consumed-overlay persistence, script/event-state persistence, line open/locked + texture-variant persistence, V5 action-owned removed-sprite persistence, hardware-proven checkpoint-resume HUD/cache/input rearm, and HUB/world feedback framebuffer ownership gating.

Player/HUB compact roots:

```text
EspNativeGameplayPlayerState = 52 B
EspNativeGameplayHubView = 28 B
```

### HUB

```text
pages = INV | WPN | STAT | SYS
WPN = complete 3x3 normal arsenal
STAT = read-only
SYS = two-step SAVE/LOAD checkpoint page
MENU underlay = 32x20 RGB565 = 1280 B
world dispatch blocked while HUB active
turn advance disabled while HUB active
```

INV projects Notebook, carried items, Credits and keys. WPN is the 4x3 direct-touch normal arsenal grid; familiar IDs 9..11 remain excluded from the normal weapon grid. STAT owns the bounded in-game SAVE/LOAD controls. The main menu exposes a separate LOAD-only entry through the same checkpoint service.

## Main-menu Load Game — REAL-CYD PASS

```text
Start Game
Load Game
Options
Help/About
```

The ESP32 presentation replaces the obsolete J2ME Exit row and keeps the existing double-tap confirmation contract. The original main-menu LOAD milestone was hardware-proven at 18c1cfb for the then-current checkpoint formats: a successful read released menu-only runtime, restored the native world and entered ST_PLAYING with intro replay disabled; missing/invalid saves remained in MENU_MAIN with red No Save.

**Current qualification:** after the later V8 monster-state extension, the user has reproduced a cold-boot regression where main-menu Load Game reports No Save for a valid V8 file that subsequently loads successfully through HUB/STAT in the same firmware. Therefore the historical main-menu pass must not be treated as a current V8 cold-load proof until that startup-path regression is fixed and retested.

## Native checkpoint save/load

### V1 core — hardware proven

```text
/sd/DoomRPG-ESP32.sav
DRPGSAV1
version=1
recordBytes=132
```

The v1 semantic core persists:

```text
saved BSP sourceBytes + sourceCRC32
immutable runtime FNV
map/load identity
full settled EspPlayerViewState
full EspNativeGameplayPlayerState
player FNV
record CRC32
```

Atomic write contract:

```text
temp write -> reread/validate -> old primary to backup -> temp to primary -> reread/validate
```

The final LOAD path rebuilds the BSP from native PAK data, verifies immutable identity, restores player/pose, then recreates the two semantic HUD reprime owners before configuring the gameplay session.

### V2 resource section — REAL-CYD PASS

V2 adds one bounded pointer-free resource snapshot to the proven v1 core:

```text
DRPGSAV2
version=2
recordBytes=276
v1 read compatibility=retained
max consumed payload=128 B
max represented sprites=1024
Entrance sprites=344
Entrance used consumed bytes=43
```

The section stores runtime/map identity, sprite count, consumed count/byte count, and the consumed bitset. It never persists the heap-owned `ResourceOwner` itself.

The real-CYD behavior proved both directions:

```text
pickup consumed before SAVE -> remains absent after LOAD
pickup consumed after SAVE -> reappears after LOAD
```

Armor Shards and a Small Medkit exercised that path through normal native topology/rendering.

### V3 script section — REAL-CYD PASS

Current writes use:

```text
DRPGSAV3
version=3
recordBytes=808
v1/v2 read compatibility=retained
```

V3 keeps the v1 core plus v2 resource section and appends exactly one `EspMapScriptStateSnapshot`. The section is pointer-free and bounded:

```text
ESP_MAP_SCRIPT_STATE_SNAPSHOT_MAX_BYTES=512
Entrance events=93
Entrance byteCodes=265
Entrance event-state bytes=47
Entrance removed-command bytes=34
Entrance script storage=81 B
```

The payload stores:

```text
sourceArenaFNV1a
eventCount
byteCodeCount
eventStateBytes
removedCommandBytes
storageBytes
packed event states + removed-command bits
```

Restore is performed only after fresh immutable runtime reconstruction. It validates the runtime FNV, event/bytecode counts, exact packed sizes and zero tail, copies into the existing compact owner without allocation, and checks the post-restore semantic fingerprint before session configure.

Final mirror SAVE witness after two Armor Shards, the Small Medkit and relevant event/script mutations:

```text
[NATIVESAVE] SAVE ... version=3 bytes=808
             map=1 gameplayLoadMapId=1
             pos=160,1504 angle=64
             playerFNV=549e6620 runtimeFNV=c3882516
             recordCrc=dc35a833
             resources=3/43B sprites=344
             script=93/265/81B scriptFNV=f9e59e9f
             atomic=temp+backup+rename
```

Gameplay then diverged by taking a Bullet Clip and Fire Ext, reaching:

```text
playerFNV=a6e115a7
weapon=1
weapons=0006
ammo0=10
ammo1=12
consumed resources=5
```

LOAD restored exactly the checkpoint:

```text
[PLAYERRES] READY ... playerFNV=549e6620
[PLAYERRES] RESTORE ... consumed=3 bytes=43
[NATIVESAVE] REPRIME-HUD ... refresh=pending clear=ready ...
[NATIVESAVE] LOAD ... version=3 bytes=808
             pos=160,1504 angle=64
             playerFNV=549e6620
             resources=restored/3/43B
             script=restored/93/265/81B/f9e59e9f
[ENGINESESSION] HUD ... hp=30/30 armor=8/20 weapon=2 ammo=8
```

The semantic mirror proof is stronger than the checksum alone. Event 79 had already advanced before SAVE, and after LOAD it remained ineligible:

```text
[MOVEEVENT] EXIT-PREFLIGHT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
[MOVEEVENT] EXIT ... tile=738 ... status=NO_ELIGIBLE event=79 eligible=0 ...
```

Combined with the earlier opposite-direction test where post-SAVE script mutations were rolled back and became executable again:

```text
script/event mutation after SAVE -> rolled back by LOAD
script/event mutation before SAVE -> preserved by LOAD
```

Detailed records:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md)

### Historical V3 save-world boundary (superseded by V4)

Persisted now:

```text
settled player pose
EspNativeGameplayPlayerState
player-resource consumed overlay
EspMapScriptState event states + removed-command bits
```

Still fresh after LOAD:

```text
line open/locked state and texture variants
automap reveal state
monster state/position/activation/combat consequences
destructibles
gameplay RNG
```

Continue one owner at a time; never dump a raw runtime/legacy object graph.

The long-session V3 hardware run stabilized at:

```text
heap8=17724
largest8=8692
```

That remained stable across the final LOAD/replay segment. Fragmentation/headroom is still below the advisory target and remains a review item, but the run does not show a new per-LOAD leak.

### V4 line section — REAL-CYD PASS

Current writes use:

```text
DRPGSAV4
version=4
recordBytes=1212
v1/v2/v3 read compatibility=retained
```

V4 appends one bounded `EspMapLineCheckpointSnapshot` to the proven v1/v2/v3 sections. Entrance uses 60 bytes per line bitset for 480 lines. The persisted semantics are:

```text
open bits
locked bits
mutable locked/unlocked texture-10 variant bits
line/runtime identity + semantic fingerprints
```

The real-CYD load rebuilt canonical Entrance first, then restored:

```text
[MAPLINECHECKPOINT] RESTORE ... open=0 locked=6 texture10=1
                    lineFNV=69334d90 textureFNV=bda09634
[NATIVESAVE] LOAD ... version=4 bytes=1212
             lines=restored/480/60B/open0/locked6/tex101/69334d90/bda09634
```

The soldier-controlled door at line 352 then opened with `locked=0` without replaying the soldier unlock script, proving the saved unlock and matching texture state survived LOAD.

The first V4 hardware attempt triggered a `loopTask` stack canary while entering STAT. The final fix moved the large checkpoint read workspace to bounded static storage and removed unnecessary full-record stack copies. Hardware now reaches the STATUS SAVE/LOAD UI, performs V4 LOAD, reprimes the session and continues gameplay without reset.

Post-LOAD invariants:

```text
shapeData == NULL
mediaTexels == NULL
heap8=14076
largest8=6644
```

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md)

### V5 action-removal section — REAL-CYD PASS

Current writes use:

```text
DRPGSAV5
version=5
recordBytes=1356
v1/v2/v3/v4 read compatibility=retained
max removed payload=128 B / 1024 sprites
Entrance removed bytes=43 / 344 sprites
```

V5 appends one pointer-free `EspNativeGameplayActionRemovedSnapshot` containing
runtime/map identity, sprite count, removed count/bytes, semantic FNV and the
removed-sprite bitset. Restore validates all identity/count/FNV/tail invariants
and mutates only the compact action-engine removal owner.

Hardware SAVE witness after clearing fire sprite 74:

```text
[ACTIONENGINE] FIRE-COMMIT ... sprite=74 ammo=10->9 ...
[NATIVESAVE] SAVE ... version=5 bytes=1356
             pos=288,1248 angle=0
             playerFNV=15cb16e4
             resources=5/43B
             script=93/265/81B/26f291e3
             lines=480/60B/open1/locked6/tex101/c50b0721/bda09634
             actionRemoved=1/43B/a54be373
```

Hardware LOAD witness:

```text
[NATIVESAVE] REPRIME-HUD ... refresh=pending clear=ready ...
[NATIVESAVE] LOAD ... version=5 bytes=1356
             pos=288,1248 angle=0
             actionRemoved=restored/1/43B/a54be373
[ENGINESESSION] RESUME checkpoint=restored freshFirstFrame=skipped dynamicLines=gameplay-wrapper
[DYNAMICLINES] FRAME angle=0 open=1 ... render=ok immutableRuntime=yes
[RESIDENTGAMEPLAY] READY map=current entry=checkpoint-resume ...
[ENGINESESSION] READY map=1 angle=0 ... TURN+MOVE=armed
```

The real-CYD visual result matched the semantic snapshot: the fire cleared before
SAVE remained absent after LOAD, while another fire that was never cleared
remained lit.

The hardware investigation also established the permanent checkpoint-resume
ordering. A restored checkpoint does not replay the historical fresh-map first
frame. It restores settled HUD owners, uses the production gameplay renderer to
prime caches (and therefore dynamic lines), then one-shot admits resident
gameplay after HUD + resident cache + large cache are ready. Fresh startup still
requires the normal first-frame witness.

The V5 memory correction shares one static read/write save workspace and streams
V5 write verification in a 64-byte compare buffer. This recovered startup DRAM
after the first V5 candidate caused an OOM/reset while loading mappings.

Exact final code/CI boundary:

```text
d65e5b9be9947e92c700b2296790b003ff7b7df0
esp32-cyd #387 / 35704985512 = SUCCESS
REAL-CYD = PASS
```

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md)

### Current save-world boundary

Persisted now:

```text
settled player pose
EspNativeGameplayPlayerState
player-resource consumed overlay
EspMapScriptState event states + removed-command bits
EspMapLineState open/locked state
EspMapLineTextureState locked/unlocked texture variants
EspNativeGameplayActionEngine action-owned removed-sprite overlay
```

Still fresh after LOAD:

```text
automap reveal state
monster mutable state/positions/activation/combat consequences
full entity/sprite dynamic state and transformed definitions
destructible transformed state such as crate -> pickup
power-coupling health/death globals
persistent GSprites
ceiling/floor color
other legacy player metadata not yet owned natively
```

Gameplay RNG is rebuilt fresh, but the recovered original save format does not
serialize RNG state, so this is not a missing-vs-original save field.

Continue one owner at a time; never dump a raw runtime/legacy object graph.

## HUB/action-feedback visual ownership fix — REAL-CYD PASS

During the save-v2 hardware test, a pickup message could expire while HUB owned the framebuffer. The original bug left a stale `Got ...` fragment over the MENU area and could produce:

```text
[HUB] CLOSE ... menuUnderlayRestore=FAILED ... exactHud=NO
[RESIDENTGAMEPLAY] HUB-RECOVER ...
```

The branch adds a bounded ownership gate so action-feedback / viewport-flash expiry does not restore world pixels while HUB owns the framebuffer:

```text
8b7a4c04dee1622954f2ea453ca1b15792fbf6fa
5a1020fd5d160c111ff09ecb8a480f37ea8d0578
CI #267 SUCCESS
```

The timer still uses real elapsed time and resumes after HUB closes. The user reproduced the original pickup-message/HUB scenario on the real CYD and confirmed the stale fragment has disappeared. This fix is hardware-valid at `5a1020fd5d160c111ff09ecb8a480f37ea8d0578`.

## CHANGEMAP candidate on this branch

Entrance level-exit recovery:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
```

The branch owns a bounded WAIT_STATS/ACK transition handoff and target resident/session reconstruction with fail-closed errors. Keep it intact. It still needs its own real-CYD level-exit PASS.

## Rotation no-turn parity — REAL-CYD PASS

Legacy/J2ME parity is restored at code boundary:

```text
b548321f477626777800371f0f82a9f3c2375bd9
```

`DoomCanvas_finishMovement()` advances the gameplay turn; `DoomCanvas_finishRotation()` does not. The native observer now mirrors that split: angle-only changes update the settled-view baseline but do not schedule monster AI.

The user confirmed the corrected behavior on the real classic CYD. The supplied hardware run also reconfirmed normal turn scheduling for actual gameplay actions:

```text
reason=PLAYER_ATTACK -> scheduled
reason=MOVE          -> scheduled
```

Detailed record:

- [`MILESTONE_NATIVE_ROTATE_NO_TURN.md`](MILESTONE_NATIVE_ROTATE_NO_TURN.md)

## Preferred next milestone

The bounded native Automap core has passed on the real CYD. The next major
gameplay candidate is the existing **CHANGEMAP / Entrance level-exit
transition**.

The smallest useful hardware proof is:

```text
eligible Entrance exit event
 -> SAVEGAME / target placement semantics
 -> CHANGEMAP request
 -> stats handoff when required
 -> requested-map rebuild
 -> target player/view placement
 -> resident gameplay re-arm
```

Keep remaining Automap parity separate: PASS_TURN/other normal playing actions
while the map is open, hardware execution of `EV_GIVEMAP`, and save persistence
of reveal state are not claimed by the current Automap milestone.

Separate pending work still includes the mixed physical/touch SAVE cursor
regression and unrelated deferred gameplay families.

## Secret-door multi-line SELECT transaction

The hidden door on `/intro.bsp` is a useful legacy case because one SELECT event owns two separate line mutations rather than one. Hardware recovered the exact event as tile `195`, event `10`, with two eligible opcode-15 (`EV_OPENLINE`) commands targeting lines `471` and `470`.

The permanent native SELECT rule is therefore not “exactly one door command”. It is:

- the eligible event must remain a pure line-command family;
- the complete batch is preflighted before the first mutation;
- at most eight lines are allowed, matching `openDoors[8]`;
- commit occurs in event order with exact line/script removed-bit rollback available;
- mixed opcode families and unsupported sequential duplicate-line semantics remain fail-closed.

The real CYD produced:

```text
[ACTION] DOOR-BATCH event=10 count=2 status=OK [0]line=471/op=15/open=0->1/removed=0->1 [1]line=470/op=15/open=0->1/removed=0->1
[DYNAMICLINES] FRAME angle=64 open=4 ... render=ok immutableRuntime=yes
```

Both lines reported `DOORANIM SNAP` because their immutable flags do not mark regular animated doors. The visual/world result was correct: the secret door opened, the player crossed it, and later movement over tile `195` reported `NO_ELIGIBLE`, proving the remove-if-handled state was committed for both commands.

See `MILESTONE_NATIVE_SECRET_DOOR_BATCH.md` for the exact hardware evidence and transaction boundary.

## Intro display-fit contract

The classic CYD uses a 160x120 logical framebuffer while the original intro composition assumes a 128x128 story space. Hardware review established that one fit rule does not look correct for every intro element.

The permanent split is:

- ordinary story-page image geometry: centered 120x120 aspect-preserving viewport;
- scrolling starfield: full 160x120;
- dedicated animated story scene: full 160x120;
- narrative glyphs: centered 156x120 soft-wide mapping;
- no intermediate framebuffer.

This keeps the animation visually full on the 4:3 CYD while avoiding the slightly over-stretched text produced by a full 160-pixel text mapping. The real-CYD visual result on `0d21332bd2524bca73d5284f70e053ca8ba6430d` was explicitly accepted as correct.

See `MILESTONE_NATIVE_INTRO_DISPLAY_POLISH.md` for the exact geometry and hardware boundary.

## Recent milestone index

- [`MILESTONE_NATIVE_AUTOMAP.md`](MILESTONE_NATIVE_AUTOMAP.md)
- [`MILESTONE_MAIN_MENU_LOAD.md`](MILESTONE_MAIN_MENU_LOAD.md)
- [`MILESTONE_NATIVE_INTRO_DISPLAY_POLISH.md`](MILESTONE_NATIVE_INTRO_DISPLAY_POLISH.md)
- [`MILESTONE_NATIVE_SECRET_DOOR_BATCH.md`](MILESTONE_NATIVE_SECRET_DOOR_BATCH.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V6_CRATE_TRANSFORMS.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V6_CRATE_TRANSFORMS.md)

- [`MILESTONE_NATIVE_CRATE_SUBTYPE2.md`](MILESTONE_NATIVE_CRATE_SUBTYPE2.md)

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V5_ACTION_REMOVALS.md)

- [`MILESTONE_NATIVE_PLAYER_HIT_FEEDBACK.md`](MILESTONE_NATIVE_PLAYER_HIT_FEEDBACK.md)
- [`MILESTONE_NATIVE_ROTATE_NO_TURN.md`](MILESTONE_NATIVE_ROTATE_NO_TURN.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_WEAPON_SELECT.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_INVENTORY_LIST.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_TOUCH_UI.md)
- [`MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md`](MILESTONE_NATIVE_GAMEPLAY_HUB_V2.md)
- [`MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md`](MILESTONE_NATIVE_MONSTER_ACTIVE_SEQUENCE.md)
- [`MILESTONE_NATIVE_MAP_FLASH_REUSE.md`](MILESTONE_NATIVE_MAP_FLASH_REUSE.md)
- [`MILESTONE_NATIVE_MAP_FLASH_BACKING.md`](MILESTONE_NATIVE_MAP_FLASH_BACKING.md)

## Current intentionally incomplete families

See `PORTING_STATUS.md` for the authoritative list. Important current boundaries include:

```text
save-v6 mutable-world sections beyond each validated owner
CHANGEMAP hardware level-exit validation
audio
password late presentation cleanup replay
GIVEMAP hardware execution + remaining Automap action parity
Automap reveal-state checkpoint persistence
CHECK_KEY production route
HUB Notebook activation / consumable use / Options / store
remaining advanced combat/monster/special-death families
```

## Development workflow

```text
recover true main + docs
 -> choose one bounded native owner/family
 -> recover exact legacy behavior where relevant
 -> design a permanent compact API
 -> keep unrelated families fail-closed
 -> commit + push agent/*
 -> build esp32-cyd in CI
 -> test on real CYD
 -> Serial is truth
 -> fix failures directly
 -> after PASS, docs-only tail
 -> merge-ready
```

Never merge into `main` without explicit user request.
