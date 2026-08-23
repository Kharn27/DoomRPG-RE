# ESP32 documentation map

This file defines the current ESP32 CYD documentation map.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: detailed implementation and hardware evidence/candidate contracts.

## Recent merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md) | CHANGEMAP pending transition intent | #61 | `fc39ac60757e0d992e3729a5044a9d83e9994971` |
| [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) | SHOW/HIDE compact topology | #62 | `ed5cd9a09c9ae36f999661f4284f64400681b1af` |
| [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md) | level-exit stats | #63 | `533784b5483e14a12558fb08c9331d8b744caa88` |
| [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md) | player exit-state | #64 | `3759bcd12a3f6d36a6a696457110ab27474c24b8` |
| [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md) | stats-menu intent | #65 | `c8679133351fa00e01a67103386b7676660c4a6e` |
| [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md) | Junction preflight | #66 | `9f981f490282200f216aef66d22608d2244beb00` |
| [`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md) | resident handoff | #67 | `fddae899fd7dc01b20cf6bd532489326380954e3` |
| [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md) | committed map swap | #68 | `00268a100c6662cb883f9a02d979b4f29eecbf12` |
| [`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md) | fresh Junction spawn | #69 | `992f38374840113409e776fb82ce57ab014607e5` |
| [`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md) | active player/view owner | #70 | `8a82891bb8d9c62582170cc4b3b74d270849e77b` |
| [`MAP1_NATIVE_HUD_REFRESH.md`](MAP1_NATIVE_HUD_REFRESH.md) | HUD dirty ownership | #71 | `02b7f143a12e6df86ada094af10ef580ad572aad` |
| [`MAP1_NATIVE_PLAYER_SETUP.md`](MAP1_NATIVE_PLAYER_SETUP.md) | fresh-map Player_setup | #72 | `9077ae4496bdcc06b6b99846332ab43b38943a8a` |
| [`MAP1_NATIVE_INITIAL_TILE_ENTER.md`](MAP1_NATIVE_INITIAL_TILE_ENTER.md) | first fresh-map tile dispatch | #73 | `0bc171affad8416ed1a7918a4a67fd4d53d61efe` |
| [`MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md`](MAP1_NATIVE_FINISH_ROTATION_ORIENTATION.md) | finishRotation orientation | #74 | `2decae5067438dc1a2d9c29335cfc0cad5538645` |
| [`MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md`](MAP1_NATIVE_FINISH_ROTATION_SECOND_TILE.md) | finishRotation second tile | #75 | `7a0e57cf13d02320be3a238dc73499a023c9f04c` |
| [`MAP1_NATIVE_DURABLE_FACING.md`](MAP1_NATIVE_DURABLE_FACING.md) | finishRotation durable facing | #76 | `3ab143110a1f44ebb44bc130d12d1844f3ae73ca` |

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #76 hardware-proved the final durable `DoomCanvas_checkFacingEntity()` and
therefore completes the recovered `DoomCanvas_finishRotation()` sequence natively.

```text
EspPlayerFacingState=32 B
stateFNV=95aa1108
kind=none
traceEntities=0
PlayerView FNV=afcdcf74
finishRotationComplete=yes
persistent heap=0 B
```

Real-CYD Junction resident/RAM boundary remains:

```text
snapshotFNV=bc9071e9
runtime=bc432a0f
map=c5cdfc04
script=bc9b18ff
line=3658710d
texture=537319ad
automap=0b2ae445
topology=d6e8df7d
heap8=72736
largest8=34804
ST_PLAYING=no
entities=0
monsters=0
```

## Exact caller order after finishRotation

Current `src/DoomCanvas.c` continues with:

```text
Hud.msgCount=0
Hud.statBarMessage=NULL
Hud.logMessage[0]='\0'
if MAP_JUNCTION: Game_givemap()
else: DoomCanvas_uncoverAutomap()
Player_selectWeapon(current weapon)
initial Game_saveState when !isLoaded
clear isLoaded/isSaved/activeLoadType
clear queued events / particles
isUpdateView=true
DoomCanvas_setState(ST_PLAYING)
idleTime=time+8000
```

This sequence is being ported in bounded caller-order milestones instead of one
large post-load finalizer.

## Current hardware candidate

[`MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md`](MAP1_NATIVE_POST_LOAD_HUD_CLEAR.md) owns
only the first three HUD message-channel resets after finishRotation.

```text
branch = agent/esp32-native-post-load-hud-clear
base   = 3ab143110a1f44ebb44bc130d12d1844f3ae73ca
status = HARDWARE CANDIDATE — NOT YET CYD-PROVEN
```

### Permanent owner

```text
ESP32/include/esp_hud_post_load_clear_state.h
ESP32/src/esp_hud_post_load_clear_state.c

EspHudPostLoadClearState = 8 B
persistent heap = 0 B
```

It represents only:

```text
messageCount=0
statBarMessagePresent=0
logMessageLength=0
cleared=1
active=1
```

The owner is deliberately distinct from the hardware-proven post-spawn
`EspHudRefreshState`: that older state owns `Hud.isUpdate=true`; this one owns
the later post-load message reset.

`EspHudPostLoadClear_prepare()` requires:

```text
active fresh-map PlayerView
PlayerView FNV boundary afcdcf74
all PlayerView pending bits clear
matching active durable facing
finishRotation complete
```

Saved-world load is fail-closed. `route()` parks only this owner and does not
mutate PlayerView, facing or the resident map.

### Hardware probe

```text
ESP32/include/native_junction_post_load_hud_clear_probe.h
ESP32/src/native_junction_post_load_hud_clear_probe.c
```

Expected block:

```text
=== Doom RPG ESP32-native Junction post-load HUD clear ===
[JUNCTIONHUDCLEAR] READY ...
```

The probe requires:

```text
EspHudPostLoadClearState=8 B
PlayerView afcdcf74 unchanged
Facing 95aa1108 unchanged
resident snapshot bc9071e9 unchanged
automap 0b2ae445 unchanged
pack closed
heap/largest delta=0
legacy Game/Player/Hud/DoomCanvas/Render/framebuffer unchanged
ST_PLAYING=no
entities=0
monsters=0
```

It hashes the complete legacy `Hud_t` before/after and also logs the three exact
legacy message fields, proving native semantic ownership without writing the
legacy HUD.

## Hardware-proven canons through PR #76

```text
Entrance snapshotFNV=b3811f3d
Entrance heap=18008 B
Junction snapshotFNV=bc9071e9
Junction heap=10540 B
Junction sourceFNV=fefaf5ca
catalogFNV=ce322e3f
committed COMMITTED FNV=2c595a62
JunctionSpawnFNV=ba6af4a7
JunctionPlayerViewFNV=d1131d18
postHudPlayerViewFNV=d17fa0d1
JunctionHudRefreshFNV=6965ee06
PlayerSetupSemanticFNV=3b27c6a1
postSetupPlayerViewFNV=c21fba3c
JunctionInitialTileFNV=f73e28b2
postInitialTilePlayerViewFNV=1bd0f09b
JunctionOrientationFNV=acc754a6
JunctionSecondTileFNV=09e58e0d
JunctionDurableFacingFNV=95aa1108
postFacingPlayerViewFNV=afcdcf74
```

## Architecture direction

```text
original behavior/data
 -> /DoomRPG-ESP32.pak
 -> compact immutable map                        [hardware-proven]
 -> compact mutable world overlays               [hardware-proven]
 -> native event semantics                       [hardware-proven by family]
 -> native transition/residency                  [hardware-proven]
 -> native fresh-map player chain                [hardware-proven]
 -> finishRotation durable facing                [hardware-proven]
 -> post-load HUD message reset                  [CURRENT CANDIDATE]
 -> direct Junction Game_givemap                 [next]
 -> remaining caller-side load completion
 -> ST_PLAYING
 -> native gameplay
 -> native renderer
```

Current hardware PARK:

```text
state=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativePlayerView=yes
nativeInitialTile=yes
nativeOrientation=yes
nativeSecondTile=yes
nativeFacing=yes
facingPending=no
finishRotationComplete=yes
ST_PLAYING=no
legacy Game.entities=0
legacy Game.monsters=0
noGameplay=yes
```

Mandatory invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Next test

Build and flash normal `esp32-cyd`, then return the complete
`[JUNCTIONHUDCLEAR]` Serial block. Promote only after the real CYD proves the
owner, unchanged legacy HUD/PlayerView/facing/resident/automap, closed PAK and
zero RAM/framebuffer regression.

After PASS, the next bounded milestone is direct caller-side Junction
`Game_givemap()` against native automap/map-state owners.
