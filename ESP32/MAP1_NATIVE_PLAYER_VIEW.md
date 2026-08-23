# ESP32 native Junction player/view application milestone

Branch: `agent/esp32-native-player-view`

Base merged `main`:

```text
PR   = #69 — native Junction spawn projection
main = 992f38374840113409e776fb82ce57ab014607e5
```

Hardware-tested firmware:

```text
fe1630ad5618dfd35bbbc555de8f9762d0b046f8
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

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

This milestone applies that already-validated projection into the first permanent active native player/view owner. It still does not mutate legacy DoomCanvas, Render, Hud, Game or Player state and still does not execute facing lookup, Player_setup, tile-enter or ST_PLAYING.

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

This milestone owns the first eight placement fields in native state and represents `Hud.isUpdate=true` as `hudRefreshPending=1`. Everything after that remains pending.

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
EspPlayerViewState = 44 B
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

## Hardware-proven real Junction owner

Real CYD:

```text
stateBytes=44
stateFNV=d1131d18
view=992/1888/36
viewAngle=64
dest=992/1888
destAngle=64
viewZOld=4
targetMap=9
gameplayLoadMapId=2
loadType=0
active=1
spawnApplied=1
```

`d1131d18` is now a hardware canon for the 44-byte native player/view ABI.

Pending follow-ups remain exact:

```text
hudRefresh=1
facingRefresh=1
playerSetup=1
tileEnter=1
hudApplied=no
facingApplied=no
playerSetupApplied=no
tileEnterApplied=no
```

## Hardware-proven packed override application

The temporary probe also applies the already hardware-proven synthetic packed override:

```text
sourceSpawnParam=00030167
view=480/736/36
viewAngle=192
dest=480/736
destAngle=192
stateFNV=9ed47d08
sourceProjectionFNV=e0a5110b
```

`9ed47d08` is now a hardware canon. The final PARK is restored to the real Junction placement.

## Hardware-proven fail-closed boundary

Real CYD:

```text
nullSpawn=1
inactive=1
badGeometry=1
badPending=1
repeat=1
repeatAtomic=yes
reset=1
stateAtomic=yes
```

The once-only active-owner rule is hardware-proven: a second application is refused without modifying the live native view owner.

## Resident integrity

Complete Junction residency stayed byte-exact:

```text
snapshotFNV=bc9071e9->bc9071e9
targetLeftResident=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

The resident map remains Junction throughout the player/view apply/reset/override/reapply sequence.

## RAM proof

Real CYD:

```text
heap8=72956->72956
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

The 44-byte owner is static/caller-independent state and adds no persistent heap allocation.

Stable post-probe heartbeat:

```text
heap=138720
heap8=72956
largest8=34804
SD=ready
VIDEO=ready
CORE=ready
```

The absolute heap baseline is build-context dependent; the important same-build proof is zero delta across the player/view application.

## Legacy / framebuffer integrity

Same-probe witnesses:

```text
placementFNV=5d1076bf->5d1076bf
playerFNV=a1725bcb->a1725bcb
frameFNV=a3e3cc8e->a3e3cc8e
legacyRuntimeClear=yes
DoomCanvasMutation=no
GameMutation=no
PlayerMutation=no
RenderMutation=no
HudMutation=no
```

These witness hashes are same-build equality checks, not cross-build canons.

Legacy topology remains closed:

```text
Game.entities=0
Game.monsters=0
```

## Final hardware PARK

Real CYD:

```text
state=9
page=3
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativeSpawnState=yes
nativePlayerView=yes
spawnAppliedNative=yes
legacySpawnApplied=no
hudRefreshPending=yes
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

This is the first hardware-proven active native player/view owner on the committed Junction map.

## Hardware canons added by this milestone

```text
EspPlayerViewState bytes = 44
Junction player/view FNV = d1131d18
packed override view FNV = 9ed47d08

view/dest X = 992
view/dest Y = 1888
viewZ       = 36
angle       = 64
viewZOld    = 4
```

Inherited canons remain:

```text
Junction spawn FNV   = ba6af4a7
packed spawn FNV     = e0a5110b
Junction snapshotFNV = bc9071e9
Junction heap        = 10540 B
```

## Boundary after PASS

Native ownership now includes a live player/view state on committed Junction while presentation/gameplay initialization remains explicitly separate.

Still intentionally outside:

```text
actual stats-menu rendering/input
HUD refresh consumption
native facing-entity query
Player_setup-equivalent fresh-map initialization
initial tile-enter event execution
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The player/view owner is intentionally not reset by `EspMapResidentLifecycle_resetAll()`: player state and map arena have different lifetimes. A future transition orchestrator must explicitly reset/reapply player placement.

The next milestone must be selected only after merge and recovery from true `main`. The natural next small boundaries are explicit HUD-refresh consumption and compact-topology facing lookup; do not collapse Player_setup, tile-enter and ST_PLAYING into the same milestone without a fresh legacy audit.

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
