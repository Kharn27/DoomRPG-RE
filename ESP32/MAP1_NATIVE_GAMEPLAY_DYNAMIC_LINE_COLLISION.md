# MAP1 native gameplay dynamic line collision

Status: **REAL-CYD HARDWARE PASS**

```text
branch = agent/esp32-native-gameplay-dynamic-line-collision
base main = 3b17a400c35338e434fab16ae0c2a3a63ab47e3e
hardware-tested implementation SHA = 52ddbf979e33f99be27c8344eb4e0572ac4d0547
```

## Goal

The preceding closed-line milestone recovered the missing line-derived collision entity used by the original game, but deliberately failed closed whenever any native line was open.

This milestone removes that global dynamic-line fail-close and makes native cardinal collision consume the permanent compact `EspMapLineState` overlay per line:

```text
closed line -> participates in collision
open line   -> skipped by collision
close again -> participates again
```

No legacy `Entity_t`, `entityDb`, runtime ZIP, map-wide mutable line copy, sound, animation, renderer mutation, SELECT behavior, tile-event dispatch, turn advance, or monster/entity activation is introduced.

## Legacy behavior recovered

The desktop/J2ME reference implements door collision topology by toggling line flag `0x40` and relinking/unlinking the corresponding line-derived entity:

```text
open  -> Game_unlinkEntity(line entity)
close -> Game_linkEntity(line entity, link tile)
```

`Game_performDoorEvent()` also rejects any line carrying lock flag `0x400` before changing the open state. The dynamic collision consumer therefore models only the already-decided open/closed topology; it does **not** bypass lock/key semantics.

## Permanent implementation

Files changed from merged `main`:

```text
ESP32/include/esp_native_gameplay_collision.h
ESP32/src/esp_native_gameplay_collision.c
ESP32/src/native_junction_move_collision_probe.c
```

Core collision behavior:

- preserve static WALL collision ordering;
- preserve reverse line-entity scan ordering and legacy geometry/link nudges;
- preserve trace mask `0xf287` semantics;
- read `EspMapLineState_getOpen(lineIndex)` during line scan;
- skip only the specific open line;
- keep closed lines as line-derived blockers;
- keep compact sprite-entity collision unchanged;
- keep `EspNativeGameplayCollisionResult` at 16 B;
- allocate nothing in the collision hot path.

The previous global rule:

```text
lineState.openCount != 0 -> DYNAMIC_LINES_UNSUPPORTED
```

is no longer used merely because some line is open.

## Activation witness

Canonical Junction arrival-door line `35` is used as a reversible collision-consumer witness at boot:

```text
source tile = 943
reserved destination tile = 975
line = 35
texture = 7
entity type = 0
initial line FNV = 3658710d
```

The probe requires:

```text
CLOSED -> BLOCKED_ENTITY by line 35
setOpen(35, 1)
OPEN   -> CLEAR for the same cardinal trace
setOpen(35, 0)
CLOSED -> byte-identical collision result to the first trace
```

It also requires exact rollback of line-state FNV/counts plus frame, player view, legacy owners, resident snapshot, heap and pack state.

### Probe correction discovered on first CYD run

The first implementation SHA `429a86d2d84cbbedc807046491d06db1e37b9474` contained a contradictory diagnostic guard: while the line was intentionally OPEN it required both the line-state FNV to change and the entire `EspMapResidentSnapshot` to remain byte-identical, even though that snapshot contains `lineStateFNV1a`.

Real hardware correctly exposed the contradiction:

```text
[LINECOLLISION] BLOCK source=943 dest=975 line=35 texture=7 flags=00000505 type=0 defTile=312
[DYNLINECOLLISION] CLOSED line=35 tile=943->975 status=ENTITY blocker=65535 type=0 openLines=0 stateFNV=3658710d
[DYNLINECOLLISION] FAILED reason=open side effect ... stateFNV=3658710d baselineFNV=3658710d ...
[MOVEPROBE] FAILED dynamic line collision witness ...
[NATIVEBOOT] BLOCKED predecessor probe failure ...
```

The fail path restored line 35 exactly, but `NATIVEBOOT` correctly prevented gameplay activation, which made touch feedback visible while MOVE/TURN execution stayed disabled.

Fix SHA `52ddbf979e33f99be27c8344eb4e0572ac4d0547` changes only the witness: while OPEN, the resident snapshot must remain identical **except** for its intentional `lineStateFNV1a`; after CLOSE, the full resident snapshot must again be byte-identical.

## Real-CYD result

After flashing the fixed branch, the user physically moved through Junction and exercised both MOVE and TURN repeatedly without the previous boot block.

Representative supplied logs:

```text
[MOVEPROBE] RESTORED frame=bfb65ae4 exact=yes queued=yes lifecycleReturnBeforeExecute=yes
[MOVE] OK n=12 seq=17 action=FORWARD ... tile=591->559 ...
[MOVE] RENDER ... heap=38928->38928 largest=29684->29684 ... legacyStable=yes residentStable=yes orientationStable=yes

[TURN] OK n=5 seq=20 action=TURN_RIGHT ... pos=992,1120 ...
[TURN] RENDER ... heap=38928->38928 largest=29684->29684 ... legacyStable=yes residentStable=yes

[MOVE] OK n=24 seq=41 action=FORWARD ... tile=746->778 ...
[MOVE] RENDER ... heap=38928->38928 largest=29684->29684 ... legacyStable=yes residentStable=yes orientationStable=yes
```

The supplied post-fix excerpt starts after activation and therefore does not include the exact `DYNLINECOLLISION OPEN` FNV. Do not invent that value. However, the normal move service can only become active after `verifyDynamicLineCollision()` succeeds; the repeated real MOVE/TURN execution therefore proves the corrected CLOSED -> OPEN -> CLOSE witness completed on the physical CYD.

Long-run values visible in the supplied run:

```text
heap8 = 38928 stable
largest8 = 29684 stable
move/turn stackHighWater = 860
shapeData = NULL (inherited invariant)
mediaTexels = NULL (inherited invariant)
legacyStable = yes
residentStable = yes
turnAdvance = no
tileDispatch = no
```

## What this milestone does NOT make playable

There is intentionally no player-operated door yet.

Current touch SELECT still logs:

```text
action=SELECT
consumedBy=probe
dispatch=observer
gameplay=no
```

The line-35 witness opens and closes invisibly during activation and restores the original closed state before gameplay begins. Line 35 itself carries lock flag `0x400` in canonical Junction (`flags=0x505`), so it must not be used as a fake unlocked SELECT door.

The next gameplay boundary must recover native SELECT/front-tile event semantics and preserve lock/key behavior rather than bypassing it.

## Hardware certificate

```text
REAL-CYD HARDWARE PASS
hardware-tested implementation SHA = 52ddbf979e33f99be27c8344eb4e0572ac4d0547
merge-ready after docs-only closeout audit
```

All commits after the hardware-tested implementation SHA must remain documentation-only for this certificate to remain valid.
