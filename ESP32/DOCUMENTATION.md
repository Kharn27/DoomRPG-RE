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
main at branch creation = 23bdd1dfe92f860b62d5d8cede517122ac589464
current main = 23bdd1dfe92f860b62d5d8cede517122ac589464
main merge = PR #139
branch = agent/esp32-native-rotate-no-turn
hardware-tested save-v2 resource boundary = f52d3f272e75ed29f68037fd343e40252d2ec6bf
hardware-tested save-v3 script boundary = fd206c5238ac2db62939d100bf3d08ac39081c69
hardware-tested save-v4 line boundary = 2efb9634ffc1c2fb433c4c3340ff9c722c5c7b4d
hardware-tested current code boundary = 2efb9634ffc1c2fb433c4c3340ff9c722c5c7b4d
merged save-touch cursor fix = f3dd883e937799eb2ad93812982edb1d4a06bcab (CI PASS, real-CYD mixed-input retest pending)
rotation no-turn candidate = b548321f477626777800371f0f82a9f3c2375bd9 (real-CYD pending)
status = REAL-CYD CHECKPOINT V4 PASS; merged mixed-input cursor retest pending; ROTATE no-turn parity candidate pending
```

GitHub Actions `esp32-cyd` run #265 / run ID `35196771704` passed on the exact save-v2 resource boundary. The HUB/action-feedback ownership gate passed CI run #267 / run ID `35198140562` and is also real-CYD validated. Save-v3 script persistence passed CI run #275 / run ID `35199788280` on exact code boundary `fd206c5238ac2db62939d100bf3d08ac39081c69`. Save-v4 line persistence plus the HUB stack fix passed CI run #293 / run ID `35344853078` on exact code boundary `2efb9634ffc1c2fb433c4c3340ff9c722c5c7b4d` and is real-CYD validated. PR #139 merged at `23bdd1dfe92f860b62d5d8cede517122ac589464`; normal `esp32-cyd` run #311 passed on that exact merged main. The mixed physical/touch SAVE cursor fix is therefore merged and CI-proven, but its dedicated real-CYD mixed-input check is still pending.

Latest milestones:

- [`MILESTONE_NATIVE_ROTATE_NO_TURN.md`](MILESTONE_NATIVE_ROTATE_NO_TURN.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V4_LINES.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V3_SCRIPT.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V2_RESOURCES.md)
- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md)

The branch also contains the production CHANGEMAP candidate. Its dedicated level-exit hardware test remains pending; checkpoint validation does not imply CHANGEMAP PASS.

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

The real-CYD-owned engine includes native movement/collision, event-first SELECT, bounded event/script execution, dialog, dynamic doors/lines, mutable line textures, shared PlayerState, pickups/resources, hazards, native weapon rendering/control/combat, monster state/position/activation/movement/attack families, raw-flash requested-map backing, HUB INV/WPN/STAT, bounded checkpoint save/load, resource consumed-overlay persistence, script/event-state persistence, line open/locked + texture-variant persistence, and hardware-proven HUB/world feedback framebuffer ownership gating.

Player/HUB compact roots:

```text
EspNativeGameplayPlayerState = 52 B
EspNativeGameplayHubView = 28 B
```

### HUB

```text
pages = INV | WPN | STAT
MENU underlay = 32x20 RGB565 = 1280 B
world dispatch blocked while HUB active
turn advance disabled while HUB active
```

INV projects Notebook, carried items, Credits and keys. WPN is the 4x3 direct-touch normal arsenal grid; familiar IDs 9..11 remain excluded from the normal weapon grid. STAT owns the bounded SAVE/LOAD controls.

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

### Current save-world boundary

Persisted now:

```text
settled player pose
EspNativeGameplayPlayerState
player-resource consumed overlay
EspMapScriptState event states + removed-command bits
EspMapLineState open/locked state
EspMapLineTextureState locked/unlocked texture variants
```

Still fresh after LOAD:

```text
automap reveal state
monster state/position/activation/combat consequences
destructibles
gameplay RNG
```

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

## Preferred next milestone

Current branch: `agent/esp32-native-rotate-no-turn`.

Legacy/J2ME parity requires rotation in place to be view/facing work only, not turn advancement. The reference split is exact:

```text
DoomCanvas_finishMovement() -> Game_advanceTurn()
DoomCanvas_finishRotation() -> no Game_advanceTurn()
```

Candidate `b548321f477626777800371f0f82a9f3c2375bd9` removes angle-only monster-turn scheduling while still updating the observer baseline. Hardware should see:

```text
[MONSTERTURN] ROTATE-NO-TURN ... legacyAdvance=no
```

and must not see `[MONSTERTURN] SCHEDULE ... reason=ROTATE`. MOVE, PLAYER_ATTACK and PASS_TURN remain turn-producing actions.

Detailed candidate record:

- [`MILESTONE_NATIVE_ROTATE_NO_TURN.md`](MILESTONE_NATIVE_ROTATE_NO_TURN.md)

Also retain one pending regression check from the merged PR review: physical SAVE/LOAD cursor navigation followed by direct touch must dispatch the authoritative row without an extra cursor move. The code is already merged in main, but Serial/hardware truth for that exact mixed-input sequence is still pending.

After these checks, the next separate polish family is player attack feedback: damage message first, then enemy-hit blood pixels if the recovered legacy path supports a clean bounded owner.

The production CHANGEMAP candidate remains present and still requires its own dedicated real-CYD level-exit PASS.

## Recent milestone index

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
save-v4 mutable-world sections beyond each validated owner
CHANGEMAP hardware level-exit validation
audio
password input
GIVEMAP production route
CHECK_KEY production route
HUB Notebook activation / consumable use / Automap / Options / store
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
