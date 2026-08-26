# MAP1 native gameplay closed line-entity collision recovery

## Status

```text
branch = agent/esp32-native-gameplay-door-view-probe
base main = 2aae0676528ab00c3494d142d8b35c22b7685dce
base PR = #97 — shared-payload large range cache + bounded legacy wall guard
hardware-tested implementation SHA = 5c01d91f9c6320460b2ecaf033f68a88bde80dfd
status = REAL-CYD HARDWARE PASS
merge-ready = yes after docs-only closeout audit
```

This milestone started as a renderer/view investigation around the Junction arrival door. Real CYD evidence proved the renderer was not the root cause. Native movement collision was incomplete and allowed the player to step exactly onto a closed legacy line entity.

The first semantic-fix firmware `efea93977d20ba94e2fd5d6981ebce2e7916bc5b` proved the collision correction. PR #98 review then correctly identified that the completed door-view diagnostic still performed a second BSP visibility traversal plus success-path logging on every normal gameplay render. That temporary witness was removed from production and the cleaned firmware `5c01d91f9c6320460b2ecaf033f68a88bde80dfd` was reflashed and revalidated on the physical classic CYD.

## Original symptom and root cause

Fresh Junction spawn:

```text
player = 992,1888,36
angle = 64 / North
tile = 943
frame = ba3e5182
viewport = 9206eb24
HUD = 6c2aa46f
```

The historical MOVE milestone classified:

```text
BACK 943->975 flags=1c CLEAR
```

That committed the player to `992,1952`, exactly the midpoint of closed arrival-door line 35:

```text
line = 35
texture = 7
raw = 960,1952 -> 1024,1952
flags = 00000505
midpoint = 992,1952
entity-def lookup = 305 + 7 = 312
entity type = 0
linked tile = 975
```

With the 16-unit gameplay camera offset, the wall then sat almost on the camera and filled the viewport. The huge close-up door was therefore a valid render of an illegal gameplay pose, not a projection, BSP, orientation, sprite-compositor, or presentation bug.

Temporary post-world diagnostics proved the formerly illegal frame itself was coherent:

```text
world rendered=1
world presented=0
HUD exact=yes
sprite accounting complete=yes
unsupported=0
render scratch stable=yes
final present succeeds
```

No renderer semantic change was needed.

## Recovered legacy collision behavior

Legacy `Game_loadMapEntities()` materializes qualifying map lines as entities:

```text
EntityDef_lookup(305 + line.texture)
```

If no definition exists and fallback flags `0x18` are present, the static wall definition is used. The created line entity is linked into `Game.entityDb` and participates in `Game_trace(..., 62087 / 0xf287)`.

Native collision now reproduces the needed permanent semantics without creating legacy entities or a pointer-heavy database:

```text
static WALL cell
 -> closed line-derived entities
 -> compact map-sprite entities
```

Line placement reproduces the legacy geometry `+/-3` nudge followed by the one-unit line-entity link nudge before midpoint-to-tile conversion. Reverse line order matches the legacy head-insert ordering for line entities.

For line 35, entity type 0 is included by trace mask `0xf287`; therefore `BACK 943->975` must be blocked.

## Permanent compact entity-definition owner

Files:

```text
ESP32/include/esp_entity_def_type_catalog.h
ESP32/src/esp_entity_def_type_catalog.c
```

Contract:

```text
source = /entities.db inside /DoomRPG-ESP32.pak
record size = 24 B
lookup limit = 817 tileIndex values
storage = 817 B BSS
heap allocation = 0
stored metadata = eType only
runtime ZIP = no
```

No names, parms, subtypes, pointers, `Entity_t`, or 1024-entry legacy `entityDb` are retained.

`EspNativeGameplayCollisionResult` remains exactly `16 B`. A line-derived blocker reports `blockerSpriteIndex=65535` because it is deliberately not represented as a fake map sprite.

Dynamic opened-line relinking remains outside this milestone. Any live opened native line still fail-closes through `ESP_NATIVE_GAMEPLAY_COLLISION_UNSUPPORTED_DYNAMIC_LINES`.

## First real-CYD semantic-fix proof

Firmware `efea93977d20ba94e2fd5d6981ebce2e7916bc5b` produced:

```text
[LINECOLLISION] BLOCK source=943 dest=975 line=35 texture=7 flags=00000505 type=0 defTile=312
[MOVE] BLOCKED n=1 seq=1 action=BACK delta=0,64 tile=943->975 collision=ENTITY blocker=65535 type=0 frame=ba3e5182 exact=yes heap=38216->38216 largest=21492->21492 turnAdvance=no tileDispatch=no
```

The user confirmed on the physical CYD that the arrival-door visual bug was gone.

## PR #98 production cleanup

Review correctly flagged the temporary door witness as inappropriate for the normal gameplay hot path. Cleanup commits:

```text
c8b39ab1dde922045391f160ab447b6f974ccfbb
  remove EspNativeDoorViewProbe_log() from EspNativeGameplayFrame_renderTurn()
  remove DOORVIEW BUILD/ARMED/postWorld success logs
  remove TURNFRAME SPRITES success log
  keep failure-only TURNFRAME diagnostics

1d84f58770087237020a5b3ecfbfc2bfe8fe7bde
  delete ESP32/src/esp_native_door_view_probe.c

5c01d91f9c6320460b2ecaf033f68a88bde80dfd
  delete ESP32/include/esp_native_door_view_probe.h
```

The normal gameplay path now performs only the real world render; there is no diagnostic second BSP traversal.

## Final real-CYD cleanup retest

Normal environment: `esp32-cyd`.

The cleaned firmware `5c01d91f9c6320460b2ecaf033f68a88bde80dfd` was flashed on the real classic CYD.

Fresh Junction neighbor census is now exact:

```text
FORWARD      943->911 : CLEAR
BACK         943->975 : ENTITY / line 35 / type 0
STRAFE_LEFT  943->942 : WALL
STRAFE_RIGHT 943->944 : WALL
```

The decisive blocked-door witness remains:

```text
[LINECOLLISION] BLOCK source=943 dest=975 line=35 texture=7 flags=00000505 type=0 defTile=312
[MOVE] BLOCKED n=1 seq=1 action=BACK delta=0,64 tile=943->975 collision=ENTITY blocker=65535 type=0 frame=ba3e5182 exact=yes heap=38928->38928 largest=29684->29684 turnAdvance=no tileDispatch=no
```

The same cleaned renderer was then exercised repeatedly by successful native TURN renders. Hardware observed:

```text
N -> W -> S -> E -> N
final frame=ba3e5182
final viewport=9206eb24
final HUD=6c2aa46f
canonicalRoundTrip=exact
```

Representative successful render contract after cleanup:

```text
tempHud=0 B
routeNoPresent=1
finalPresent=1
legacyStable=yes
residentStable=yes
turnAdvance=no
tileDispatch=no
facingRefresh=deferred
stackHighWater=860
```

The final North turn reported:

```text
world=180664 us
sprite=9595 us
hud=1321 us
present=35034 us
total=236218 us
heap=38928->38928
largest=29684->29684
```

Long-run heartbeat remained stable:

```text
heap=104596
heap8=38928
largest8=29684
SD=ready
VIDEO=ready
CORE=ready
```

Most importantly for the review cleanup, the retest contains **no `[DOORVIEW]` output at all** and no `[TURNFRAME] ... fail=` witness, Guru Meditation, or reboot. The user reports the build works correctly on the physical CYD.

The supplied retest excerpt does not contain a successful CLEAR MOVE render; it contains the intentional line-35 blocked MOVE plus four successful TURN renders through the same cleaned `EspNativeGameplayFrame_renderTurn()` path. Do not invent a missing CLEAR-MOVE fingerprint.

## Corrected movement canon

Fresh Junction immediate movement truth is permanently superseded to:

```text
FORWARD      943->911 : CLEAR
BACK         943->975 : ENTITY / closed line 35 / type 0
STRAFE_LEFT  943->942 : WALL
STRAFE_RIGHT 943->944 : WALL
```

The old `BACK=CLEAR` record remains historical evidence of the omission that this milestone fixes.

## Invariants preserved

```text
shapeData == NULL
mediaTexels == NULL
/DoomRPG-ESP32.pak remains native backing store
runtime ZIP access remains forbidden
legacy Game.entities = 0
legacy Game.monsters = 0
no pointer-heavy legacy entityDb is constructed
EspNativeGameplayCollisionResult = 16 B
blocked move mutates no player/view state
blocked move mutates no framebuffer
Game_advanceTurn = no
Game_executeTile = no
facing refresh = deferred
opened-line relinking = fail-closed
```

## Superseded diagnostic branch

The older unmerged branch:

```text
agent/esp32-native-door-view-witness
tip = e04195e60a0499a4da3dc189eef98446d074fd92
base = 2aae0676528ab00c3494d142d8b35c22b7685dce
```

changes only:

```text
ESP32/platformio.ini                  +6 lines
ESP32/src/native_door_view_witness.c  +230 lines
```

It contains only the obsolete wrapper-based renderer witness and no permanent collision correction. The later direct witness has also been removed from production after finding the root cause.

Disposition: **safe to abandon/delete; no cherry-pick required**.

## Merge boundary

Final hardware-tested firmware-bearing SHA:

```text
5c01d91f9c6320460b2ecaf033f68a88bde80dfd
```

Status:

```text
REAL-CYD HARDWARE PASS
MERGE-READY after docs-only closeout audit
```

All commits after `5c01d91...` must be documentation-only for this hardware certificate to remain valid. After merge, recover the exact new `main` SHA before creating the next `agent/*` branch.
