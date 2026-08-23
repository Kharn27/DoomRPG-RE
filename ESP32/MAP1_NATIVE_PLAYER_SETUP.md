# ESP32 native Junction fresh-map Player_setup milestone

Branch: `agent/esp32-native-player-setup`

Base merged `main`:

```text
PR   = #71 — native post-spawn HUD refresh
main = 02b7f143a12e6df86ada094af10ef580ad572aad
```

Firmware candidate:

```text
d808d895e97daef5d454ca06d5fda1738e99b147
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

PR #71 hardware-proved the post-spawn HUD ownership transfer and parked the real Junction player/view at:

```text
PlayerView FNV=d17fa0d1
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
HUD owner FNV=6965ee06
```

This milestone owns only the next exact fresh-map legacy operation: `Player_setup()`.

It deliberately does **not** execute the initial tile event, `finishRotation()`, the durable facing query, HUD presentation, or `ST_PLAYING`.

## Recovered legacy semantics

Recovered `Player_setup()` is:

```c
player->time = DoomRPG_GetUpTimeMS();
player->moves = 0;
player->xpGained = 0;
player->berserkerTics = 0;
player->dogFamiliar = NULL;
player->NotebookString[0] = '\0';
if (player->disabledWeapons) {
    Player_restoreWeapons(player);
}
```

`Player_restoreWeapons()` is not folded into this milestone. A nonzero `disabledWeapons` can alter `weapons`, clear `disabledWeapons`, select another weapon, and request a view refresh. Until a native inventory/weapon owner exists, that branch remains fail-closed.

The real fresh-run hardware path is therefore required to present:

```text
disabledWeapons=0
```

The probe reads this legacy value only as a hardware precondition. Permanent APIs remain legacy-engine free.

## Correct ordering boundary

The already-recovered fresh-map sequence remains:

```text
placement
Hud.isUpdate=true                         [owned]
first checkFacingEntity()                 [transient, deliberately unowned]
Player_setup()                            [THIS MILESTONE]
initial Game_executeTile(...)             [deferred]
finishRotation():
  recalc viewSin/viewCos/viewStep         [deferred]
  Game_executeTile(... | 0x400)           [deferred]
  final checkFacingEntity()               [deferred]
```

The first facing value is not observable by `Player_setup()` or the intervening tile execution and is later overwritten by the durable post-rotation facing result. This milestone therefore preserves `facingRefreshPending=1`.

## Permanent fresh-map player session owner

Files:

```text
ESP32/include/esp_player_fresh_map_state.h
ESP32/src/esp_player_fresh_map_state.c
```

Expected classic-ESP32 ABI:

```text
EspPlayerFreshMapState = 24 B
persistent heap = 0 B
```

State:

```text
levelStartTimeMs        dynamic caller-sampled uptime
moves                   0
xpGained                0
berserkerTics           0
familiarActive          0
notebookEmpty           1
weaponRestorePerformed  0
targetMapId             9
gameplayLoadMapId       2
loadType                0
setupApplied            1
active                  1
```

`levelStartTimeMs` is intentionally dynamic. It mirrors the 32-bit `DoomRPG_GetUpTimeMS()` write and is verified against the exact value sampled immediately before routing.

The empty `Player.NotebookString` state is represented compactly by `notebookEmpty=1`. The reusable `EspMapNotebookState` remains caller-owned until a non-empty gameplay NOTE must be materialized; no permanent 512-byte buffer is introduced merely to represent an empty string.

## Deterministic semantic fingerprint

A raw whole-state FNV varies because `levelStartTimeMs` varies. The probe therefore also hashes a copy with only `levelStartTimeMs` normalized to zero.

Expected normalized FNV-1a:

```text
setup semanticFNV = 3b27c6a1
```

The raw `stateFNV` is logged as a same-run witness only.

## Player/view ownership transfer

`EspPlayerViewState` remains 44 B. New primitive:

```text
EspPlayerView_consumePlayerSetup(targetMapId, gameplayLoadMapId, loadType)
```

It is valid only after HUD routing and clears exactly:

```text
playerSetupPending: 1 -> 0
```

while preserving:

```text
hudRefreshPending=0
facingRefreshPending=1
tileEnterPending=1
all placement/load identity unchanged
```

Hardware-proven input FNV from PR #71:

```text
d17fa0d1
```

Expected output FNV:

```text
c21fba3c
```

## Permanent API

```text
EspPlayerFreshMap_reset()
EspPlayerFreshMap_isReady()
EspPlayerFreshMap_view()
EspPlayerFreshMap_prepare()
EspPlayerFreshMap_route()
```

`prepare()` is pure and zeroes caller output on refusal.

`route()` requires:

```text
fresh loadType=0
live post-HUD player/view
live matching HUD owner
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
disabledWeapons=0
```

On success it parks the 24-byte session owner and atomically consumes only `playerSetupPending`.

## Fail-closed cases

The hardware probe verifies:

```text
null player/view
null HUD owner
null output
inactive player/view
nonzero loadType
HUD not yet consumed
missing facing pending bit
missing setup pending bit
missing tile-enter pending bit
HUD identity mismatch
disabledWeapons != 0
repeat live route
reset to empty
```

No refused case may mutate the player/view or HUD owners.

## Temporary hardware probe

Files:

```text
ESP32/include/native_junction_player_setup_probe.h
ESP32/src/native_junction_player_setup_probe.c
```

It arms only after the hardware-proven HUD probe has completed.

Expected decisive Serial:

```text
[JUNCTIONSETUPPROBE] ARMED ...

=== Doom RPG ESP32-native Junction fresh-map Player_setup ===

[JUNCTIONSETUP] READY stateBytes=24 stateFNV=<dynamic> semanticFNV=3b27c6a1 startMs=<runtime> startExact=yes moves=0 xpGained=0 berserker=0 familiar=0 notebookEmpty=1 weaponRestore=0 targetMap=9 gameplayLoadMapId=2 loadType=0 active=1 setupApplied=1

[JUNCTIONSETUP] PLAYER viewBytes=44 beforeFNV=d17fa0d1 afterFNV=c21fba3c hudPending=0 facingPending=1 playerSetupPending=0 tileEnterPending=1 placementExact=yes

[JUNCTIONSETUP] ORDER hudOwned=yes firstFacingTransientUnowned=yes playerSetupApplied=yes tileEnterDeferred=yes finishRotationDeferred=yes finalFacingDeferred=yes disabledWeapons=0

[JUNCTIONSETUP] FAILCLOSED nullView=1 nullHud=1 nullOutput=1 inactive=1 loadType=1 hudPending=1 missingFacing=1 missingSetup=1 missingTile=1 hudMismatch=1 weaponRestore=1 reset=1 prepareAtomic=yes repeat=1 repeatAtomic=yes

[JUNCTIONSETUP] RESIDENT snapshotFNV=bc9071e9->bc9071e9 targetLeftResident=yes payload=10410 entities=30 enemies=0 destructibles=3 packClosed=yes

[JUNCTIONSETUP] RAM heap8=...->... delta=0 largest8=...->... delta=0 persistentHeapBytes=0

[JUNCTIONSETUP] LEGACY ... unchanged ... PlayerMutation=no ...

[JUNCTIONSETUP] PARK state=9 page=3 mapSwapCommitted=yes targetMap=9 junctionResident=yes nativePlayerView=yes nativeHudRefresh=yes nativePlayerSetup=yes setupApplied=yes hudDirty=yes facingPending=yes playerSetupPending=no tileEnterPending=yes finishRotationPending=yes finalFacingPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes
```

## Strict PASS boundary

A PASS must prove:

```text
EspPlayerFreshMapState bytes=24
semanticFNV=3b27c6a1
startExact=yes
moves=0
xpGained=0
berserker=0
familiar=0
notebookEmpty=1
weaponRestore=0
real disabledWeapons=0
PlayerView d17fa0d1 -> c21fba3c
only playerSetupPending cleared
HUD owner 6965ee06 unchanged
Junction snapshot bc9071e9 unchanged
heap/largest unchanged
PAK closed
legacy Player/Game/Hud/DoomCanvas/Render unchanged
framebuffer unchanged
tile-enter not executed
finishRotation/final-facing not executed
ST_PLAYING=no
legacy entities=0
legacy monsters=0
```

## Boundary after PASS

A hardware PASS establishes the first native per-level player session root with a real level-start clock and exact fresh-map reset semantics.

The next operation is the **initial tile-enter execution** at the already hardware-proven Junction position `(992,1888)` with the recovered facing-direction flag. `finishRotation()`/its second tile execution, final facing, and `ST_PLAYING` remain later boundaries until separately audited.

Mandatory invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map access forbidden
```

Normal hardware environment:

```text
esp32-cyd
```

No local PlatformIO build or hardware PASS is claimed for this candidate.
