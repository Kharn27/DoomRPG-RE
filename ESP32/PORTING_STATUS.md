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
hardware-tested implementation SHA = 5c01d91f9c6320460b2ecaf033f68a88bde80dfd
status = REAL-CYD HARDWARE PASS
merge-ready = yes after docs-only closeout audit
```

Evidence: [`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md).

The Junction arrival-door symptom is closed. The renderer was correct; native movement collision had allowed `BACK` from spawn into tile `975`, exactly onto closed line `35`. The permanent fix adds bounded line-derived collision semantics. PR #98 review then removed the completed diagnostic BSP witness from the normal gameplay hot path, and that production cleanup is now also hardware-revalidated.

## Current hardware root-cause proof

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

The earlier native MOVE collision family modeled static WALL cells plus compact map-sprite entities but omitted line-derived entities. That omission incorrectly produced `BACK 943->975 = CLEAR`.

Current real-CYD truth at `5c01d91...`:

```text
[LINECOLLISION] BLOCK source=943 dest=975 line=35 texture=7 flags=00000505 type=0 defTile=312
[MOVE] BLOCKED ... tile=943->975 collision=ENTITY blocker=65535 type=0 frame=ba3e5182 exact=yes heap=38928->38928 largest=29684->29684
```

Corrected immediate Junction movement canon:

```text
FORWARD      943->911 : CLEAR
BACK         943->975 : ENTITY / closed line 35 / type 0
STRAFE_LEFT  943->942 : WALL
STRAFE_RIGHT 943->944 : WALL
```

The blocked BACK mutates neither player position nor framebuffer.

## PR #98 production cleanup — hardware proven

Review correctly flagged that the temporary door witness still performed a second `EspNativeBspVisibility_build()` plus map-line scans and success-path `printf`s on every normal gameplay render.

Cleanup commits:

```text
c8b39ab1dde922045391f160ab447b6f974ccfbb
  remove EspNativeDoorViewProbe_log() from gameplay render
  remove DOORVIEW success-path logs
  remove TURNFRAME SPRITES success log
  retain failure-only TURNFRAME diagnostics

1d84f58770087237020a5b3ecfbfc2bfe8fe7bde
  delete esp_native_door_view_probe.c

5c01d91f9c6320460b2ecaf033f68a88bde80dfd
  delete esp_native_door_view_probe.h
```

The final CYD retest proves the cleanup:

```text
no [DOORVIEW] output
no [TURNFRAME] ... fail=
no Guru Meditation / reboot
heap8=38928 stable
largest8=29684 stable
```

Four successful TURN renders exercised the same cleaned `EspNativeGameplayFrame_renderTurn()` path and returned exactly to canonical North:

```text
N frame=ba3e5182 viewport=9206eb24 HUD=6c2aa46f
W frame=23ee0954 viewport=de06a408 HUD=9281a6d1
S frame=da1c4297 viewport=582c2ad8 HUD=a78d0f96
E frame=8cfdfe34 viewport=17c48c15 HUD=1d908304
N round-trip=exact
```

Representative final North render:

```text
tempHud=0 B
routeNoPresent=1
finalPresent=1
world=180664 us
sprite=9595 us
hud=1321 us
present=35034 us
total=236218 us
stackHighWater=860
legacyStable=yes
residentStable=yes
```

The supplied cleanup retest excerpt does not contain a successful CLEAR MOVE render; it contains the intentional blocked-door MOVE plus successful TURN renders through the same shared cleaned renderer. Do not invent a missing CLEAR-MOVE fingerprint.

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

It exists so native consumers can reproduce entity-type decisions without constructing legacy `EntityDef_t`, `Entity_t`, or pointer-heavy `entityDb` owners.

Closed line collision reproduces legacy line placement, including the recovered `+/-3` geometry nudge and one-unit entity-link nudge. Public ABI remains:

```text
EspNativeGameplayCollisionResult = 16 B
```

For a line-derived blocker, `blockerSpriteIndex=65535` means deliberately “not a map sprite”.

## Open-line boundary remains fail-closed

This milestone adds only closed line-derived collision. Dynamic line/entity relinking is still intentionally absent:

```text
if native lineState.openCount != 0
 -> ESP_NATIVE_GAMEPLAY_COLLISION_UNSUPPORTED_DYNAMIC_LINES
```

Do not infer door-open collision semantics until a dedicated milestone recovers them.

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

Final hardware witness for the current implementation:

```text
heap=104596 stable
heap8=38928 stable
largest8=29684 stable
TURN legacyStable=yes
residentStable=yes
turnAdvance=no
tileDispatch=no
shapeData=NULL
mediaTexels=NULL
```

Absolute allocator values are witnesses, not semantic fingerprints.

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

The wall packed-index guard remains hardware-proven and unchanged by this milestone.

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

It contains only the obsolete wrapper-based renderer witness and no permanent collision fix. No cherry-pick is required.

Disposition: **safe to abandon/delete**.

## Current hardware PARK

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
REAL-CYD HARDWARE PASS
hardware-tested implementation SHA = 5c01d91f9c6320460b2ecaf033f68a88bde80dfd
MERGE-READY = YES after docs-only closeout audit
```

All commits after `5c01d91...` must remain documentation-only for this hardware certificate to stay valid. After merge, recover the exact new `main` SHA and reread this file, `DOCUMENTATION.md`, and [`MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md`](MAP1_NATIVE_GAMEPLAY_CLOSED_LINE_COLLISION.md) before branching again.
