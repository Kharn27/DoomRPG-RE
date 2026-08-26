# Doom RPG ESP32 CYD porting status

Authoritative recovery point for the classic ESP32-2432S028R port. Current GitHub `main` + this file + `DOCUMENTATION.md` + the latest relevant milestone archive override chat memory.

## Latest merged hardware baseline

```text
PR   = #97 — shared-payload large range cache + bounded legacy wall guard
main = 2aae0676528ab00c3494d142d8b35c22b7685dce
status = MERGED
```

Merged evidence: [`MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md`](MAP1_NATIVE_GAMEPLAY_LARGE_RANGE_CACHE.md).

## Current candidate milestone

```text
branch = agent/esp32-native-gameplay-door-view-probe
base   = 2aae0676528ab00c3494d142d8b35c22b7685dce
hardware-proven semantic-fix SHA = efea93977d20ba94e2fd5d6981ebce2e7916bc5b
post-review firmware SHA = 5c01d91f9c6320460b2ecaf033f68a88bde80dfd
status = REAL-CYD RETEST REQUIRED
merge-ready = no
```

Evidence: [`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md).

The Junction arrival-door symptom is understood and the collision correction is hardware-proven: native movement had allowed `BACK` from spawn into tile `975`, exactly onto closed line `35`. The current firmware candidate additionally removes the now-completed diagnostic BSP witness from the normal gameplay hot path after PR #98 review; that post-test code cleanup requires one final real-CYD retest before merge.

## Hardware-proven root cause

Fresh Junction canon:

```text
player=992,1888,36
angle=64 / North
tile=943
frame=ba3e5182
viewport=9206eb24
HUD=6c2aa46f
```

Door line:

```text
line=35
texture=7
raw=960,1952 -> 1024,1952
flags=00000505
midpoint=992,1952
def lookup=305+7=312
entity type=0
linked tile=975
```

Recovered legacy behavior:

```text
Game_loadMapEntities()
 -> EntityDef_lookup(305 + line.texture)
 -> create/link line-derived Entity_t
 -> Game_trace(..., 62087 / 0xf287) sees the closed line entity
```

The old native movement milestone modeled static WALL cells plus map-sprite entity topology, but omitted line-derived entities. That omission incorrectly classified:

```text
BACK 943->975 = CLEAR
```

Hardware-tested semantic fix `efea939...` produced:

```text
[LINECOLLISION] BLOCK source=943 dest=975 line=35 texture=7 flags=00000505 type=0 defTile=312
[MOVE] BLOCKED ... tile=943->975 collision=ENTITY blocker=65535 type=0 frame=ba3e5182 exact=yes heap=38216->38216 largest=21492->21492
```

Therefore the corrected immediate Junction movement canon is:

```text
FORWARD      943->911 : CLEAR
BACK         943->975 : ENTITY / closed line 35 / type 0
STRAFE_LEFT  943->942 : WALL
STRAFE_RIGHT 943->944 : WALL
```

The player remains at `992,1888` and the framebuffer remains exactly `ba3e5182` on the blocked BACK. The user confirmed on the physical CYD that the huge arrival-door view disappears when this illegal movement is blocked.

## Renderer diagnosis closed

Temporary direct post-world diagnostics at `efea939...` proved the renderer/compositor was stable even at the formerly illegal pose:

```text
world rendered=1
world presented=0
HUD exact=yes
sprite accounting complete=yes
unsupported=0
render scratch stable=yes
final present succeeds
```

The formerly observed `4 walls / 17120 pixels` close-up was caused by the player being moved onto the door midpoint, leaving the 16-unit gameplay camera extremely close to the wall. Do not change camera orientation, BSP side tests, wall projection, sprite composition, or `PlatformVideo_present()` to address this resolved symptom.

The same hardware run revalidated cardinal TURN canons and exact North round trip:

```text
N frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f
E frame=8cfdfe34 viewport=17c48c15 HUD=1d908304
S frame=da1c4297 viewport=582c2ad8 HUD=a78d0f96
W frame=23ee0954 viewport=de06a408 HUD=9281a6d1
N round-trip=exact
```

## PR #98 review cleanup — RETEST REQUIRED

Code review correctly flagged that the temporary door witness still ran on every successful gameplay render, causing a second full `EspNativeBspVisibility_build()` traversal plus map-line scans and success-path console I/O.

The current firmware candidate removes the completed diagnostic from production gameplay:

```text
c8b39ab1dde922045391f160ab447b6f974ccfbb
  EspNativeGameplayFrame_renderTurn():
  - no EspNativeDoorViewProbe_log()
  - no DOORVIEW BUILD/ARMED/postWorld success logs
  - no TURNFRAME SPRITES success log
  - failure-only TURNFRAME diagnostics retained

1d84f58770087237020a5b3ecfbfc2bfe8fe7bde
  delete ESP32/src/esp_native_door_view_probe.c

5c01d91f9c6320460b2ecaf033f68a88bde80dfd
  delete ESP32/include/esp_native_door_view_probe.h
```

The normal gameplay render now executes only its real world pass rather than repeating BSP visibility for an already-solved witness.

Because these are firmware changes after `efea939...`, the previous PASS does **not** certify the current HEAD. Required final retest:

```text
normal env = esp32-cyd
reach Junction
BACK at spawn remains BLOCKED by line 35 / type 0
perform at least one successful TURN and one successful MOVE render
no DOORVIEW success-path spam
no TURNFRAME failure
no Guru Meditation / reboot
heap/resident state stable
```

## Permanent compact entity-definition owner

Files:

```text
ESP32/include/esp_entity_def_type_catalog.h
ESP32/src/esp_entity_def_type_catalog.c
```

Contract:

```text
source=/entities.db inside /DoomRPG-ESP32.pak
lookup limit=817 tileIndex values
storage=817 B BSS
heap allocation=0
stored metadata=eType only
runtime ZIP=no
```

It exists so native consumers can reproduce entity-type decisions without creating legacy `EntityDef_t` arrays, `Entity_t` objects, or the 1024-pointer `entityDb`.

Closed line collision reproduces legacy line placement, including recovered render geometry nudges and the one-unit entity-link nudge. Public ABI remains:

```text
EspNativeGameplayCollisionResult = 16 B
```

For a line-derived blocker, `blockerSpriteIndex=65535` means deliberately “not a map sprite”.

## Open-line boundary remains fail-closed

This milestone adds only the initial closed-line topology required by legacy collision. It does **not** implement dynamic line/entity relinking.

```text
if native lineState.openCount != 0
 -> ESP_NATIVE_GAMEPLAY_COLLISION_UNSUPPORTED_DYNAMIC_LINES
```

Do not infer door-open collision semantics until that family gets its own bounded milestone.

## Permanent hardware / memory invariants

```text
board       = ESP32-2432S028R classic CYD
MCU         = ESP32-D0WD-V3 dual core 240 MHz
flash       = 4 MB
PSRAM       = none
framebuffer = 160x120 RGB565 = 38400 B
shapeData   = NULL
mediaTexels = NULL
runtime ZIP = forbidden for migrated map/graphics paths
backing     = /DoomRPG-ESP32.pak
legacy Game.entities = 0
legacy Game.monsters = 0
native ST_PLAYING = reached
legacy ST_PLAYING = not reached
native PLAYING service = reached
broad legacy PLAYING loop = forbidden
```

Last hardware witness, valid for `efea939...`:

```text
heap=103884 stable
heap8=38216 stable
largest8=21492 stable
TURN/MOVE legacyStable=yes
residentStable=yes
turnAdvance=no
tileDispatch=no
shapeData=NULL
mediaTexels=NULL
```

Absolute heap values are witnesses, not semantic fingerprints. New absolute values after diagnostic removal must come from the retest; do not predict them.

## Stable Junction resident canons

```text
resourceMapId=9 / /junction.bsp
gameplayLoadMapId=2
sourceBytes=21051
crc32=4a2c5800
sourceFNV=fefaf5ca
spawnIndex=943
spawnDirection=64
payload=10410 B
runtimeFNV=bc432a0f
mapFNV=8dba0bb4
scriptFNV=bc9b18ff
lineFNV=3658710d
textureStateFNV=537319ad
automapFNV=b699bd75
topologyFNV=d6e8df7d
snapshotFNV=bb714d80
```

Canonical native gameplay framebuffer:

```text
frame=ba3e5182
viewport=9206eb24
HUD bands=6c2aa46f
HUD stateFNV=4756db9c
```

## Renderer/storage baseline inherited from merged PR #97

```text
persistent PAK/render owner=yes
small exact cache <=1024 B=yes
shared exact 2048 B tail cache=yes
large retained slots=3
canonical warm reads=19 x 2048 B
saved versus predecessor=3 reads / 6144 B
legacy cross-block wall guard=16 B BSS
shapeData=NULL
mediaTexels=NULL
```

The earlier wall packed-index recovery remains hardware-proven and unchanged by this milestone.

## Native transition / gameplay chain

Hardware-proven high-level chain:

```text
native map transition/residency
 -> spawn/player/view ownership
 -> post-load native ST_PLAYING
 -> PLAYING service
 -> sparse graphics catalog
 -> walls + textured planes
 -> BSP-visible billboards + glows
 -> native HUD
 -> calibrated touch intent
 -> TURN_LEFT/TURN_RIGHT
 -> FORWARD/BACK/STRAFE
 -> static WALL collision
 -> compact sprite-entity collision
 -> closed line-entity collision
 -> viewport-only gameplay recomposition
 -> persistent bounded render-resource owner
```

Still intentionally absent:

```text
Game_eventFlagsForMovement
post-move tile event execution
actual Game_advanceTurn semantics
dynamic opened-line/entity relinking
SELECT interaction / door use
weapon switching execution
PASS_TURN execution
MENU/AUTOMAP gameplay execution
entity/monster activation and AI
facing refresh after gameplay actions
first-person weapon overlay
native durable save storage
sound playback
```

Generic `EspMapOpcodeExecutor` remains limited to opcodes `11/19/20` and fail-closes all others.

## Superseded diagnostic branch

The unmerged branch:

```text
agent/esp32-native-door-view-witness
tip=e04195e60a0499a4da3dc189eef98446d074fd92
base=2aae0676528ab00c3494d142d8b35c22b7685dce
```

is exactly two commits ahead of the same base and changes only:

```text
ESP32/platformio.ini                  +6
ESP32/src/native_door_view_witness.c  +230
```

It contains only the older wrapper-based renderer witness. The later direct witness also served its purpose and has now been removed from the normal candidate. Neither diagnostic branch/file is required by the permanent closed-line collision fix.

Disposition: **safe to abandon/delete; no cherry-pick required**.

## Current hardware PARK

The following semantic state is proven by `efea939...`; the production-cleanup firmware must revalidate it before merge:

```text
legacyState=9 / ST_INTRO
page=3
targetMap=9
junctionResident=yes
nativeST_PLAYING=yes
nativePlayingService=yes
nativeGraphicsCatalog=yes
nativeFirstFrame=yes
texturedPlanes=yes
nativeBaseBillboards=yes
glowCompanions=yes
nativeHud=yes
nativeInput=yes
nativeTurnDispatch=yes
nativeMovementDispatch=yes
nativeGameplayViewportHotPath=yes
persistentRenderResourceOwner=yes
smallExactRangeCache=yes
largeExact2048RangeCache=yes
legacyWallGuard=yes
static wall collision=yes
compact linked sprite-entity collision=yes
closed line-entity collision=yes
spawn BACK blocked by line 35=yes
dynamic opened-line collision=fail-closed
TURN canonical round-trip=exact
legacy Game.entities=0
legacy Game.monsters=0
Game_advanceTurn=no
Game_executeTile=no
facingRefresh=deferred
```

## Merge recommendation

```text
REAL-CYD semantic fix = PASS at efea93977d20ba94e2fd5d6981ebce2e7916bc5b
current firmware candidate = 5c01d91f9c6320460b2ecaf033f68a88bde80dfd
current candidate retest = REQUIRED
MERGE-READY = NO
```

After the current firmware candidate passes on the real CYD, only documentation may change before merge. Then record the exact tested SHA and declare merge-ready.
