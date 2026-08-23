# ESP32 native Junction post-spawn HUD refresh milestone

Branch: `agent/esp32-native-post-spawn-refresh`

Base merged `main`:

```text
PR   = #70 — native player/view owner
main = 8a82891bb8d9c62582170cc4b3b74d270849e77b
```

Firmware candidate:

```text
1761e2929ec50260a1f27373ce477530b84d041a
```

Status: **IMPLEMENTED; REAL-CYD HARDWARE VALIDATION PENDING**.

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

This milestone owns only the immediate recovered `Hud.isUpdate = true` semantic. The native equivalent is a tiny HUD dirty owner. No HUD drawing or framebuffer change is performed.

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

Therefore this milestone deliberately **does not claim facing ownership**. The previous single `facingRefreshPending` bit remains set and is treated as the still-required durable final facing boundary. Implementing a facing query before native setup/tile execution would be out of order.

An initial compact facing-index implementation was explored during this audit and removed from the final branch diff before the firmware candidate was frozen.

## Permanent native HUD dirty owner

Files:

```text
ESP32/include/esp_hud_refresh_state.h
ESP32/src/esp_hud_refresh_state.c
```

State:

```text
EspHudRefreshState = 8 B expected classic-ESP32 ABI
persistent heap = 0 B
```

Fields:

```text
reason
refreshPending
routed
active
targetMapId
gameplayLoadMapId
loadType
reserved
```

Post-spawn value expected for Junction:

```text
reason=POST_SPAWN
refreshPending=1
routed=1
active=1
targetMapId=9
gameplayLoadMapId=2
loadType=0
```

Static little-endian 8-byte FNV prediction:

```text
6965ee06
```

This remains a prediction until the real CYD confirms it.

## Player/view ownership transfer

`EspPlayerViewState` remains 44 B and gains only an API; its ABI is unchanged.

New primitive:

```text
EspPlayerView_consumeHudRefresh(targetMapId, gameplayLoadMapId, loadType)
```

It clears exactly:

```text
hudRefreshPending: 1 -> 0
```

while requiring and preserving:

```text
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
spawnApplied=1
active=1
all coordinates/identity unchanged
```

Expected player/view fingerprint after the transfer:

```text
before = d1131d18
 after = d17fa0d1
```

`d17fa0d1` is a candidate until hardware confirmation.

## Permanent API contract

```text
EspHudRefresh_reset()
EspHudRefresh_isReady()
EspHudRefresh_view()
EspHudRefresh_preparePostSpawn()
EspHudRefresh_routePostSpawn()
```

`preparePostSpawn()` is pure and pointer-free. It validates one player/view state and creates caller-owned HUD metadata without mutation.

`routePostSpawn()`:

```text
requires live native player/view
requires fresh-map loadType=0
requires hud/facing/setup/tile pending all set
creates native HUD dirty owner
atomically consumes only player/view hudRefreshPending
```

No allocation, PAK I/O, legacy mutation, rendering, facing query, Player_setup, tile execution or ST_PLAYING occurs.

## Fail-closed boundary

The hardware probe checks refusal/atomicity for:

```text
null player/view
null output
inactive player/view
nonzero loadType
missing HUD pending bit
missing facing pending bit / wrong ordering
repeat route while HUD owner active
reset to empty
```

Pure refused outputs are zeroed when an output pointer exists.

## Temporary hardware probe

Files:

```text
ESP32/include/native_junction_hud_refresh_probe.h
ESP32/src/native_junction_hud_refresh_probe.c
```

It arms only after the hardware-proven Junction player/view probe has parked the real `d1131d18` owner.

Expected decisive Serial:

```text
[JUNCTIONHUDPROBE] ARMED ...

=== Doom RPG ESP32-native Junction post-spawn HUD refresh ===

[JUNCTIONHUD] READY stateBytes=8 stateFNV=6965ee06 reason=POST_SPAWN refreshPending=1 routed=1 active=1 targetMap=9 gameplayLoadMapId=2 loadType=0

[JUNCTIONHUD] PLAYER viewBytes=44 beforeFNV=d1131d18 afterFNV=d17fa0d1 hudPending=0 facingPending=1 playerSetupPending=1 tileEnterPending=1 placementExact=yes

[JUNCTIONHUD] ORDER firstFacingTransient=yes finalFacingDeferred=yes finishRotationDeferred=yes playerSetupPending=yes tileEnterPending=yes

[JUNCTIONHUD] FAILCLOSED nullView=1 nullOutput=1 inactive=1 loadType=1 missingHud=1 missingFacing=1 reset=1 prepareAtomic=yes repeat=1 repeatAtomic=yes

[JUNCTIONHUD] RESIDENT snapshotFNV=bc9071e9->bc9071e9 targetLeftResident=yes payload=10410 entities=30 enemies=0 destructibles=3 packClosed=yes

[JUNCTIONHUD] RAM heap8=...->... delta=0 largest8=...->... delta=0 persistentHeapBytes=0

[JUNCTIONHUD] LEGACY ... unchanged ... HudMutation=no

[JUNCTIONHUD] PARK state=9 page=3 mapSwapCommitted=yes targetMap=9 junctionResident=yes nativePlayerView=yes nativeHudRefresh=yes hudDirty=yes hudRouted=yes facingPending=yes playerSetupPending=yes tileEnterPending=yes ST_PLAYING=no entities=0 monsters=0 noGameplay=yes
```

## Strict PASS boundary

A PASS must prove:

```text
EspHudRefreshState bytes=8
HUD stateFNV=6965ee06
player/view d1131d18 -> d17fa0d1
only hudRefreshPending cleared
facing/setup/tile remain pending
Junction snapshot bc9071e9 unchanged
heap/largest unchanged
PAK closed
legacy Hud/DoomCanvas/Render/Game/Player unchanged
framebuffer unchanged
ST_PLAYING=no
legacy entities=0
legacy monsters=0
```

## Boundary after PASS

A hardware PASS would establish the native equivalent of `Hud.isUpdate=true` without forcing presentation into the gameplay lifecycle.

The next exact legacy operation is `Player_setup()`. The durable facing query must remain deferred until after native setup/tile execution and native `finishRotation`-equivalent orientation preparation, because the final legacy facing state is computed there.

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
