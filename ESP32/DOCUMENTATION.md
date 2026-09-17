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
main at branch creation = 9b085a9d8ed254edc98463f33f6d1534215326b9
current main = 9b085a9d8ed254edc98463f33f6d1534215326b9
branch = agent/esp32-native-gameplay-changemap-transition
hardware-tested code boundary = 1c2cbe13d6001459eb8679f7262e495a51585476
status = REAL-CYD NATIVE CHECKPOINT SAVE/LOAD V1 PASS
```

GitHub Actions `esp32-cyd` run #258 / run ID `35194006759` passed on the exact hardware-tested code SHA.

Latest milestone:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md)

The branch also contains the production CHANGEMAP candidate. Its dedicated level-exit hardware test is still pending; checkpoint validation does not imply CHANGEMAP PASS.

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

Canonical retained fingerprints:

```text
mapStateFNV=cd99b98e
scriptFNV=f9e3d9df
lineFNV=e5e74861
textureFNV=f1fc1875
automapFNV=669b1aa7
topologyFNV=3f321e43
```

Resident cache baseline:

```text
owner=23592 B
payload=19456 B
range records=288 x 12 B
resident entry slots=24
large exact range=2048 B
```

## Current native gameplay frontier

The real-CYD-owned engine now includes native movement/collision, event-first SELECT, bounded event/script execution, dialog, dynamic doors/lines, mutable line textures, shared PlayerState, pickups/resources, hazards, native weapon rendering/control/combat, monster state/position/activation/movement/attack families, raw-flash requested-map backing, HUB INV/WPN/STAT and bounded checkpoint save/load.

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

INV projects Notebook, carried items, Credits and keys. WPN is the 4x3 direct-touch normal arsenal grid; familiar IDs 9..11 remain excluded from the normal weapon grid. STAT currently owns the bounded SAVE/LOAD controls.

## Native checkpoint save/load v1

Hardware-proven slot:

```text
/sd/DoomRPG-ESP32.sav
magic=DRPGSAV1
version=1
recordBytes=132
```

Atomic write contract:

```text
temp write -> reread/validate -> old primary to backup -> temp to primary -> reread/validate
```

Persisted content:

```text
saved BSP sourceBytes + sourceCRC32
immutable runtime FNV
map/load identity
full settled EspPlayerViewState
full EspNativeGameplayPlayerState
player FNV
record CRC32
```

The final LOAD path rebuilds the BSP from native PAK data, verifies immutable identity, restores the player and pose, then restores the two semantic HUD reprime owners before configuring the gameplay session.

Key real-CYD witness:

```text
[NATIVESAVE] REPRIME-HUD ... refresh=pending clear=ready mutation=owners-only turn=no
[NATIVESAVE] LOAD ... pos=416,1696 angle=128 playerFNV=363261d1
[ENGINESESSION] HUD ... armor=8/20 weapon=2 ammo=8
[ENGINESESSION] SPRITES ...
[MAPFLASH] REUSE HIT ...
[MAPFLASH] ARM ... resident=1
[ENGINECACHE] PRIMED ...
[RESIDENTGAMEPLAY] READY ...
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
```

A stronger rollback test acquired Fire Ext, ammo and a Small Medkit after SAVE. LOAD restored the prior player root and Fire Ext ownership disappeared, proving exact player rollback rather than a visual-only respawn.

Multiple completed LOAD cycles did not show a fixed monotonic per-load heap loss. Continue to watch `heap8` and `largest8`, especially as further lazy gameplay owners are exercised.

Detailed record:

- [`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V1.md)

### V1 limitation

The current log states `world=fresh-rebuild`. This means mutable map-local owners are reconstructed fresh. The player/pose checkpoint is durable, but consumed pickups, script state, lines/textures, automap, monsters, destructibles and RNG are not yet serialized.

The next save-v2 work must therefore be sectioned owner-by-owner instead of dumping the whole runtime.

## CHANGEMAP candidate on this branch

Entrance level-exit recovery:

```text
SAVEGAME -> /junction.bsp, targetMapId 9, savePos 992,1888 angle 64
CHANGEMAP -> /junction.bsp, targetMapId 9, showStats 1, spawnParam 0
```

The branch owns a bounded WAIT_STATS/ACK transition handoff and target resident/session reconstruction with fail-closed errors. Keep it intact while save persistence advances. It still needs its own real-CYD level-exit PASS.

## Preferred next save-v2 section

Persist the native player-resource consumed overlay first.

Current owner characteristics:

```text
Entrance sprites=344
consumed bitset bytes=43
owner is map/runtime-identity scoped
owner is freed by session reset and recreated after resident rebuild
```

The permanent API should export/import bytes explicitly, validate map/runtime/sprite identity, and never expose/save an owner pointer.

Hardware proof should establish both directions:

```text
pickup consumed before SAVE -> remains consumed/hidden after LOAD
pickup consumed after SAVE -> reappears after LOAD
player root/pose -> exact saved state
unowned world sections -> still explicitly fresh
```

After that PASS, choose the next compact mutable owner (script/event state, line state/textures, automap, monster state/position/RNG, etc.) as a separate milestone.

## Recent milestone index

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
save-v2 mutable-world sections beyond each validated owner
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
