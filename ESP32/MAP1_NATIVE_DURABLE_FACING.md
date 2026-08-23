# ESP32 native Junction durable facing milestone

Branch: `agent/esp32-native-durable-facing`

Base merged `main`:

```text
PR   = #75 — native finishRotation second tile
main = 7a0e57cf13d02320be3a238dc73499a023c9f04c
```

Status: **HARDWARE CANDIDATE — NOT YET CYD-PROVEN**.

## Objective

Own only the final legacy operation in `DoomCanvas_finishRotation()`:

```c
DoomCanvas_checkFacingEntity(doomCanvas);
```

Everything before it is hardware-proven native:

```text
PlayerView FNV         = 1bd0f09b
InitialTile FNV        = f73e28b2
Orientation FNV        = acc754a6
SecondTile FNV         = 09e58e0d
script FNV             = bc9b18ff
facingRefreshPending   = 1
```

`ST_PLAYING`, entity AI/gameplay and rendering remain outside this milestone.

## Exact recovered legacy semantics

For current Junction orientation `(destX,destY)=(992,1888)`, angle 64,
`viewSin=65536`, `viewCos=0`, `viewStepX=0`, `viewStepY=-64`:

```text
trace start = (992,1857)
trace end   = (992,1665)
trace flags = 0x0001f6ff
cell path   = (15,29) -> (15,28) -> (15,27) -> (15,26)
```

The recovered final filter is not simply nearest-sprite lookup. `Game_trace()`
walks `entityDb` head-to-tail and `checkFacingEntity()` keeps the first trace
candidate satisfying one of:

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

`EspMapTopologyQuery_findLinkedOnTile()` walks the existing compact map-sprite
topology by link order with tri-state results:

```text
 1 = entity returned
 0 = valid end of tile chain
-1 = invalid/inconsistent context
```

The durable resolver combines:

```text
EspMapRuntime immutable sprite/line geometry + block flags
EspMapSpriteTopology linked sprite entities + order/type/subtype
EspMapLineState current line open state
/DoomRPG-ESP32.pak /entities.db bounded range reads for line EntityDef type
```

Line entities are reconstructed in reverse line-index order because legacy map
load creates them after map-sprite entities and prepends them to `entityDb`.
Wall sentinel semantics are reconstructed from the block-map wall bit and
whether an initial line entity occupied that tile head.

The PAK is opened only during facing resolution and must be closed before the
API returns. No ZIP access, allocation or decompression is used.

## Permanent owner

```text
ESP32/include/esp_player_facing_state.h
ESP32/src/esp_player_facing_state.c
```

Candidate ABI:

```text
EspPlayerFacingState = 32 B
persistent heap = 0 B
```

Kinds:

```text
0 none
1 sprite
2 line
3 wall sentinel
```

The owner records the exact trace segment, normalized legacy identity, hit
index/tile, entity type/subtype, trace candidate count and map/load identity.
It contains no pointers.

API:

```text
EspPlayerFacing_reset()
EspPlayerFacing_isReady()
EspPlayerFacing_view()
EspPlayerFacing_prepare()
EspPlayerFacing_route()
```

`route()` consumes only `EspPlayerViewState.facingRefreshPending` after a fully
successful resolution. The expected deterministic PlayerView transition is:

```text
before FNV = 1bd0f09b
facingRefreshPending 1 -> 0
after FNV candidate = afcdcf74
```

`afcdcf74` is a candidate fingerprint until real-CYD proof.

No facing-owner FNV or hit kind/index is predicted before hardware. Those values
must come from the actual resident Junction data.

## Hardware probe

Temporary files:

```text
ESP32/include/native_junction_facing_probe.h
ESP32/src/native_junction_facing_probe.c
```

The normal `esp32-cyd` lifecycle runs it only after the hardware-proven second
finishRotation tile probe.

Expected marker:

```text
=== Doom RPG ESP32-native Junction durable facing ===
[JUNCTIONFACING] READY ...
```

The READY line publishes the actual:

```text
stateFNV
kind / kindName
hitIndex / hitTile
entityType / entitySubType
legacyIdentity
traceEntities
start/end coordinates
```

The probe also requires:

```text
PlayerView 1bd0f09b -> afcdcf74
InitialTile f73e28b2 unchanged
Orientation acc754a6 unchanged
SecondTile 09e58e0d unchanged
resident snapshot bc9071e9 unchanged
script bc9b18ff unchanged
PAK closed after resolution
heap/largest same-build delta 0
legacy Game/Player/Hud/DoomCanvas/Render/framebuffer unchanged
legacy Player.facingEntity unchanged
ST_PLAYING=no
entities=0
monsters=0
```

Fail-closed coverage includes null owners/output, wrong order/pending bits,
unsupported angle, mismatched InitialTile identity, inactive Orientation,
inactive SecondTile, topology inconsistency, storage failure and repeat route.

## Ordering boundary

```text
placement                         [hardware-proven]
HUD dirty                         [hardware-proven]
transient old-vector facing       [deliberately unowned]
Player_setup                      [hardware-proven]
initial Game_executeTile          [hardware-proven]
finishRotation orientation prep   [hardware-proven]
second Game_executeTile           [hardware-proven]
final durable facing              [THIS MILESTONE]
ST_PLAYING                        [deferred]
```

A successful durable-facing route makes `finishRotation()` semantically complete
while deliberately leaving the canvas in `ST_INTRO` for the next milestone.

## Mandatory invariants

```text
shapeData == NULL
mediaTexels == NULL
runtime ZIP access forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
ST_PLAYING not reached
```

## Promotion rule

Do not promote this milestone until normal `esp32-cyd` Serial proves a complete
`[JUNCTIONFACING] READY`, exact post-view `afcdcf74`, stable resident owners,
closed PAK and zero same-build heap/legacy/framebuffer mutation.

After PASS, only documentation commits may follow the flashed firmware SHA.

## Next boundary after PASS

Once durable facing is proven, `DoomCanvas_finishRotation()` is fully owned
natively. The next milestone can then recover the exact caller-side progression
toward `ST_PLAYING` without bundling gameplay/render work into this PR.
