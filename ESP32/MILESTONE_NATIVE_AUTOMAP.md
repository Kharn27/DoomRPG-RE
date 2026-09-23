# Milestone — Native Automap core

Status: **REAL-CYD PASS for the bounded Automap core**.

## Git / hardware boundary

The Automap behavior was exercised on the real classic CYD before the later
main-menu rebase.

```text
hardware-tested Automap code head = cdd8168588edc5a0d996f19d7cf6c2466ba4a51b
esp32-cyd CI #572 = SUCCESS
static RAM = 44832 B
flash = 738613 B
```

The branch was then rebuilt on top of the newer main that contains the native
main-menu `Load Game` work:

```text
main = bbe9b10c23dcb7a1d6b63db421073c0eeaf1872c
branch = agent/esp32-native-automap
rebased Automap candidate = 4463b12500defef53447fc8004dc11a1a8e8c793
esp32-cyd CI #573 = SUCCESS
static RAM = 44944 B
flash = 741093 B
```

The rebased candidate is zero commits behind main and contains the same Automap
delta plus the merged main-menu work. The Automap behavior itself has real-CYD
evidence; the combined rebased hash is CI-valid but is not independently
promoted to a new hardware PASS without a replay.

## Recovered legacy behavior

The native implementation follows the original Doom RPG/J2ME split rather than
inventing a new map model.

Legacy Automap behavior relevant to this milestone:

```text
ST_AUTOMAP
 -> same playing-event input path as normal gameplay
 -> movement may advance gameplay/monster turns
 -> rotation changes facing without advancing monster turns
 -> visible world geometry publishes Automap reveal state
 -> visited tiles fill the explored floor cells
 -> visible lines are drawn as thin map delimiters
 -> visible special sprites/planes gain Automap visibility
 -> GIVEMAP reveals the complete eligible map geometry
```

A visually surprising original behavior was explicitly checked against the
original game during hardware review: Automap line visibility can expose map
geometry behind a still-closed door, and human/special markers appear only once
the renderer has actually exposed their relevant visibility. This is legacy
parity, not treated as an ESP32 bug.

## Permanent native owners

The milestone keeps the compact native ownership model:

```text
EspMapAutomapState
 -> line reveal bitset
 -> sprite reveal bitset
 -> visited tile projection remains in EspMapState

EspNativeBspVisibility
 -> transient render-derived visibility
 -> publishes legacy reveal side effects
 -> no second framebuffer

EspNativeGameplayAutomap
 -> renders directly into shared 160x120 RGB565 framebuffer
 -> map area = 96x96 at (32,12), scale=3
 -> compact player-facing chevron
```

No `shapeData`, `mediaTexels`, legacy entity graph or map-wide texture owner
is introduced.

## Real-CYD PASS — open / close ownership

The first hardware run proved that the Automap takes and releases framebuffer
ownership cleanly:

```text
[RESIDENTGAMEPLAY] QUEUE ... action=AUTOMAP ... context=WORLD
[AUTOMAP] FRAME map=96x96@32,12 scale=3 ...
[RESIDENTGAMEPLAY] AUTOMAP-OPEN ... turnAdvance=no ...
[HUBACTIONGATE] PAUSE owner=automap ...
...
[GAMEPLAYHUD] REPAINT ...
[RESIDENTGAMEPLAY] FRAME reason=AUTOMAP-CLOSE ...
[RESIDENTGAMEPLAY] AUTOMAP-CLOSE ... mode=world turnAdvance=no
[HUBACTIONGATE] RESUME owner=world ...
```

The world framebuffer returned cleanly with no stale Automap pixels in the HUD.

## Real-CYD PASS — live exploration

Movement while the map is open is native and live. Successful MOVE commits the
same player/view mutation as normal gameplay, updates visited cells and schedules
the normal monster-turn path.

Example hardware progression:

```text
[AUTOMAP] FRAME ... visited=13 ... player=51,91 ...
[RESIDENTGAMEPLAY] MOVE ... tile=839->838 ... committed=yes
...
[AUTOMAP] FRAME ... visited=17 ... player=45,91 ...
[RESIDENTGAMEPLAY] MOVE ... tile=837->836 ... committed=yes
[MONSTERTURN] SCHEDULE ... reason=MOVE ...
```

A blocked MOVE while Automap owns input also follows the recovered legacy rule
that the attempted movement still advances the gameplay turn. The implementation
reuses the existing pending-turn owner rather than adding permanent BSS.

## Real-CYD PASS — pickups while Automap stays visible

The hardware run crossed Armor Shards with Automap active. Player resource state
committed normally while the framebuffer owner remained the Automap:

```text
[PLAYERRES] PREPARE ... action=armor value=4->8 ...
[AUTOMAP] FRAME ...
[PLAYERRES] AUTOMAP-FRAME ... ownership=retained
[PLAYERRES] COMMIT ... armor=8/20 ...
```

World feedback is gated while the Automap owns the framebuffer, preventing stale
top-bar/viewport restoration from overwriting the map.

## Real-CYD PASS — SELECT and regular doors

The first MOVE/TURN candidate intentionally left SELECT fail-closed. Hardware
then exposed the exact omission:

```text
[RESIDENTGAMEPLAY] AUTOMAP-IGNORE ... action=SELECT ...
```

Legacy recovery showed that `ST_AUTOMAP` uses the same playing-event handler,
so SELECT was enabled through the existing native action engine.

The user then validated regular door interaction while Automap remained active:

```text
[DOORANIM] ARM line=275 open=0->1 flags=00000205 frames=4 ...
[ACTION] SELECT ... status=DOOR_OK tile=837 event=86 ...
[DOORANIM] AUTOMAP-FRAME 1/4 ...
[DOORANIM] AUTOMAP-FRAME 2/4 ...
[DOORANIM] AUTOMAP-FRAME 3/4 ...
[DOORANIM] AUTOMAP-FRAME 4/4 ...
[DOORANIM] AUTOMAP-COMPLETE ... transaction=committed
[ACTION] DOOR-BATCH event=86 count=1 status=OK ...
[RESIDENTGAMEPLAY] SELECT ... committed=yes ...
```

The following MOVE crossed the now-open line successfully. The same hardware run
also exercised the automatic close-on-exit movement event entirely through the
Automap render sink.

## Real-CYD PASS — render-derived thin delimiters

The original renderer marks map geometry as seen when it becomes render-visible:

```text
normal line visibility -> legacy line flag 0x80
special sprite visibility -> legacy info bit 0x10000000
```

The native renderer now publishes the equivalent semantics into
`EspMapAutomapState`. While the Automap is already open, a transient
`EspNativeBspVisibility` pass performs the same BSP/backface/clip visibility
work without rasterizing or presenting the 3D framebuffer.

The user compared the resulting behavior with the original Doom RPG and
confirmed the apparently odd reveal behavior matches it. The Automap visual
result was accepted as correct.

## GIVEMAP boundary

Native `EV_GIVEMAP (9)` support is implemented as a bounded synchronous event
chain with a compact transactional Automap snapshot and exact rollback support.

This route is **code/CI candidate only** until an actual eligible GIVEMAP event
is observed executing on the real CYD. Do not promote it to hardware PASS from
the Automap presentation tests alone.

## Intentionally deferred Automap parity

This milestone is the bounded core, not a claim that every legacy action is
already live while the map is open.

Still deferred or separately owned:

```text
PASS_TURN while Automap active
weapon cycling / combat actions while Automap active
MENU/HUB transitions from Automap
hardware execution of EV_GIVEMAP
checkpoint persistence of Automap reveal state — closed by V7 (`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V7_AUTOMAP.md`)
advanced/enhanced Automap behavior beyond original parity
```

Dialog/password paths exit Automap before taking exclusive modal ownership.

## Result

**Native Automap core: REAL-CYD PASS.**

The production architecture now has a native, compact, live-updating Automap
with hardware-proven open/close ownership, movement, pickup retention, SELECT
door interaction and legacy-style reveal publication.

Automap reveal-state checkpoint persistence is now separately hardware-proven
by native SAVE V7; see
[`MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V7_AUTOMAP.md`](MILESTONE_NATIVE_GAMEPLAY_SAVE_LOAD_V7_AUTOMAP.md).

The next major gameplay frontier identified during later original-game parity
review is the permanent **facing-entity top-bar label**. Remaining Automap
action parity can still be completed as its own bounded follow-up when required.
