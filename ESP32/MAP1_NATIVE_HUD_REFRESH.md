# ESP32 native Junction post-spawn HUD refresh milestone

Branch: `agent/esp32-native-post-spawn-refresh`

Base merged `main`:

```text
PR   = #70 — native player/view owner
main = 8a82891bb8d9c62582170cc4b3b74d270849e77b
```

Hardware-tested firmware:

```text
1761e2929ec50260a1f27373ce477530b84d041a
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

PR #70 hardware-proved an active 44-byte native player/view owner on committed Junction:

```text
stateFNV=d1131d18
view/dest=992/1888
z=36
angle=64
viewZOld=4
hudRefreshPending=1
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
```

This milestone owns only the immediate recovered `Hud.isUpdate = true` semantic. The native equivalent is a tiny HUD dirty owner. No HUD drawing or framebuffer mutation is performed.

## Legacy order audit and scope correction

Fresh-map loading is more subtle than the earlier high-level boundary suggested.

Recovered order:

```text
Game_spawnPlayer:
  apply player coordinates / angle / z
  Render.viewZOld = 4
  Hud.isUpdate = true
  DoomCanvas_checkFacingEntity()      # first check
  Player_setup()
  Game_executeTile(...)               # initial tile execution

then caller:
  DoomCanvas_finishRotation()
    recalc viewSin/viewCos/viewStep from destAngle
    Game_executeTile(... | 0x400)
    DoomCanvas_checkFacingEntity()    # durable final check
```

`viewSin/viewCos/viewStep` are not recalculated by `Game_spawnPlayer()`. The first facing check therefore uses the previous orientation vector. `Player_setup()` does not read `Player.facingEntity`, and `Game.c` does not consume it during the intervening initial tile execution. The durable facing is recomputed later after `finishRotation()` and after tile effects that may mutate world topology.

Therefore this milestone deliberately **does not claim facing ownership**. `facingRefreshPending` remains set and represents the still-required durable final-facing boundary. An early compact facing-index experiment was removed from the final firmware diff before the candidate was frozen.

## Permanent native HUD dirty owner

Files:

```text
ESP32/include/esp_hud_refresh_state.h
ESP32/src/esp_hud_refresh_state.c
```

Permanent state:

```text
EspHudRefreshState = 8 B
persistent heap = 0 B
```

Real-CYD Junction value:

```text
reason=POST_SPAWN
refreshPending=1
routed=1
active=1
targetMapId=9
gameplayLoadMapId=2
loadType=0
stateFNV=6965ee06
```

`6965ee06` is now a hardware canon.

## Player/view ownership transfer

`EspPlayerViewState` remains 44 B and its ABI is unchanged.

New primitive:

```text
EspPlayerView_consumeHudRefresh(targetMapId, gameplayLoadMapId, loadType)
```

The real CYD proved exactly:

```text
beforeFNV=d1131d18
afterFNV=d17fa0d1
hudRefreshPending: 1 -> 0
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
placementExact=yes
```

`d17fa0d1` is now a hardware canon for the post-HUD 44-byte player/view owner.

## Permanent API contract

```text
EspHudRefresh_reset()
EspHudRefresh_isReady()
EspHudRefresh_view()
EspHudRefresh_preparePostSpawn()
EspHudRefresh_routePostSpawn()
```

`preparePostSpawn()` is pure and pointer-free. `routePostSpawn()` validates the live fresh-map player/view owner, creates the native HUD dirty owner and consumes only `hudRefreshPending`.

No allocation, PAK I/O, legacy mutation, rendering, facing query, `Player_setup`, tile execution or `ST_PLAYING` occurs.

## Hardware fail-closed proof

Real CYD:

```text
nullView=1
nullOutput=1
inactive=1
loadType=1
missingHud=1
missingFacing=1
reset=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

Refused pure outputs are zeroed when an output pointer exists, and repeat routing leaves both owners unchanged.

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

The complete Junction resident owner set remained byte-exact.

## RAM proof

Real CYD:

```text
heap8=72940->72940
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat after the probe:

```text
heap=138704
heap8=72940
largest8=34804
```

Absolute heap is build-context dependent; the hardware proof is the zero same-build delta.

## Legacy / framebuffer integrity

Same-build witnesses:

```text
placementFNV=5d1076bf->5d1076bf
playerFNV=a1725bcb->a1725bcb
frameFNV=7a95b5b5->7a95b5b5
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
hudDirty=yes
hudRouted=yes
hudRefreshPending=0
facingPending=yes
playerSetupPending=yes
tileEnterPending=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

## Hardware canons added by this milestone

```text
EspHudRefreshState bytes = 8
Junction HUD FNV         = 6965ee06
post-HUD PlayerView FNV  = d17fa0d1
persistent heap          = 0 B
```

Inherited canons remain:

```text
pre-HUD PlayerView FNV   = d1131d18
Junction spawn FNV       = ba6af4a7
Junction snapshotFNV     = bc9071e9
Junction resident heap   = 10540 B
```

## Boundary after PASS

Native ownership now includes the semantic equivalent of `Hud.isUpdate=true` without coupling gameplay state to presentation.

The next meaningful fresh-map operation is `Player_setup()`-equivalent initialization, followed by the initial tile-enter execution. Durable facing must remain deferred until the correct later `finishRotation()` ordering, because that step recalculates orientation vectors, executes another tile-facing event and only then produces the final observable facing entity.

Still intentionally outside:

```text
actual HUD rendering / dirty consumption by renderer
native Player_setup-equivalent initialization
initial tile-enter execution
finishRotation-equivalent orientation preparation
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
MERGE agent/esp32-native-post-spawn-refresh
```

Hardware-tested firmware:

```text
1761e2929ec50260a1f27373ce477530b84d041a
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
