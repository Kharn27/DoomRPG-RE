# ESP32 native Junction player spawn/load projection milestone

Branch: `agent/esp32-native-junction-spawn`

Base merged `main`:

```text
PR   = #68 — native committed Junction transition
main = 00268a100c6662cb883f9a02d979b4f29eecbf12
```

Hardware-tested firmware:

```text
08a3a29c5e4e4a64000fa12a877299bbb1e772a0
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

PR #68 left Junction as the live compact native resident map while intentionally retaining player placement as pending:

```text
sourceMap=1 / Entrance
targetMap=9 / Junction
mapSwapCommitted=yes
junctionResident=yes
spawnParam=0 retained
spawnApplied=no
ST_PLAYING=no
```

This milestone owns the deterministic placement part of recovered `Game_spawnPlayer()` for a fresh normal map load. It projects those writes into one small pointer-free native state without mutating legacy `Game`, `Player`, `DoomCanvas`, `Render`, HUD, gameplay or presentation.

It deliberately stops before:

```text
DoomCanvas_checkFacingEntity()
Player_setup()
initial Game_executeTile()
ST_PLAYING
native gameplay rendering
```

## Recovered legacy contract

Header fallback when `game->spawnParam == 0`:

```text
x     = render->mapSpawnIndex % 32
y     = render->mapSpawnIndex / 32
angle = render->mapSpawnDir
```

Packed override when `spawnParam != 0`:

```text
x     = spawnParam & 31
y     = (spawnParam >> 5) & 31
angle = (spawnParam >> 10) & 255
game->spawnParam = 0
```

Common placement writes:

```text
viewX = destX = x * 64 + 32
viewY = destY = y * 64 + 32
viewZ = 36
viewAngle = destAngle = angle
render->viewZOld = 4
```

Legacy then refreshes facing and, for a fresh map, performs `Player_setup()` and the initial tile-enter event. Those follow-ups remain pending.

## Recovered load semantics

The ordinary CHANGEMAP path is the fresh-map path:

```text
loadType     = 0
gameIsLoaded = 0
```

A nonzero `loadType` routes the loading state into saved-game restoration via `Game_loadState(loadType)`. Saved/loaded contexts therefore remain fail-closed in this milestone.

## Permanent native API

Files:

```text
ESP32/include/esp_player_spawn_state.h
ESP32/src/esp_player_spawn_state.c
```

API:

```text
EspPlayerSpawn_reset()
EspPlayerSpawn_prepareCommitted()
```

State:

```text
EspPlayerSpawnState = 24 B
persistent heap = 0 B
```

The pointer-free state records source spawn parameter, tile/world coordinates, angle, view constants, spawn source, load type, target identities and explicit pending follow-up bits.

`EspPlayerSpawn_prepareCommitted()` requires:

```text
transition phase = COMMITTED
transition committed = 1
pendingConsumed = 1
loadType = 0
gameIsLoaded = 0
complete target inventory matching committed target
currently resident runtime matching that inventory
resident lifecycle ready
```

It performs no PAK I/O, allocation or legacy mutation. Refused paths zero the output when one exists.

## Hardware-proven real Junction projection

The committed transition retains:

```text
spawnParam = 0
```

Junction BSP header:

```text
spawnIndex     = 943
spawnDirection = 64
```

Real CYD result:

```text
stateBytes=24
stateFNV=ba6af4a7
targetMap=9
gameplayLoadMapId=2
spawnParam=00000000
source=HEADER
tileIndex=943
tile=15/29
world=992/1888
angle=64
viewZ=36
viewZOld=4
loadType=0
active=1
```

The predicted 24-byte FNV `ba6af4a7` is now a hardware canon.

Pending follow-ups were preserved exactly:

```text
facingRefresh=1
playerSetup=1
tileEnter=1
spawnApplied=no
facingApplied=no
playerSetupApplied=no
tileEnterApplied=no
```

## Hardware-proven packed override

The temporary probe used only a local committed-state copy with:

```text
spawnParam=00030167
```

Real CYD decode:

```text
tileIndex=359
tile=7/11
world=480/736
angle=192
source=OVERRIDE
overrideUsed=1
headerIgnored=yes
stateFNV=e0a5110b
```

The predicted override FNV `e0a5110b` is now a hardware canon. The real committed transition remained unchanged.

## Hardware-proven load semantic

```text
loadType=0
gameIsLoaded=0
normalMapLoad=yes
savedGameLoad=no
activeLoadType=0
loadTypeMutation=no
```

This confirms fresh-map CHANGEMAP semantics without opening saved-game restore behavior.

## Hardware-proven fail-closed boundary

All requested refusals passed:

```text
nullTransition=1
nullInventory=1
nullOutput=1
notCommitted=1
loadType=1
loadedWorld=1
targetMismatch=1
runtimeMismatch=1
badHeaderSpawn=1
reset=1
outputAtomic=yes
```

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

The hardware-tested owner FNVs remain:

```text
runtime  = bc432a0f
map      = c5cdfc04
script   = bc9b18ff
line     = 3658710d
texture  = 537319ad
automap  = 0b2ae445
topology = d6e8df7d
snapshot = bc9071e9
```

## RAM proof

Spawn projection itself has no heap cost:

```text
heap8=73012->73012
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

The same firmware's prerequisite committed transition measured:

```text
sourceHeap=65544
targetHeap=73012
targetHeapGain=7468
largest=34804->34804
```

The absolute heap baseline is 40 B lower than the previous PR #68 firmware build context, but the proven resident costs remain unchanged:

```text
Entrance actual heap = 18008 B
Junction actual heap = 10540 B
free-heap gain        = 7468 B
```

## Legacy / framebuffer integrity

Same-probe witnesses:

```text
placementFNV=5d1076bf->5d1076bf
playerFNV=a1725bcb->a1725bcb
frameFNV=833705d2->833705d2
legacyRuntimeClear=yes
DoomCanvasMutation=no
GameMutation=no
PlayerMutation=no
RenderMutation=no
HudMutation=no
```

These witness hashes are same-build equality checks, not cross-build canons.

Legacy topology remained closed:

```text
Game.entities=0
Game.monsters=0
```

## Final PARK

Real CYD:

```text
state=9
page=3
committedTransition=yes
mapSwapCommitted=yes
targetMap=9
junctionResident=yes
nativeSpawnState=yes
spawnProjected=yes
spawnApplied=no
loadType=0
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
entities=0
monsters=0
noGameplay=yes
```

Stable post-probe heartbeat included:

```text
heap=138776
heap8=73012
largest8=34804
SD=ready
VIDEO=ready
CORE=ready
```

The existing heartbeat label `ZIP=ready` does not imply runtime ZIP map access. Runtime backing remains `/DoomRPG-ESP32.pak`.

## Hardware canons added by this milestone

```text
EspPlayerSpawnState bytes = 24
Junction spawn FNV        = ba6af4a7
packed override FNV       = e0a5110b

Junction tileIndex = 943
Junction tile      = 15/29
Junction world     = 992/1888
Junction angle     = 64
viewZ              = 36
viewZOld           = 4
loadType           = 0
```

## Boundary after PASS

Native ownership now reaches through deterministic fresh-map player placement while still separating placement computation from gameplay initialization.

Still intentionally outside:

```text
actual stats-menu rendering/input
application of projected coordinates to a native player/view owner
native facing-entity query
Player_setup-equivalent fresh-map initialization
initial tile-enter event execution
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The next milestone must be selected only after merge and recovery from true `main`. Likely next boundaries are player/view application and/or native facing query, but they must be re-audited from the merged repo before implementation.

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
