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

## Current merge-ready milestone

[`MAP1_NATIVE_PLAYER_SETUP.md`](MAP1_NATIVE_PLAYER_SETUP.md) hardware-proves the fresh-map `Player_setup()` semantic owner.

```text
branch = agent/esp32-native-player-setup
base   = 02b7f143a12e6df86ada094af10ef580ad572aad
hardware-tested firmware = d808d895e97daef5d454ca06d5fda1738e99b147
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Permanent API

```text
EspPlayerFreshMapState = 24 B
persistent heap = 0 B

EspPlayerFreshMap_reset
EspPlayerFreshMap_isReady
EspPlayerFreshMap_view
EspPlayerFreshMap_prepare
EspPlayerFreshMap_route
EspPlayerView_consumePlayerSetup
```

### Recovered exact semantics

The supported fresh-map `Player_setup()` path now owns:

```text
levelStartTimeMs = exact sampled uptime
moves=0
xpGained=0
berserkerTics=0
familiarActive=0
notebookEmpty=1
weaponRestorePerformed=0
```

The real hardware run proved:

```text
startMs=27538
startExact=yes
moves=0
xpGained=0
berserker=0
familiar=0
notebookEmpty=1
weaponRestore=0
```

`levelStartTimeMs` is dynamic. The raw state FNV is therefore a same-run witness only:

```text
stateFNV=d0ab146e
```

The deterministic semantic FNV normalizes only `levelStartTimeMs` to zero:

```text
setup semanticFNV=3b27c6a1
```

`3b27c6a1` is hardware-proven and canonical.

### Player/view ownership transfer

Hardware proved:

```text
beforeFNV=d17fa0d1
afterFNV=c21fba3c
hudRefreshPending=0
facingRefreshPending=1
playerSetupPending=0
tileEnterPending=1
placementExact=yes
```

Only `playerSetupPending` is consumed. `c21fba3c` is the hardware canon for the post-setup player/view state.

### Weapon restore boundary

Recovered `Player_setup()` calls `Player_restoreWeapons()` only when `disabledWeapons != 0`. That branch can mutate weapons, clear disabled state, select a replacement weapon, and request view refresh.

This milestone intentionally supports only:

```text
disabledWeapons=0
```

and fails closed for the nonzero branch. Real CYD proved the fresh-run path has `disabledWeapons=0` and the `weaponRestore=1` refusal gate works.

### Hardware fail-closed proof

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

### Resident / RAM integrity

```text
snapshotFNV=bc9071e9->bc9071e9
targetLeftResident=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes

heap8=72900->72900
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Stable heartbeat:

```text
heap=138664
heap8=72900
largest8=34804
```

### Legacy / framebuffer integrity

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

These are same-build equality witnesses, not cross-build canons.

### Correct ordering boundary

Fresh-map order remains:

```text
placement
HUD dirty
transient old-vector facing write
Player_setup                           [hardware-proven]
initial tile-enter                     [next]
finishRotation orientation preparation
second tile execution
final durable facing
```

The transient first facing result remains deliberately unowned; the durable facing must wait until the later correct ordering.

Final hardware PARK:

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
legacy entities=0
legacy monsters=0
noGameplay=yes
```

## Hardware-proven canons through current milestone

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
playerSetupBytes=24
PlayerSetupSemanticFNV=3b27c6a1
postSetupPlayerViewFNV=c21fba3c
```

Same-run dynamic setup witness:

```text
stateFNV=d0ab146e at startMs=27538
```

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
 -> Player_setup-equivalent session root          [hardware-proven]
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
second tile/facing event
final native facing-entity query
ST_PLAYING progression
full native entity/monster gameplay
native gameplay renderer
sound playback
```

The player/view, HUD and fresh-map setup owners deliberately have different lifetimes from the map-resident arena and are not part of the seven-owner resident snapshot.

`shapeData == NULL` and `mediaTexels == NULL` remain mandatory.

## Next bounded milestone after merge

Recover from the true post-merge `main` before implementation. The next exact operation is the **initial tile-enter execution at `(992,1888)`**. Keep `finishRotation()`, its second tile execution, final facing and `ST_PLAYING` as later boundaries unless a fresh legacy audit proves otherwise.

## Merge recommendation

```text
MERGE agent/esp32-native-player-setup
```

Hardware-tested firmware:

```text
d808d895e97daef5d454ca06d5fda1738e99b147
```

Every later commit on this branch must remain documentation-only unless another firmware is flashed.
