# MAP1 native gameplay closed line-entity collision recovery

## Status

```text
branch = agent/esp32-native-gameplay-door-view-probe
base main = 2aae0676528ab00c3494d142d8b35c22b7685dce
base PR = #97 — shared-payload large range cache + bounded legacy wall guard
hardware-proven semantic-fix SHA = efea93977d20ba94e2fd5d6981ebce2e7916bc5b
post-review cleanup SHA = 5c01d91f9c6320460b2ecaf033f68a88bde80dfd
status = REAL-CYD RETEST REQUIRED
merge-ready = no
```

This milestone started as a renderer/view investigation around the Junction arrival door. The real CYD proved that the renderer was not the root cause. The visual corruption came from an incomplete native movement-collision model that allowed the player to step onto a closed legacy line entity.

The semantic fix itself is hardware-proven at `efea939...`. A later PR review correctly identified that the temporary door/BSP witness still performed a second full BSP visibility traversal and success-path console logging on every normal gameplay render. That diagnostic was removed after the hardware run, so the current branch head requires one final normal `esp32-cyd` retest before merge.

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

## Temporary renderer diagnosis

The bounded door-view diagnostics at the hardware-tested semantic-fix SHA proved the failing post-move frame itself was internally coherent:

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

This is interpreted correctly: the renderer was drawing a valid but nonsensical camera pose produced by collision semantics. No orientation sign, BSP side test, wall projection rule, sprite compositor rule, or `PlatformVideo_present()` behavior needed to be changed.

The same hardware run also revalidated all four cardinal rotations at the legal spawn and returned exactly to canonical North:

```text
N frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f
E frame=8cfdfe34 viewport=17c48c15 HUD=1d908304
S frame=da1c4297 viewport=582c2ad8 HUD=a78d0f96
W frame=23ee0954 viewport=de06a408 HUD=9281a6d1
N round-trip frame=ba3e5182 exact
```

### PR-review cleanup

The temporary direct door-view witness was useful for diagnosis but was not a permanent gameplay service. PR review #98 correctly flagged that the normal successful render path still called it unconditionally, causing a second `EspNativeBspVisibility_build()` traversal plus map-line scans and multiple `printf`s per MOVE/TURN.

The current candidate removes that cost completely:

```text
c8b39ab1dde922045391f160ab447b6f974ccfbb
  remove EspNativeDoorViewProbe_log() from EspNativeGameplayFrame_renderTurn()
  remove DOORVIEW BUILD/ARMED/postWorld success logs
  remove TURNFRAME SPRITES success log
  retain failure-only TURNFRAME diagnostics

1d84f58770087237020a5b3ecfbfc2bfe8fe7bde
  delete ESP32/src/esp_native_door_view_probe.c

5c01d91f9c6320460b2ecaf033f68a88bde80dfd
  delete ESP32/include/esp_native_door_view_probe.h
```

The normal gameplay path therefore performs only the actual world render; it no longer repeats BSP visibility merely for a solved diagnostic. Because this is firmware code after the last hardware-tested SHA, the current candidate is intentionally **not merge-ready until retested**.

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

Files:

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

`EspNativeGameplayCollision_traceCardinalStep()` includes closed line-derived entities in addition to the already proven tile WALL bit and compact map-sprite entity topology.

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

## Real-CYD semantic-fix proof

Normal environment: `esp32-cyd`.

Hardware-tested SHA `efea93977d20ba94e2fd5d6981ebce2e7916bc5b` produced the decisive witness:

```text
[LINECOLLISION] BLOCK source=943 dest=975 line=35 texture=7 flags=00000505 type=0 defTile=312
[MOVE] BLOCKED n=1 seq=1 action=BACK delta=0,64 tile=943->975 collision=ENTITY blocker=65535 type=0 frame=ba3e5182 exact=yes heap=38216->38216 largest=21492->21492 turnAdvance=no tileDispatch=no
```

This proves the intended collision behavior:

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

These values certify `efea939...`; they are not yet a hardware certificate for post-review cleanup SHA `5c01d91...`.

## Corrected movement canon

Fresh Junction immediate movement is:

```text
FORWARD      943->911 : CLEAR
BACK         943->975 : ENTITY / closed line 35 / type 0
STRAFE_LEFT  943->942 : WALL
STRAFE_RIGHT 943->944 : WALL
```

The historical MOVE milestone's `BACK=CLEAR` record remains useful as the hardware evidence that exposed the missing line-entity family, but it is no longer the current collision truth.

## Invariants preserved by the semantic fix

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

## Superseded diagnostic branch

The older branch:

```text
agent/esp32-native-door-view-witness
tip = e04195e60a0499a4da3dc189eef98446d074fd92
base = 2aae0676528ab00c3494d142d8b35c22b7685dce
```

contains only two commits above the same base and changes only:

```text
ESP32/platformio.ini                         +6 lines
ESP32/src/native_door_view_witness.c         +230 lines
```

That wrapper-based witness is superseded. The more precise direct witness used on this milestone has itself now been retired from the normal gameplay build after finding the root cause. No code from `agent/esp32-native-door-view-witness` is required by the permanent collision fix.

Disposition: **safe to abandon/delete; no cherry-pick required**.

## Merge boundary

Last hardware-tested firmware-bearing SHA:

```text
efea93977d20ba94e2fd5d6981ebce2e7916bc5b
```

Current post-review firmware candidate:

```text
5c01d91f9c6320460b2ecaf033f68a88bde80dfd
```

Code changed after the hardware-tested SHA specifically to remove completed diagnostics from the production render path. Therefore:

```text
REAL-CYD SEMANTIC FIX PROVEN
CURRENT HEAD RETEST REQUIRED
MERGE-READY = NO until retest passes
```

Required retest is intentionally small: normal `esp32-cyd`, reach Junction, verify spawn BACK is still blocked by line 35, perform a MOVE/TURN render, confirm no `DOORVIEW` success spam and no failure/reboot/heap drift. After PASS, update docs only and declare merge-ready.
