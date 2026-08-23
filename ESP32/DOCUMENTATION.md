# ESP32 documentation map

This file defines the current ESP32 CYD documentation map.

## Source of truth

- [`README.md`](README.md): stable build/flash guide.
- [`PORTING_STATUS.md`](PORTING_STATUS.md): authoritative current recovery point.
- Milestone archives: detailed implementation and hardware evidence.

## Recent merged milestones

| Archive | Purpose | PR | Merged `main` |
| --- | --- | ---: | --- |
| [`MAP1_NATIVE_CHANGE_MAP_INTENT.md`](MAP1_NATIVE_CHANGE_MAP_INTENT.md) | CHANGEMAP pending transition intent | #61 | `fc39ac60757e0d992e3729a5044a9d83e9994971` |
| [`MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md`](MAP1_NATIVE_SHOW_HIDE_TOPOLOGY.md) | SHOW/HIDE compact topology; all MAP_INTRO opcode families owned | #62 | `ed5cd9a09c9ae36f999661f4284f64400681b1af` |
| [`MAP1_NATIVE_LEVEL_EXIT_STATS.md`](MAP1_NATIVE_LEVEL_EXIT_STATS.md) | map-derived level-exit stats | #63 | `533784b5483e14a12558fb08c9331d8b744caa88` |
| [`MAP1_NATIVE_PLAYER_EXIT_STATE.md`](MAP1_NATIVE_PLAYER_EXIT_STATE.md) | pointer-free player exit writes | #64 | `3759bcd12a3f6d36a6a696457110ab27474c24b8` |
| [`MAP1_NATIVE_STATS_MENU_INTENT.md`](MAP1_NATIVE_STATS_MENU_INTENT.md) | LEVEL/OVERALL stats-menu intent | #65 | `c8679133351fa00e01a67103386b7676660c4a6e` |
| [`MAP1_NATIVE_TRANSITION_PREFLIGHT.md`](MAP1_NATIVE_TRANSITION_PREFLIGHT.md) | map catalog + Junction PAK/BSP preflight | #66 | `9f981f490282200f216aef66d22608d2244beb00` |
| [`MAP1_NATIVE_RESIDENT_HANDOFF.md`](MAP1_NATIVE_RESIDENT_HANDOFF.md) | reversible full resident Entrance/Junction handoff | #67 | `fddae899fd7dc01b20cf6bd532489326380954e3` |
| [`MAP1_NATIVE_COMMITTED_TRANSITION.md`](MAP1_NATIVE_COMMITTED_TRANSITION.md) | transactional committed Entrance -> Junction resident swap | #68 | `00268a100c6662cb883f9a02d979b4f29eecbf12` |
| [`MAP1_NATIVE_JUNCTION_SPAWN.md`](MAP1_NATIVE_JUNCTION_SPAWN.md) | fresh-map Junction spawn/load projection | #69 | `992f38374840113409e776fb82ce57ab014607e5` |
| [`MAP1_NATIVE_PLAYER_VIEW.md`](MAP1_NATIVE_PLAYER_VIEW.md) | permanent active Junction player/view owner | #70 | `8a82891bb8d9c62582170cc4b3b74d270849e77b` |
| [`MAP1_NATIVE_HUD_REFRESH.md`](MAP1_NATIVE_HUD_REFRESH.md) | post-spawn HUD dirty ownership and corrected facing order | #71 | `02b7f143a12e6df86ada094af10ef580ad572aad` |

Older milestone archives remain in this directory and are indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #71 hardware-proved native ownership of recovered `Hud.isUpdate=true` without presentation:

```text
EspHudRefreshState=8 B
HUD FNV=6965ee06
PlayerView FNV d1131d18 -> d17fa0d1
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=1
tileEnterPending=1
persistent heap=0 B
```

Junction remains resident at:

```text
snapshotFNV=bc9071e9
payload=10410 B
actual heap=10540 B
entities=30 enemies=0 destructibles=3
```

The corrected fresh-map ordering is:

```text
placement
HUD dirty
transient old-vector facing write
Player_setup
initial tile-enter
finishRotation orientation preparation
second tile execution
final durable facing
```

The transient facing result is not consumed before it is overwritten, so native facing remains intentionally deferred.

## Current hardware candidate

[`MAP1_NATIVE_PLAYER_SETUP.md`](MAP1_NATIVE_PLAYER_SETUP.md) owns only fresh-map `Player_setup()` semantics.

```text
branch = agent/esp32-native-player-setup
base   = 02b7f143a12e6df86ada094af10ef580ad572aad
firmware candidate = d808d895e97daef5d454ca06d5fda1738e99b147
status = IMPLEMENTED / REAL-CYD HARDWARE VALIDATION PENDING
```

### Permanent API

```text
EspPlayerFreshMapState = 24 B expected
persistent heap = 0 B

EspPlayerFreshMap_reset
EspPlayerFreshMap_isReady
EspPlayerFreshMap_view
EspPlayerFreshMap_prepare
EspPlayerFreshMap_route

EspPlayerView_consumePlayerSetup
```

### Recovered exact semantics

Supported `Player_setup()` path owns:

```text
levelStartTimeMs = sampled DoomRPG_GetUpTimeMS()
moves=0
xpGained=0
berserkerTics=0
familiarActive=0
notebookEmpty=1
weaponRestorePerformed=0
```

The real path must have `disabledWeapons=0`. A nonzero value is fail-closed because recovered `Player_restoreWeapons()` can mutate weapons, select a replacement weapon, and request a view refresh; those effects are not silently approximated.

### Candidate deterministic canons

The timestamp is dynamic, so the setup semantic FNV normalizes only `levelStartTimeMs` to zero:

```text
setup semanticFNV = 3b27c6a1      # prediction
PlayerView before = d17fa0d1       # hardware-proven input
PlayerView after  = c21fba3c       # prediction
```

The candidate clears only:

```text
playerSetupPending: 1 -> 0
```

and preserves:

```text
hudRefreshPending=0
facingRefreshPending=1
tileEnterPending=1
placement/load identity unchanged
HUD owner FNV=6965ee06 unchanged
```

### Candidate fail-closed proof

The probe checks:

```text
nullView
nullHud
nullOutput
inactive
loadType
hudPending
missingFacing
missingSetup
missingTile
hudMismatch
weaponRestore
reset
prepareAtomic
repeat
repeatAtomic
```

### Required hardware integrity

A PASS requires:

```text
setup bytes=24
semanticFNV=3b27c6a1
startExact=yes
real disabledWeapons=0
PlayerView d17fa0d1 -> c21fba3c
Junction snapshot bc9071e9 unchanged
heap/largest unchanged
persistentHeapBytes=0
PAK closed
legacy Player/Game/Hud/DoomCanvas/Render unchanged
framebuffer unchanged
initial tile-enter deferred
finishRotation/final facing deferred
ST_PLAYING=no
legacy entities=0
legacy monsters=0
```

## Hardware-proven canons through PR #71

```text
Entrance snapshotFNV=b3811f3d
Entrance heap=18008 B
Junction snapshotFNV=bc9071e9
Junction heap=10540 B
Junction sourceFNV=fefaf5ca
catalogFNV=ce322e3f
preflightFNV=108e5c7b
statsMenuIntentFNV=96afe901
levelExitStatsFNV=bd41bcfa
playerExitAppliedFNV=298eaaa4
committedTransitionFNV=2c595a62
playerSpawnBytes=24
JunctionSpawnFNV=ba6af4a7
packedOverrideFNV=e0a5110b
playerViewBytes=44
JunctionPlayerViewFNV=d1131d18
packedOverrideViewFNV=9ed47d08
postHudPlayerViewFNV=d17fa0d1
hudRefreshBytes=8
JunctionHudRefreshFNV=6965ee06
```

Candidate-only predictions are not promoted into this hardware canon list until Serial PASS.

## Architecture direction

```text
original Doom RPG behavior/data
 -> native pack-backed parsers
 -> compact immutable map + explicit mutable owners
 -> native event/script ownership
 -> exit chain
 -> map catalog/preflight
 -> explicit resident lifecycle
 -> reversible full resident handoff              [hardware-proven]
 -> committed stats-ack-gated transition          [hardware-proven]
 -> fresh-map load semantic                       [hardware-proven]
 -> native player spawn projection                [hardware-proven]
 -> permanent native player/view application      [hardware-proven]
 -> post-spawn HUD dirty ownership                [hardware-proven]
 -> Player_setup-equivalent session root          [candidate]
 -> initial tile-enter
 -> finishRotation-equivalent orientation + tile event
 -> durable facing query
 -> ST_PLAYING / native gameplay/render loop
```

Still outside:

```text
actual stats-menu rendering/input
actual HUD rendering / renderer dirty consumption
weapon restore/select ownership when disabledWeapons!=0
initial tile-enter execution
finishRotation-equivalent orientation preparation
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next bounded milestone after candidate PASS + merge

Recover from true post-merge `main`. The next exact operation is the initial tile-enter at the hardware-proven Junction position `(992,1888)`. Keep `finishRotation()`, its second tile execution, final facing and `ST_PLAYING` as later boundaries unless a fresh legacy audit proves otherwise.

## Merge recommendation

```text
DO NOT MERGE YET — hardware validation pending
```

Firmware candidate:

```text
d808d895e97daef5d454ca06d5fda1738e99b147
```
