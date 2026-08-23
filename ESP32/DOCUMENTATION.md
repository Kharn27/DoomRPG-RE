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

Older archives remain indexed by Git history. `PORTING_STATUS.md` is the preferred recovery point.

## Latest merged boundary

PR #75 hardware-proved the second `Game_executeTile()` in
`DoomCanvas_finishRotation()`:

```text
EspPlayerFinishRotationTileState=24 B
stateFNV=09e58e0d
tile=943
flags=0x10000400
eventIndex=61
eligible=0
executed=0
removed=0
script FNV=bc9b18ff unchanged
persistent heap=0 B
```

## Current merge-ready milestone

[`MAP1_NATIVE_DURABLE_FACING.md`](MAP1_NATIVE_DURABLE_FACING.md) hardware-proves
the final durable `DoomCanvas_checkFacingEntity()` in recovered
`DoomCanvas_finishRotation()`.

```text
branch = agent/esp32-native-durable-facing
base   = 7a0e57cf13d02320be3a238dc73499a023c9f04c
hardware-tested firmware = 660c797e2168260a861c185fae9e812769b46156
status = REAL-CYD HARDWARE PASS / MERGE-READY
```

### Exact legacy ray

At Junction angle 64:

```text
traceStart=(992,1857)
traceEnd=(992,1665)
traceFlags=0x0001f6ff
tile path=(15,29),(15,28),(15,27),(15,26)
```

The final legacy selection keeps the first trace candidate matching `eType==14`,
a line entity (`Entity.info & 0x00200000`), the wall sentinel (`Entity.info==0`),
or an ordinary sprite whose tile differs from the trace-start tile. Ordinary
near-tile candidates are skipped.

### Permanent native query + facing owner

```text
ESP32/include/esp_map_topology_query.h
ESP32/src/esp_map_topology_query.c
EspMapTopologyQuery_findLinkedOnTile()

ESP32/include/esp_player_facing_state.h
ESP32/src/esp_player_facing_state.c
EspPlayerFacingState = 32 B
```

The resolver combines:

```text
immutable EspMapRuntime sprite/line/block data
compact EspMapSpriteTopology entity type/tile/order
EspMapLineState current open state
bounded /entities.db PAK reads for line EntityDef type/subtype
```

No `Entity_t`, legacy `entityDb[1024]`, `Game_trace()` or
`Player.facingEntity` write is used. PAK access is temporary and closes before
return. Persistent heap cost is zero.

### Hardware facing canon

The real CYD resolved no durable target on the exact ray:

```text
EspPlayerFacingState=32 B
stateFNV=95aa1108
kind=0 / none
hitIndex=65535
hitTile=65535
entityType=255
entitySubType=255
legacyIdentity=00000000
traceEntities=0
traceFlags=0001f6ff
start=992/1857
end=992/1665
```

Hardware-proven PlayerView transition:

```text
beforeFNV=1bd0f09b
afterFNV=afcdcf74
facingRefreshPending=1 -> 0
consumedOnlyFacing=yes
```

Input owners remain exact:

```text
InitialTile FNV=f73e28b2
Orientation FNV=acc754a6
SecondTile FNV=09e58e0d
```

### Hardware fail-closed proof

```text
nullView=1
nullInitial=1
nullOrientation=1
nullSecond=1
nullOutput=1
missingFacing=1
tilePending=1
angle=1
initialMismatch=1
orientationInactive=1
secondInactive=1
prepareAtomic=yes
repeat=1
repeatAtomic=yes
```

### Hardware RAM / integrity

```text
snapshotFNV=bc9071e9->bc9071e9
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes

heap8=72736->72736
largest8=34804->34804
persistentHeapBytes=0
```

Same-build equality witnesses:

```text
gameFNV=c655ff85->c655ff85
playerFNV=c64e7862->c64e7862
canvasFNV=1b7ba23f->1b7ba23f
renderFNV=f9344dec->f9344dec
frameFNV=9eb7ce0f->9eb7ce0f
legacyRuntimeClear=yes
GameMutation=no
PlayerMutation=no
FacingEntityMutation=no
HudMutation=no
DoomCanvasMutation=no
RenderMutation=no
```

## Hardware-proven canons through current branch

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

Resident owners:

```text
runtime=bc432a0f
map=c5cdfc04
script=bc9b18ff
line=3658710d
texture=537319ad
automap=0b2ae445
topology=d6e8df7d
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
 -> caller-side ST_PLAYING progression           [next]
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

This is the first hardware-proven point where recovered fresh-map
`DoomCanvas_finishRotation()` is fully complete natively.

Mandatory invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP map access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Next bounded milestone after merge

After merge, recover from the exact new `main` SHA and audit the caller-side code
immediately following `DoomCanvas_finishRotation()`. Isolate only the smallest
state progression toward `ST_PLAYING`; keep gameplay entities, AI and rendering
outside until separately owned and hardware-proven.

## Merge recommendation

```text
MERGE agent/esp32-native-durable-facing
```

Hardware-tested firmware:

```text
660c797e2168260a861c185fae9e812769b46156
```

All commits after that tested SHA on this branch must be documentation-only.
