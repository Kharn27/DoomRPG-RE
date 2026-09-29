# Milestone — Junction -> Sector 1 native progression recovery

Date: 2026-09-29

## Boundary

This branch began as a structural consolidation experiment, but the attempted
translation-unit merge was reverted exactly after a real-CYD regression was
shown to reproduce on the restored tree. The lasting result of the branch is
therefore not a large source consolidation. It is a set of permanent generic
runtime corrections uncovered while extending progression beyond Entrance.

Authoritative Git boundary before this documentation-only tail:

```text
main = fbacb595170e85b736bcb4e8c97cc22390eba1d8
branch = agent/esp32-consolidation-v1
hardware-tested code head = 7d64839a31376c4ca0a3ec4f0f5ae10395b04635
esp32-cyd CI #1009 = SUCCESS
static RAM = 45784 B
flash = 816517 B
artifact id = 11037553080
```

## Real-CYD progression proved on this branch

### Junction zero-enemy semantics

Junction has no enemies. Several owners had incorrectly treated an empty monster
set as "not ready".

The real CYD now proves:

```text
PASS_TURN
 -> request accepted
 -> monster turn scheduled
 -> COMPLETE candidates=0
 -> active sequence COMPLETE activeCount=0
```

V9 SAVE also serializes the empty monster set canonically:

```text
monsters=0
positions=0
activation=0
topology=3 destructibles
```

and V9 LOAD restores that state without manufacturing zero-length MonsterState
or MonsterPosition allocations. Checkpoint resume and GIB presentation adoption
also accept the zero-enemy world. The user explicitly confirmed SAVE and LOAD in
Junction on the real classic CYD.

### Direct Junction -> Sector 1 CHANGEMAP

The exact target recovered from the authoritative BSP is:

```text
map catalog id = 2
resource = /level01.bsp
name = Sector 1
gameplay loadMapId = 3
spawn tile = 477
spawn direction = 192
source CRC32 = f60dfb2b
runtime arena FNV = 82d2ee25
```

Junction event 62 is:

```text
EV_SAVEGAME
EV_CHANGEMAP
EV_CLOSELINE
EV_OPENLINE
```

with `showStats=0`.

The branch removed three accidental assumptions that Entrance -> Junction had
hidden:

1. EV_SAVEGAME's return/save route and EV_CHANGEMAP's immediate target are
   independent legacy strings.
2. A transition door may lead either to WAIT_STATS or directly to a READY
   handoff when `showStats=0`.
3. Fresh-map spawn is not angle-64-only. The native spawn pipeline now owns all
   four exact cardinal legacy bases 0/64/128/192.

The transition remains transactional and fail-closed. The destructive resident
map switch is serviced only after the source SELECT/door transaction returns.

The real CYD proves the full direct handoff through:

```text
MAPFLASH READY map=2
MAPRT READY arenaBytes=12476 arenaFNV=82d2ee25
NATIVECHANGEMAP COMMIT sourceMap=9 targetMap=2 gameplayLoadMapId=3
target spawn/session
first playable Sector 1 frame
```

### Spawn and movement event generalization

Sector 1 exposed two more milestone-shaped restrictions rather than
level-specific bugs.

The spawn tile executes EV_FORCEMESSAGE. The native first-tile and
finish-rotation dispatches now admit one bounded transactional FORCE_MESSAGE,
reuse the permanent status-message owner/rollback semantics, and clear the
spawn-time status at the same post-spawn point as legacy HUD cleanup.

Then leaving spawn tile 477 exposed event 43 with a mixed movement batch
containing EV_FORCEMESSAGE plus another already-owned command family. The
permanent mixed MOVE executor now admits one FORCE_MESSAGE inside the same
bounded atomic batch as:

```text
state
show
line lock/unlock/toggle
open/close line
```

Execution preserves legacy command order and exact reverse rollback. This is a
generic MOVEEVENT capability; there is no Sector-1/event-43 special case.

Real-CYD progression proof was first established at `934d0e2...` and is
reconfirmed after the review fixes on `7d64839...`:

```text
MOVE tile=477 -> 509, pos=1888,992, angle=192, committed=yes
TURN 192 -> 128, legacyAdvance=no
MOVE tile=509 -> 508, pos=1824,992, angle=128, committed=yes
```

The same run proves live Level01 monster publication and ordered movement after
the player leaves spawn:

```text
MONSTERACT ACTIVE sprite=218
MONSTERACT ACTIVE sprite=237
MONSTERMOVELIVE COMMIT sprite=218 tile=532->500
MONSTERMOVELIVE COMMIT sprite=237 tile=438->439
MONSTERACTIVESEQ COMPLETE activeCount=2 delivered=2
```

So Sector 1 is no longer only loadable: the real CYD entered the map, rendered
it, moved away from spawn and continued normal monster-session processing.

### Post-review SAVE route lifetime and facing-edge parity

Code review caught two generic edge cases after the first progression PASS.

First, EV_SAVEGAME and EV_CHANGEMAP may name different maps. The transition
owner already accepted that shape, but its inline SAVEGAME route was cleared by
the session/input reset during successful handoff. The route is now copied into
the SAVE subsystem before that destructive reset. The real classic CYD proves
the lifetime boundary: after Junction -> Sector 1 and one committed MOVE
477 -> 509, a V9 SAVE logged:

```text
[NATIVESAVE] SAVE ... map=2 gameplayLoadMapId=3 pos=1888,992 angle=128
             returnRoute=/junction.bsp/416,1824/192 ...
```

The same checkpoint then loaded successfully, reprime completed and resident
gameplay reached:

```text
[ENGINESESSION] READY map=2 angle=128 ... TURN+MOVE=armed
```

V9 itself is intentionally unchanged by this review fix. The live return-route
owner is not serialized in V9; a checkpoint LOAD clears it rather than leaking
a route from the replaced session. Persisting that legacy return route in the
on-disk checkpoint, if required, is a separate save-format milestone.

Second, generalized four-cardinal fresh-map facing can project its three-tile
trace beyond a 32x32 map edge. Legacy `Game_trace()` clamps source and
destination tile coordinates independently to `[0,31]`. The native facing
trace now does the same while retaining the raw world endpoints for wall-sprite
plane crossing. This review correction is CI/build-valid on the tested code
head, but the supplied hardware path starts at Sector 1 tile 477 and does not
exercise a spawn within three trace steps of the map boundary. That exact
map-edge case remains an explicit non-claim.

## Important non-claims

This milestone does not claim that every Doom RPG map, CHANGEMAP script shape or
opcode family is now generic. Unsupported opcode families remain fail-closed.

It also does not claim that the original consolidation objective is complete.
The attempted early translation-unit merge was reverted; the branch mainly
removed runtime assumptions discovered by progression testing.

## Permanent architectural invariants

No fix in this milestone reintroduces map-wide legacy shape/texel ownership or
runtime ZIP dependence. Continue to preserve:

```text
shapeData == NULL
mediaTexels == NULL
DoomRPG-ESP32.pak = native runtime backing store
compact immutable map runtime
small explicit mutable owners
bounded static journals / bounded scratch
```

## Next direction after merge

After the user merges this branch:

1. read the exact new GitHub `main` SHA;
2. re-read PORTING_STATUS.md, DOCUMENTATION.md and this milestone;
3. create the next `agent/*` from that exact main;
4. resume structural consolidation of ESP32 sources/wrappers without weakening
   the newly hardware-proven Junction/Sector-1 progression;
5. keep legacy desktop/J2ME source as behavioral specification while reducing
   permanent ESP32 dependence on legacy implementation units.

Do not create the next branch before the merge is announced.
