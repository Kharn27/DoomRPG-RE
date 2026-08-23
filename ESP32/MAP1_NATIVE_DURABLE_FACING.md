# ESP32 native Junction durable facing milestone

Branch: `agent/esp32-native-durable-facing`

Base merged `main`:

```text
PR   = #75 — native finishRotation second tile
main = 7a0e57cf13d02320be3a238dc73499a023c9f04c
```

Hardware-tested firmware:

```text
660c797e2168260a861c185fae9e812769b46156
```

Status: **REAL-CYD HARDWARE PASS / MERGE-READY**.

## Objective

Own only the final legacy operation in `DoomCanvas_finishRotation()`:

```c
DoomCanvas_checkFacingEntity(doomCanvas);
```

Everything before it was already hardware-proven native:

```text
PlayerView FNV       = 1bd0f09b
InitialTile FNV      = f73e28b2
Orientation FNV      = acc754a6
SecondTile FNV       = 09e58e0d
script FNV           = bc9b18ff
facingRefreshPending = 1
```

`ST_PLAYING`, entity AI/gameplay and rendering remain outside this milestone.

## Exact recovered legacy semantics

For Junction `(destX,destY)=(992,1888)`, angle 64,
`viewSin=65536`, `viewCos=0`, `viewStepX=0`, `viewStepY=-64`:

```text
trace start = (992,1857)
trace end   = (992,1665)
trace flags = 0x0001f6ff
cell path   = (15,29) -> (15,28) -> (15,27) -> (15,26)
```

The final filter is not nearest-sprite lookup. `Game_trace()` walks `entityDb`
head-to-tail and `checkFacingEntity()` keeps the first trace candidate satisfying
one of:

```text
eType == 14
Entity.info & 0x00200000        # line entity
Entity.info == 0                # wall sentinel
sprite tile differs from trace-start tile
```

Ordinary entities returned on the near/start tile are skipped. Types 14/15 use
the recovered oriented wall-sprite intersection test before entering the trace
result. Trace capacity remains bounded at 8.

## Native reconstruction

The old `Entity_t* entityDb[1024]` is not revived.

Permanent spatial helper:

```text
ESP32/include/esp_map_topology_query.h
ESP32/src/esp_map_topology_query.c
```

`EspMapTopologyQuery_findLinkedOnTile()` walks the compact map-sprite topology by
link order with tri-state results:

```text
 1 = entity returned
 0 = valid end of tile chain
-1 = invalid/inconsistent context
```

The durable resolver combines:

```text
EspMapRuntime immutable sprite/line geometry + block flags
EspMapSpriteTopology linked sprite entities + order/type/subtype
EspMapLineState current line-open state
/DoomRPG-ESP32.pak /entities.db bounded range reads for line EntityDef type
```

Line entities are reconstructed in reverse line-index order because legacy map
load creates them after map-sprite entities and prepends them to `entityDb`.
Wall sentinel semantics come from the block-map wall bit plus initial line-head
occupancy.

The PAK is opened only during facing resolution and is closed before return. No
ZIP access, allocation or decompression is used.

## Permanent owner

```text
ESP32/include/esp_player_facing_state.h
ESP32/src/esp_player_facing_state.c
```

Hardware-proven ABI:

```text
EspPlayerFacingState = 32 B
persistent heap      = 0 B
stateFNV             = 95aa1108
```

Kinds:

```text
0 none
1 sprite
2 line
3 wall sentinel
```

API:

```text
EspPlayerFacing_reset()
EspPlayerFacing_isReady()
EspPlayerFacing_view()
EspPlayerFacing_prepare()
EspPlayerFacing_route()
EspPlayerView_consumeFacing()
```

`route()` consumes only `EspPlayerViewState.facingRefreshPending` after complete
resolution.

Hardware-proven PlayerView transition:

```text
before FNV = 1bd0f09b
facingRefreshPending 1 -> 0
after FNV  = afcdcf74
```

## Real Junction facing result

The real CYD resolved the exact legacy ray to no facing target:

```text
stateBytes     = 32
stateFNV       = 95aa1108
kind           = 0
kindName       = none
hitIndex       = 65535
hitTile        = 65535
entityType     = 255
entitySubType  = 255
legacyIdentity = 00000000
traceEntities  = 0
traceFlags     = 0001f6ff
start          = 992/1857
end            = 992/1665
active         = 1
targetMap      = 9
gameplayLoadMapId = 2
loadType       = 0
```

So the final durable fresh-map facing at Junction is explicitly **none**. No
legacy `Player.facingEntity` write is needed.

## Hardware fail-closed proof

Real CYD:

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

## Hardware integrity proof

Input owners stayed unchanged:

```text
InitialTile FNV = f73e28b2
Orientation FNV = acc754a6
SecondTile FNV  = 09e58e0d
unchanged=yes
```

Resident snapshot stayed exact:

```text
snapshotFNV=bc9071e9->bc9071e9
unchanged=yes
payload=10410
entities=30
enemies=0
destructibles=3
packClosed=yes
```

RAM:

```text
heap8=72736->72736
delta=0
largest8=34804->34804
delta=0
persistentHeapBytes=0
```

Same-build legacy/framebuffer equality witnesses:

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

These witness FNVs prove equality within this firmware run; they are not
cross-build semantic canons.

## Hardware PARK

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
`DoomCanvas_finishRotation()` is semantically complete natively.

## Ordering boundary

```text
placement                         [hardware-proven]
HUD dirty                         [hardware-proven]
transient old-vector facing       [deliberately unowned]
Player_setup                      [hardware-proven]
initial Game_executeTile          [hardware-proven]
finishRotation orientation prep   [hardware-proven]
second Game_executeTile           [hardware-proven]
final durable facing              [hardware-proven HERE]
ST_PLAYING                        [deferred]
```

## Mandatory invariants

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Promotion result

Promotion requirements all passed on normal `esp32-cyd` firmware
`660c797e2168260a861c185fae9e812769b46156`:

```text
complete [JUNCTIONFACING] READY=yes
post-view afcdcf74=yes
resident stable=yes
PAK closed=yes
heap/largest delta=0
legacy/framebuffer unchanged=yes
Player.facingEntity unchanged=yes
```

Only documentation commits may follow the hardware-tested SHA on this branch.

## Next boundary after merge

`DoomCanvas_finishRotation()` is now fully owned natively. After this branch is
merged, recover from the exact new `main` SHA and isolate the caller-side state
progression toward `ST_PLAYING` as its own bounded milestone. Do not bundle
entity gameplay, renderer work or legacy runtime reconstruction into that step.
