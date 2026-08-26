# MAP1 native gameplay closed line-entity collision recovery

## Status

```text
branch = agent/esp32-native-gameplay-door-view-probe
base main = 2aae0676528ab00c3494d142d8b35c22b7685dce
base PR = #97 — shared-payload large range cache + bounded legacy wall guard
hardware-tested implementation SHA = efea93977d20ba94e2fd5d6981ebce2e7916bc5b
status = REAL-CYD HARDWARE PASS
merge-ready = yes after documentation-only closeout audit
```

This milestone started as a renderer/view investigation around the Junction arrival door. The real CYD proved that the renderer was not the root cause. The visual corruption came from an incomplete native movement-collision model that allowed the player to step onto a closed legacy line entity.

## Original symptom

Fresh Junction spawn is:

```text
player = 992,1888,36
angle  = 64 / North
tile   = 943
frame  = ba3e5182
viewport = 9206eb24
HUD = 6c2aa46f
```

The earlier native collision milestone classified `BACK` as clear:

```text
BACK delta=0,+64 tile 943->975 flags=1c CLEAR
```

That move committed the player to:

```text
player = 992,1952,36
tile   = 975
camera = 992,1968
```

The relevant closed arrival-door line is:

```text
line    = 35
texture = 7
raw     = 960,1952 -> 1024,1952
flags   = 00000505
midpoint= 992,1952
```

The player was therefore moved exactly onto the door-line midpoint. With the proven 16-unit gameplay camera offset, the wall became extremely near the camera and filled most of the viewport. The huge close-up door was a consequence of illegal movement, not a projection bug.

## Renderer diagnosis that ruled out the wrong layer

The bounded door-view diagnostics proved the failing post-move frame itself was internally coherent:

```text
world rendered=1
world presented=0
HUD exact=yes
sprite accounting complete=yes
sprite unsupported=0
render scratch stable=yes
final presentation succeeds
```

At the illegal pose the world renderer produced:

```text
BSP nodes=11 leaves=1 lines=5
wall requests=4 draws=4 spans=214 pixels=17120
viewport=223030ff
frame=75bedd61
```

This is now interpreted correctly: the renderer was drawing a valid but nonsensical camera pose produced by collision semantics. No orientation sign, BSP side test, wall projection rule, sprite compositor rule, or `PlatformVideo_present()` behavior needed to be changed.

The same final hardware run also revalidated all four cardinal rotations at the legal spawn and returned exactly to canonical North:

```text
N frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f
E frame=8cfdfe34 viewport=17c48c15 HUD=1d908304
S frame=da1c4297 viewport=582c2ad8 HUD=a78d0f96
W frame=23ee0954 viewport=de06a408 HUD=9281a6d1
N round-trip frame=ba3e5182 exact
```

## Recovered legacy collision behavior

The missing behavior is in legacy `Game_loadMapEntities()` + `Game_trace()`.

For every map line, legacy attempts:

```text
EntityDef_lookup(305 + line.texture)
```

If no definition exists and the line has fallback flags `0x18`, it uses the static wall entity definition. A qualifying line is materialized as an `Entity_t`, linked into `Game.entityDb`, and therefore participates in `Game_trace(..., 62087 / 0xf287)` exactly like other movement blockers.

Line-derived entities are created after map-sprite entities. `Game_linkEntity()` inserts at the tile-list head, so reverse line order reproduces the first line blocker encountered for one tile.

Legacy line-entity placement uses the already recovered line geometry nudges before the entity midpoint is linked:

```text
render line nudge: +/-3 according to AXIS flags
entity cell nudge: -Y / +X / +Y / -X by one unit according to flags
midpoint -> tile index
```

For Junction line 35:

```text
305 + texture 7 = entity-def tileIndex 312
entity type = 0
trace mask 0xf287 includes type 0
linked tile = 975
```

Therefore `BACK 943->975` must be blocked.

## Permanent native implementation

### Bounded entity-definition type catalogue

New files:

```text
ESP32/include/esp_entity_def_type_catalog.h
ESP32/src/esp_entity_def_type_catalog.c
```

The catalogue is built directly from `/entities.db` in `/DoomRPG-ESP32.pak` during resident-map construction.

```text
lookup limit = 817 tileIndex values
storage = 817 B BSS
heap allocation = 0
runtime ZIP = no
record size = 24 B
stored value = entity eType only
```

The catalogue does not duplicate names, parms, subtypes, pointers, `Entity_t`, or the legacy 1024-entry entity database. It is reset with the resident lifecycle and rebuilt from the PAK when a map resident owner is created.

### Closed line collision

`EspNativeGameplayCollision_traceCardinalStep()` now includes closed line-derived entities in addition to the already proven tile WALL bit and compact map-sprite entity topology.

For one cardinal step it:

```text
validate native owners
 -> keep existing open-line fail-closed gate
 -> check static WALL cells
 -> recover source/destination closed line blockers from immutable compact lines
 -> resolve line entity eType through the bounded catalogue
 -> apply legacy midpoint/nudge placement
 -> apply legacy trace mask 0xf287
 -> then evaluate compact sprite entities as before
```

The public collision ABI remains unchanged:

```text
EspNativeGameplayCollisionResult = 16 B
```

`blockerSpriteIndex=65535` is retained for a line-derived blocker because the blocker is intentionally not represented as a fake map sprite.

Dynamic opened-line relinking is still outside this milestone. Any live opened native line continues to return `UNSUPPORTED_DYNAMIC_LINES` before closed-line tracing.

## Real-CYD hardware proof

Normal environment: `esp32-cyd`.

The corrected firmware produced the decisive witness:

```text
[LINECOLLISION] BLOCK source=943 dest=975 line=35 texture=7 flags=00000505 type=0 defTile=312
[MOVE] BLOCKED n=1 seq=1 action=BACK delta=0,64 tile=943->975 collision=ENTITY blocker=65535 type=0 frame=ba3e5182 exact=yes heap=38216->38216 largest=21492->21492 turnAdvance=no tileDispatch=no
```

This proves the complete intended behavior:

```text
position remains 992,1888
frame remains ba3e5182 exactly
BACK does not enter tile 975
closed arrival-door line 35 blocks movement
no world render is needed for the rejected move
heap8 remains 38216
largest8 remains 21492
turnAdvance=no
tileDispatch=no
```

The user confirmed on the physical CYD that the previously visible door bug is corrected.

Stable post-test heartbeats remained:

```text
heap=103884
heap8=38216
largest8=21492
SD=ready
VIDEO=ready
CORE=ready
```

The reduced absolute heap versus the previous milestone includes the new bounded catalogue and current diagnostics; absolute allocator values remain witnesses rather than semantic fingerprints.

## Corrected movement canon

Fresh Junction immediate movement is now:

```text
FORWARD      943->911 : CLEAR
BACK         943->975 : ENTITY / closed line 35 / type 0
STRAFE_LEFT  943->942 : WALL
STRAFE_RIGHT 943->944 : WALL
```

The historical MOVE milestone's `BACK=CLEAR` record remains useful as the hardware evidence that exposed the missing line-entity family, but it is no longer the current collision truth.

## Invariants preserved

```text
shapeData == NULL
mediaTexels == NULL
/DoomRPG-ESP32.pak remains native backing store
runtime ZIP access remains forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
no pointer-heavy legacy entityDb is constructed
EspNativeGameplayCollisionResult remains 16 B
blocked move mutates no player/view state
blocked move mutates no framebuffer
Game_advanceTurn = no
Game_executeTile = no
facing refresh = deferred
opened-line relinking = still fail-closed
```

## Diagnostic branch supersession

The older branch:

```text
agent/esp32-native-door-view-witness
tip = e04195e60a0499a4da3dc189eef98446d074fd92
base = 2aae0676528ab00c3494d142d8b35c22b7685dce
```

contains only two commits above the same base:

```text
948e9d173f0234fb3f8c3d3df4c6d0511246dbe2  add bounded door view renderer witness
e04195e60a0499a4da3dc189eef98446d074fd92  enable gameplay door view witness
```

Its only changed paths are:

```text
ESP32/platformio.ini                         +6 lines
ESP32/src/native_door_view_witness.c         +230 lines
```

That wrapper-based witness is superseded by the more precise direct diagnostics on this milestone branch and contains no permanent collision fix. It is not referenced by the authoritative current documentation. It may be abandoned/deleted after this branch is merged; no cherry-pick from it is required.

## Merge boundary

The firmware-bearing content actually tested on the physical CYD is:

```text
efea93977d20ba94e2fd5d6981ebce2e7916bc5b
```

Every commit after that SHA must remain documentation-only unless another firmware is flashed.

Status:

```text
REAL-CYD HARDWARE PASS
MERGE-READY after docs-only closeout audit
```

After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.

Do not preselect the next gameplay family from chat memory. Re-read `main`, `PORTING_STATUS.md`, `DOCUMENTATION.md`, and this milestone first. Dynamic opened-line relinking / door interaction and post-move turn/tile-event semantics remain separate bounded candidates.