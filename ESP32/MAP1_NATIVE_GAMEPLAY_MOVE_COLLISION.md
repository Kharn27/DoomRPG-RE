# MAP1 native gameplay cardinal movement + collision

## Status

```text
branch = agent/esp32-native-move-collision
base   = 89f9d5f3feaa40f2e2a0c6e9506d1d8efaf5eeb6
base PR = #93 — native gameplay TURN dispatcher
hardware-tested implementation SHA = becaa1ec5bdd68311fa2e1d626fc238d1a706779
status = REAL-CYD HARDWARE PASS
merge-ready = yes after documentation-only closeout audit
```

The final hardware-tested SHA includes the bounded wall-raster fix that keeps native cached wall texel coordinates local instead of reconstructing the legacy map-wide `mediaTexels` address space.

## Objective

Add the first native gameplay translation family on classic CYD:

```text
FORWARD
BACK
STRAFE_LEFT
STRAFE_RIGHT
```

Each action derives exactly one cardinal 64-unit step from the already hardware-proven gameplay orientation owner. A clear move mutates only the settled native `EspPlayerViewState` X/Y position, then recomposes the native Junction frame on a later lifecycle service.

Collision reproduces the current legacy `Game_trace` movement blocker boundary from compact native data:

- static wall occupancy from native tile flags;
- linked compact sprite/entity topology for entity types in the recovered movement trace mask;
- special crossing-plane semantics for legacy entity types 14/15;
- any opened dynamic native line remains fail-closed until line/entity relinking has its own milestone.

This milestone deliberately does **not** execute:

```text
Game_eventFlagsForMovement
Game_executeTile
Game_advanceTurn
entity/monster activation
facing refresh
legacy PLAYING loop
```

## Permanent native APIs

New permanent collision boundary:

```text
ESP32/include/esp_native_gameplay_collision.h
ESP32/src/esp_native_gameplay_collision.c
```

Extended permanent gameplay dispatch / player-view ownership:

```text
ESP32/include/esp_native_gameplay_dispatch.h
ESP32/src/esp_native_gameplay_dispatch.c
ESP32/include/esp_player_view_state.h
ESP32/src/esp_player_view_state.c
```

Temporary strict hardware probe:

```text
ESP32/include/native_junction_move_collision_probe.h
ESP32/src/native_junction_move_collision_probe.c
```

The existing TURN probe remains live at arbitrary settled tile-center positions so translation and rotation compose without entering legacy gameplay.

## Compact state sizes

Final real-CYD witness:

```text
EspNativeGameplayCollisionResult = 16 B
EspNativeGameplayMoveResult      = 24 B
EspPlayerViewState               = 44 B
EspNativeGameplayFrameStats      = 84 B
movement probe execScratch       = 520 B static scratch
TURN probe execScratch           = 464 B static scratch
stackHighWater                   = 172
```

The heavy execution workspaces are static bounded probe scratch rather than `loopTask` stack growth.

## Cardinal step semantics

The gameplay orientation owner defines the permanent cardinal vectors:

```text
angle 0   / E: forward +64,0
angle 64  / N: forward 0,-64
angle 128 / W: forward -64,0
angle 192 / S: forward 0,+64
```

`BACK` negates forward. Strafes rotate the forward vector by one cardinal quarter-turn. Position coordinates must remain settled tile centers (`value & 63 == 32`) and a successful move changes exactly one axis by 64 units.

Prepare/commit is stale-checked and rollback-capable. Blocked movement never commits the candidate view.

## Collision boundary

At fresh Junction spawn (`tile=943`, `pos=992,1888`, angle 64/N), the strict probe enumerated all four immediate actions:

```text
FORWARD      delta=0,-64 tile 943->911 flags=08 status=CLEAR
BACK         delta=0,+64 tile 943->975 flags=1c status=CLEAR
STRAFE_LEFT  delta=-64,0 tile 943->942 flags=01 status=WALL
STRAFE_RIGHT delta=+64,0 tile 943->944 flags=01 status=WALL
openLines=0
```

The collision result is allocation-free and does not build a pointer-heavy legacy entity database. It walks compact native topology and preserves legacy movement blocker ordering where multiple linked blockers are present.

Opened native lines deliberately return `UNSUPPORTED_DYNAMIC_LINES`: collision relinking for doors/dynamic line entities is not guessed in this milestone.

## Input / lifecycle scheduling

The hardware-proven invisible input layer remains the sole touch callback owner.

A clear movement action follows:

```text
physical tap
 -> semantic input intent
 -> 120 ms neon/glyph feedback
 -> exact restore of the current dynamic frame
 -> queue MOVE
 -> return lifecycle
 -> collision trace
 -> commit native player/view X/Y
 -> native render on later service
```

A blocked movement follows the same feedback/restore path but stops after collision and keeps position/frame exact.

No world renderer runs from the touch callback.

## Real-CYD hardware proof

Hardware-tested firmware:

```text
becaa1ec5bdd68311fa2e1d626fc238d1a706779
normal env = esp32-cyd
```

Final stable memory witness during gameplay actions:

```text
heap8    = 66708 -> 66708
largest8 = 29684 -> 29684
stackHighWater = 172
```

Absolute free-heap values are allocator witnesses, not semantic fingerprints.

### Fresh spawn -> FORWARD

```text
action=FORWARD
position 992,1888 -> 992,1824
tile 943 -> 911
frame ba3e5182 -> 66da9d16
viewFNV afcdcf74 -> 5bf915f4
walls=32 / 4384 pixels
planes=12800
sprites=19 / 2097 pixels
glows=5 / 112 pixels
spriteReads=148
collision=CLEAR
legacyStable=yes
residentStable=yes
orientationStable=yes
turnAdvance=no
tileDispatch=no
```

This proves true native translation from the canonical gameplay frame.

### TURN at moved tile center

At `pos=992,1824`, `TURN_LEFT` remained fully live:

```text
angle 64 -> 128
frame 66da9d16 -> ec232716
viewFNV 5bf915f4 -> abce5af4
walls=5 / 8600 pixels
planes=12800
legacyStable=yes
residentStable=yes
```

The TURN runtime boundary therefore composes with movement and no longer assumes canonical spawn coordinates.

### Entity collision witness

From tile 911 facing West, a FORWARD attempt was rejected by compact entity topology:

```text
action=FORWARD
tile 911 -> 910
collision=ENTITY
blockerSpriteIndex=24
blockerType=7
frame=ec232716 exact=yes
heap=66708->66708
largest=29684->29684
turnAdvance=no
tileDispatch=no
```

The blocked action changed neither view nor framebuffer.

### Strafe after rotation

The next `STRAFE_RIGHT` from angle 128 succeeded:

```text
position 992,1824 -> 992,1760
tile 911 -> 879
delta=0,-64
frame ec232716 -> 50c26281
viewFNV abce5af4 -> 7b20f2bc
walls=5 / 7237 pixels
planes=12800
legacyStable=yes
residentStable=yes
orientationStable=yes
```

This proves strafe vectors are derived from the live gameplay orientation, not hard-coded to Junction North.

### Previously failing moved-position TURN

Earlier bring-up failed when rendering a TURN from the moved position `992,1760`. The final hardware-tested SHA fixed that renderer path and completed the same class of view change:

```text
TURN_RIGHT
angle 128 -> 64
position remains 992,1760
frame 50c26281 -> fc7a5142
walls=30 / 4411 pixels
planes=12800
sprites=15 / 1123 pixels
glows=3 / 199 pixels
legacyStable=yes
residentStable=yes
```

No probe failure followed.

### Continued free movement / rotation

The user continued moving and turning beyond the original reproduction path:

```text
FORWARD: pos 992,1760 -> 992,1696 / tile 879->847
TURN_RIGHT: angle 64 -> 0 at pos 992,1696
FORWARD: pos 992,1696 -> 1056,1696 / tile 847->848
```

All actions completed with stable heap/largest and no `FAILED` marker.

`SELECT` was also tapped twice during the same run. It produced input feedback and exact restore only; no gameplay interaction was dispatched, preserving the fail-closed milestone boundary.

## Native wall-cache raster fix

The final code change before hardware PASS is:

```text
becaa1ec5bdd68311fa2e1d626fc238d1a706779
fix(esp32): raster wall cache in local texel space
```

The native wall cache already owns one bounded 2048-byte packed texture at a time. The older bridge raster nevertheless reconstructed a legacy map-wide texel coordinate using `sourceTexelOffset`, then subtracted that base again in the local sampler.

That was both unnecessary for the native architecture and unsafe for large source offsets because the intermediate 32-bit fixed-point coordinate could overflow for newly visible textures.

The final renderer keeps:

- `sourceTexelOffset` for PAK range lookup and cache identity;
- local 0..4095 texel coordinates for raster sampling.

This preserves the native no-`mediaTexels` architecture and removes the map-wide addressing artifact instead of emulating it.

## Side-effect / invariant proof

Successful and blocked movement in the final run preserved:

```text
shapeData == NULL
mediaTexels == NULL
runtime PAK backing only for migrated graphics/map data
legacy Game.entities == 0
legacy Game.monsters == 0
legacy Game/Player/Render mutation == none
resident snapshot stable
orientation stable across MOVE
position stable across blocked MOVE
Game_advanceTurn == no
Game_executeTile == no
facingRefresh == deferred
```

No movement callback renders. No unsupported action is silently executed.

## Performance note

The milestone is functionally correct on hardware but intentionally **not performance-complete**. The user reports gameplay movement/turning as very slow.

The current diagnostic hot path still performs substantial redundant work for every successful MOVE/TURN:

```text
input feedback full present
 -> exact feedback-restore full present
 -> rebuild/re-read native world caches
 -> full world + sprite/glow recomposition
 -> temporary 12.8 KiB HUD band save/restore bridge
 -> final full present
```

Representative full presents are about 34 ms each on the tested CYD, while world recomposition also re-reads bounded wall/plane/sprite data from the PAK. The neon hold itself is 120 ms.

Do **not** optimize `PlatformVideo_present()` blindly. The coherent future performance frontier is to reduce redundant render work while preserving exact frame canons:

- permanent bounded wall/plane cache ownership rather than rebuilding transient caches each action;
- viewport-only native world redraw so HUD does not require a temporary 12.8 KiB save;
- preserve on-demand turn-based redraw scheduling;
- keep a single final gameplay presentation after semantic execution;
- retain exact feedback restore and all no-allocation/no-map-wide invariants.

This optimization should be a separate milestone from gameplay semantics.

## Hardware PARK after this milestone

```text
legacyState=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeST_PLAYING=yes
nativePlayingService=yes
nativeHud=yes
nativeInput=yes
nativeTurnDispatch=yes
nativeMovementDispatch=yes
TURN_LEFT/RIGHT=yes
FORWARD/BACK/STRAFE native semantics=yes
static wall collision=yes
compact linked entity collision=yes
dynamic opened-line collision=fail-closed
SELECT execution=no
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
legacy Game.entities=0
legacy Game.monsters=0
```

## Still intentionally outside

```text
post-move Game_eventFlagsForMovement
post-move tile event execution
actual Game_advanceTurn semantics
dynamic line/entity collision relinking / opened doors
SELECT interaction
weapon switching execution
PASS_TURN execution
MENU/AUTOMAP gameplay execution
entity/monster activation and AI
facing refresh after gameplay actions
first-person weapon overlay
native durable save persistence
sound playback
```

## Next bounded milestone

After merge, recover the exact new `main` SHA before branching.

Because cardinal movement and TURN are now functionally hardware-proven but visibly slow, the preferred next frontier is a **native gameplay render hot-path milestone**: preserve the exact current semantics/frame outputs while replacing repeated transient cache/HUD-save work with bounded permanent renderer ownership and viewport-only redraw. This is deliberately separate from `PlatformVideo_present()` optimization and from new gameplay behavior.

If gameplay semantics are prioritized instead, the next bounded behavioral family is post-move turn/tile-event handling. It must be recovered from legacy first and must not enable broad entity/monster gameplay at the same time.