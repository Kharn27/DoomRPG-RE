# ESP32 native Junction player/view application milestone

Branch: `agent/esp32-native-player-view`

Base merged `main`:

```text
PR   = #69 — native Junction spawn projection
main = 992f38374840113409e776fb82ce57ab014607e5
```

Firmware candidate:

```text
fe1630ad5618dfd35bbbc555de8f9762d0b046f8
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

## Objective

PR #69 hardware-proved the deterministic fresh-map spawn projection for committed Junction:

```text
EspPlayerSpawnState = 24 B
stateFNV=ba6af4a7
world=992/1888
angle=64
viewZ=36
viewZOld=4
loadType=0
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
spawnApplied=no
```

This milestone applies that already-validated projection into the first permanent native player/view owner. It still does not mutate legacy DoomCanvas, Render, Hud, Game or Player state and still does not execute facing lookup, Player_setup, tile-enter or ST_PLAYING.

## Recovered legacy boundary

Recovered `Game_spawnPlayer()` writes, in order:

```text
DoomCanvas.viewX = DoomCanvas.destX = x*64+32
DoomCanvas.viewY = DoomCanvas.destY = y*64+32
DoomCanvas.viewZ = 36
DoomCanvas.destAngle = angle
DoomCanvas.viewAngle = angle
Render.viewZOld = 4
Hud.isUpdate = true
DoomCanvas_checkFacingEntity(...)
if (!Game.isLoaded):
    Player_setup(...)
    Game_executeTile(...)
```

The previous milestone projected the coordinate fields. This milestone owns the first eight placement fields in native state and represents `Hud.isUpdate=true` as `hudRefreshPending=1`. Everything after that remains pending.

## Permanent native owner

Files:

```text
ESP32/include/esp_player_view_state.h
ESP32/src/esp_player_view_state.c
```

API:

```text
EspPlayerView_reset()
EspPlayerView_isReady()
EspPlayerView_view()
EspPlayerView_applySpawn()
```

State:

```text
EspPlayerViewState = 44 B expected classic-ESP32 ABI
persistent heap = 0 B
```

Fields:

```text
int32 viewX
int32 viewY
int32 viewZ
int32 viewAngle
int32 destX
int32 destY
int32 destAngle
int32 viewZOld

targetMapId
gameplayLoadMapId
loadType
spawnApplied
hudRefreshPending
facingRefreshPending
playerSetupPending
tileEnterPending
active
```

The eight placement values deliberately keep legacy 32-bit width instead of prematurely compressing future gameplay coordinates.

## Apply contract

`EspPlayerView_applySpawn()` is once-only while the owner is active.

It requires a coherent active `EspPlayerSpawnState`:

```text
fresh-map loadType=0
valid target map ID
gameplayLoadMapId in 1..32
tile < 32/32
tileIndex < 1024
tileIndex == tileY*32 + tileX
world == tile*64 + 32
viewZ == 36
viewZOld == 4
facing/setup/tile-enter pending = 1
```

Header source requires:

```text
sourceSpawnParam=0
overrideUsed=0
```

Packed override source additionally re-decodes the original parameter and requires:

```text
tileX = sourceSpawnParam & 31
tileY = (sourceSpawnParam >> 5) & 31
angle = (sourceSpawnParam >> 10) & 255
```

This prevents an internally coherent coordinate set from being paired with the wrong packed parameter.

## Expected real Junction owner

From hardware-proven spawn state `ba6af4a7`:

```text
viewX=992
viewY=1888
viewZ=36
viewAngle=64

destX=992
destY=1888
destAngle=64

viewZOld=4

targetMapId=9
gameplayLoadMapId=2
loadType=0
spawnApplied=1
hudRefreshPending=1
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
active=1
```

Static little-endian 44-byte FNV prediction:

```text
d1131d18
```

This remains a candidate until the real CYD confirms it.

## Packed override application proof

The temporary probe also applies the already hardware-proven synthetic packed override:

```text
sourceSpawnParam=00030167
view/dest X=480
view/dest Y=736
angle=192
viewZ=36
viewZOld=4
```

Expected 44-byte FNV:

```text
9ed47d08
```

The final PARK is restored to the real Junction placement, not the synthetic override.

## Temporary hardware probe

Files:

```text
ESP32/include/native_junction_player_view_probe.h
ESP32/src/native_junction_player_view_probe.c
```

The probe waits for `Esp32JunctionSpawnProbe_isDone()` and obtains the exact parked 24-byte spawn state through `Esp32JunctionSpawnProbe_getState()`.

Sequence:

```text
reset native player/view owner
prove null/inactive/bad-geometry/bad-pending refusal
apply real Junction spawn
prove repeat apply refused and state atomic
reset
apply packed override and verify FNV
reset
apply real Junction spawn again
PARK with real native player/view owner active
```

## Requested hardware integrity proof

Complete Junction residency must remain byte-exact:

```text
snapshotFNV=bc9071e9 -> bc9071e9
payload=10410
entities=30
enemies=0
destructibles=3
PAK closed
```

Player/view owner itself must cost:

```text
persistent heap = 0 B
heap8 delta = 0
largest8 delta = 0
```

Legacy witnesses must remain exact:

```text
DoomCanvas placement fields unchanged
Render.viewZOld unchanged
Hud.isUpdate unchanged
Game spawn/load fields unchanged
Player witness unchanged
framebuffer unchanged
legacy Render runtime clear
legacy entities=0
legacy monsters=0
```

## Expected Serial family

```text
[JUNCTIONVIEWPROBE] ARMED ...

=== Doom RPG ESP32-native Junction player/view application ===
[JUNCTIONVIEWPROBE] CONTRACT ...

[JUNCTIONVIEW] READY stateBytes=44 stateFNV=d1131d18 view=992/1888/36 angle=64 dest=992/1888 angle=64 viewZOld=4 targetMap=9 gameplayLoadMapId=2 loadType=0 active=1 spawnApplied=1

[JUNCTIONVIEW] FOLLOWUPS hudRefresh=1 facingRefresh=1 playerSetup=1 tileEnter=1 hudApplied=no facingApplied=no playerSetupApplied=no tileEnterApplied=no

[JUNCTIONVIEW] OVERRIDE param=00030167 view=480/736/36 angle=192 dest=480/736 angle=192 stateFNV=9ed47d08 sourceProjectionFNV=e0a5110b

[JUNCTIONVIEW] FAILCLOSED nullSpawn=1 inactive=1 badGeometry=1 badPending=1 repeat=1 repeatAtomic=yes reset=1 stateAtomic=yes

[JUNCTIONVIEW] RESIDENT snapshotFNV=bc9071e9->bc9071e9 targetLeftResident=yes payload=10410 entities=30 enemies=0 destructibles=3 packClosed=yes

[JUNCTIONVIEW] RAM heap8=...->... delta=0 largest8=...->... delta=0 persistentHeapBytes=0

[JUNCTIONVIEW] LEGACY placementFNV=...->... playerFNV=...->... frameFNV=...->... legacyRuntimeClear=yes DoomCanvasMutation=no GameMutation=no PlayerMutation=no RenderMutation=no HudMutation=no

[JUNCTIONVIEW] PARK state=9 page=3 mapSwapCommitted=yes targetMap=9 junctionResident=yes nativeSpawnState=yes nativePlayerView=yes spawnAppliedNative=yes legacySpawnApplied=no hudRefreshPending=yes facingPending=yes playerSetupPending=yes tileEnterPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes
```

## Strict PASS boundary

A PASS must prove:

```text
EspPlayerViewState bytes=44
real stateFNV=d1131d18
real native view/dest=992/1888 angle=64 z=36 viewZOld=4
override stateFNV=9ed47d08
repeat apply refused atomically
Junction resident snapshot unchanged
heap/largest unchanged
PAK closed
legacy placement/Hud/Player/frame unchanged
native spawn applied=yes
legacy spawn applied=no
hud refresh still pending
facing still pending
Player_setup still pending
tile-enter still pending
ST_PLAYING=no
legacy entities=0
legacy monsters=0
```

## Boundary after PASS

A hardware PASS would establish the first active native player/view state on committed Junction. The next layer should be selected only after merge and repository recovery. The natural next candidates are compact-topology facing query and/or explicit HUD refresh consumption; `Player_setup`, tile-enter and ST_PLAYING should remain separately bounded unless the legacy audit proves otherwise.

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

No local build or hardware PASS is claimed for this candidate.
