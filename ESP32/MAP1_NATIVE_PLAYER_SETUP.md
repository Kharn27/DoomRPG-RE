# ESP32 native Junction fresh-map Player_setup milestone

Branch: `agent/esp32-native-player-setup`

Base merged `main`:

```text
PR   = #71 — native post-spawn HUD refresh
main = 02b7f143a12e6df86ada094af10ef580ad572aad
```

Hardware-tested firmware:

```text
d808d895e97daef5d454ca06d5fda1738e99b147
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

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

`Player_restoreWeapons()` is intentionally not folded into this milestone. A nonzero `disabledWeapons` can alter `weapons`, clear `disabledWeapons`, select another weapon, and request a view refresh. Until a native inventory/weapon owner exists, that branch remains fail-closed.

The real fresh-run CYD path proved:

```text
disabledWeapons=0
weaponRestorePerformed=0
```

The probe reads this legacy value only as a hardware precondition. Permanent APIs remain legacy-engine free.

## Correct ordering boundary

The fresh-map sequence remains:

```text
placement
Hud.isUpdate=true                         [owned]
first checkFacingEntity()                 [transient, deliberately unowned]
Player_setup()                            [THIS MILESTONE, hardware-proven]
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

Hardware-proven classic-ESP32 ABI:

```text
EspPlayerFreshMapState = 24 B
persistent heap = 0 B
```

Real CYD state:

```text
levelStartTimeMs        = 27538   # same-run witness, dynamic
moves                   = 0
xpGained                = 0
berserkerTics           = 0
familiarActive          = 0
notebookEmpty           = 1
weaponRestorePerformed  = 0
targetMapId             = 9
gameplayLoadMapId       = 2
loadType                = 0
setupApplied            = 1
active                  = 1
```

`levelStartTimeMs` mirrors the recovered 32-bit `DoomRPG_GetUpTimeMS()` write. Hardware proved `startExact=yes`: the parked owner contains the exact value sampled immediately before routing.

The empty `Player.NotebookString` state is represented compactly by `notebookEmpty=1`. No permanent 512-byte notebook buffer is allocated merely to represent an empty string.

## Fingerprints

The raw whole-state FNV includes dynamic `levelStartTimeMs`, so it is a same-run witness only:

```text
stateFNV=d0ab146e
startMs=27538
startExact=yes
```

For a deterministic cross-run semantic fingerprint, the probe normalizes only `levelStartTimeMs` to zero before hashing:

```text
setup semanticFNV=3b27c6a1
```

`3b27c6a1` is now the hardware canon for the fresh-map Player_setup semantic state.

## Player/view ownership transfer

`EspPlayerViewState` remains 44 B. Primitive:

```text
EspPlayerView_consumePlayerSetup(targetMapId, gameplayLoadMapId, loadType)
```

Hardware proved exactly:

```text
before FNV=d17fa0d1
 after FNV=c21fba3c
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=1
placementExact=yes
```

Only `playerSetupPending` is consumed. `c21fba3c` is now the hardware canon for the post-setup 44-byte player/view owner.

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

## Hardware fail-closed proof

Real CYD:

```text
nullView=1
nullHud=1
nullOutput=1
inactive=1
loadType=1
hudPending=1
missingFacing=1
missingSetup=1
missingTile=1
hudMismatch=1
weaponRestore=1
reset=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

The `weaponRestore=1` gate proves a nonzero `disabledWeapons` context is rejected rather than silently approximated.

## Resident integrity

Real CYD:

```text
snapshotFNV=bc9071e9->bc9071e9
targetLeftResident=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

Junction remained byte-exact.

## RAM proof

Real CYD:

```text
heap8=72900->72900
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat after the probe:

```text
heap=138664
heap8=72900
largest8=34804
```

Absolute heap is build-context dependent. The hardware proof is the zero same-build delta.

## Legacy / framebuffer integrity

Same-build witnesses:

```text
placementFNV=5d1076bf->5d1076bf
playerSetupFNV=ea247b9a->ea247b9a
frameFNV=9eb7ce0f->9eb7ce0f
legacyRuntimeClear=yes
DoomCanvasMutation=no
GameMutation=no
PlayerMutation=no
RenderMutation=no
HudMutation=no
```

These witness hashes are same-build equality checks, not cross-build canons.

## Final hardware PARK

Real CYD:

```text
state=9 / ST_INTRO
page=3
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativeHudRefresh=yes
nativePlayerSetup=yes
setupApplied=yes
hudDirty=yes
facingPending=yes
playerSetupPending=no
tileEnterPending=yes
finishRotationPending=yes
finalFacingPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Hardware canons added by this milestone

```text
EspPlayerFreshMapState bytes = 24
setup semanticFNV            = 3b27c6a1
post-setup PlayerView FNV    = c21fba3c
persistent heap              = 0 B
```

Same-run dynamic witness:

```text
stateFNV=d0ab146e at startMs=27538
```

Inherited canons remain:

```text
post-HUD PlayerView FNV=d17fa0d1
HUD owner FNV=6965ee06
Junction snapshotFNV=bc9071e9
Junction resident heap=10540 B
```

## Boundary after PASS

Native ownership now includes the fresh-map `Player_setup()` reset semantics and a real per-level start clock without touching the legacy `Player` object.

The next exact operation is the **initial tile-enter execution** at the hardware-proven Junction player position `(992,1888)`. `finishRotation()`/its second tile execution, durable final facing, and `ST_PLAYING` remain later boundaries until separately audited.

Still intentionally outside:

```text
actual HUD rendering / dirty consumption by renderer
weapon restore/select ownership for disabledWeapons!=0
initial tile-enter execution
finishRotation-equivalent orientation preparation
second tile/facing event
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

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

## Merge recommendation

```text
MERGE agent/esp32-native-player-setup
```

Hardware-tested firmware:

```text
d808d895e97daef5d454ca06d5fda1738e99b147
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
