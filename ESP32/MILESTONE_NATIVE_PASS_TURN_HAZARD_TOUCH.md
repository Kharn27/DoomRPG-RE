# Native PASS_TURN current-tile hazard touch — hardware PASS

This milestone closes the bounded legacy `PASS_TURN` current-tile hazard slice on
the classic ESP32-2432S028R CYD without PSRAM.

The repository and real-CYD Serial log are authoritative. The desktop/J2ME code
is used only as the executable behavior specification.

## Locked Git boundary

```text
main at branch creation = 63cc8897e98c1ac6bf597234b35fde87cc2f7570
branch = agent/esp32-native-pass-turn-hazard-touch
hardware-tested code boundary = e76183adf8245b465d7c398abba88a6c6a86f627
status = REAL-CYD PASS_TURN CURRENT-TILE HAZARD + REENTRANT VIEWFLASH PASS
branch policy = LOCKED; commits after tested SHA are documentation-only
```

Normal GitHub Actions `esp32-cyd` run `33969446333` / run #132 completed
successfully on the exact tested SHA. Job `101315237040` (`PlatformIO esp32-cyd`)
built the normal classic-CYD firmware and uploaded the artifacts successfully.

CI is compile/link evidence only. The hardware result below is the runtime truth.

## Recovered legacy ordering

The bounded production ordering is:

```text
Hud_addMessage("Turn passed.")
Game_touchTile(currentTile, false)
Game_advanceTurn()
```

For `Game_touchTile(..., false)`, resource pickup processing is not performed.
The native PASS_TURN route therefore touches only linked type-10/type-11 hazards
on the settled current tile, then requests the existing MonsterTurn owner.

Because the current native top bar owns one message slot, an actual hazard damage
message intentionally supersedes the transient `Turn passed.` visual. Gameplay
ordering is preserved: the hazard PlayerState mutation exists before MonsterTurn
is requested.

If the subsequent MonsterTurn request cannot be armed, the hazard mutation and
queued feedback are rolled back exactly.

## Permanent native ownership

No new map-wide or heap gameplay owner was introduced.

```text
EspNativeGameplayPassTurn_execute()
 -> EspNativeGameplayHazardTouch_processPassTurn()
 -> shared EspNativeGameplayPlayerState mutation
 -> one bounded ACTION_FEEDBACK_DAMAGE intent
 -> existing action-feedback / VIEWFLASH presenter
 -> existing MonsterTurn request
```

`EspNativeGameplayHazardPassTurnUndo` is a small transactional undo record owned
by the caller for the PASS_TURN operation only.

The current hazard subset remains bounded:

```text
type 10 -> raw health component 1, armor component 2
type 11 -> raw health component 10, armor component 10
linked current-tile hazards only
resources ignored for touched=false
player lethal transition -> fail closed
familiar redirection -> fail closed
secondary burn text / pain face / shake / sound -> deferred
```

Movement-side `Game_touchTile(..., true)` remains separately owned by the earlier
hazard-touch milestone; mixed movement tiles still fail closed until a complete
native TileTouch orchestrator owns their ordering.

## Feedback presentation ordering

The PASS_TURN hazard route must present its newly queued damage feedback before
normal feedback-expiry service is allowed to retire an older message.

The hardware-visible path is therefore:

```text
[HAZARDPASS] COMMIT ... feedback-owner-pending ... rollback=armed
[ACTIONFEEDBACK] PAINT kind=6 text="3 damage!" ...
[VIEWFLASH] PAINT color565=b800 ... durationMs=500 ...
[PASSTURN] REQUEST ... feedbackPresent=immediate
[MONSTERTURN] SCHEDULE ... reason=PASS_TURN ...
```

This prevents a still-visible previous damage message from causing the new
PASS_TURN damage feedback to be queued but never physically shown.

## Reentrant VIEWFLASH bug found by hardware testing

Real-CYD testing exposed a presentation-owner bug when a second red flash arrived
before the first 500 ms lease expired.

Old behavior:

```text
first flash -> snapshot clean border -> paint red
second flash before expiry -> snapshot current border again
current border is already red -> restore snapshot becomes red
expiry -> "restores" red indefinitely until a later full redraw
```

This was a generic VIEWFLASH snapshot corruption, not desirable "standing on
fire" state.

The permanent fix gives the border visitor three behaviors:

```text
snapshot + paint
restore snapshot
repaint while preserving existing snapshot
```

If another flash arrives while a valid flash is still visible and no full-frame
redraw replaced the framebuffer, the original pre-flash snapshot is retained and
only the color/lease are refreshed. A real fresh framebuffer causes a fresh
snapshot to be taken.

Diagnostic witness:

```text
[VIEWFLASH] REFRESH color565=b800 snapshot=preserved framebufferFresh=0
```

## Real-CYD positive witness

The user explicitly reported the final behavior as "Impeccable" on the exact
hardware-tested SHA.

A representative current-tile type-10 PASS_TURN sequence was:

```text
[HAZARDPASS] COMMIT tile=619 sprite=139 type=10 hazards=1
    rawDamage=1+2 hp=21->18 armor=0->0
    message="3 damage!"
    passMessage="Turn passed."-legacy-superseded
    flash=red-bb0000/500ms
    lethal=fail-closed rollback=armed

[ACTIONFEEDBACK] PAINT kind=6 text="3 damage!" ... durationMs=1200
[VIEWFLASH] REFRESH color565=b800 snapshot=preserved framebufferFresh=0
[VIEWFLASH] PAINT color565=b800 viewport=0,20,160,80 thickness=2
    pixels=944 durationMs=500 snapshot=bounded present=caller feedback=6
[PASSTURN] REQUEST seq=76 tile=619 ... tileTouch=hazard-committed
    type10/11=owned ... playerMutation=hazard-owned feedbackPresent=immediate
[MONSTERTURN] SCHEDULE ... reason=PASS_TURN ...
[MONSTERTURN] COMPLETE ... candidates=0 ... mutation=no
[VIEWFLASH] EXPIRE elapsedMs=504 targetMs=500 color565=b800
    restored=viewport-border-only
```

The important positive condition is not merely the presence of `EXPIRE`; the
hardware user confirmed that the red border physically disappeared after the
last lease even when the player remained on the hazard tile.

Subsequent normal movement, door animation, SELECT, rotation and strafe remained
operational after the test.

## RAM witness

The supplied real-CYD session remained stable throughout repeated hazard touches,
rapid PASS_TURN refreshes, expiry, door animation and later movement:

```text
heap = 86524
heap8 = 20792
largest8 = 18420
shapeData = NULL
mediaTexels = NULL
```

No PSRAM is used. Audio remains deferred.

## What this milestone owns

```text
PASS_TURN current-tile linked type10/type11 touch
legacy touched=false resource exclusion
hazard PlayerState mutation before MonsterTurn request
exact rollback if the turn request cannot arm
hazard damage message superseding one-slot "Turn passed." display
immediate physical presentation of the new damage feedback
500 ms red viewport-border flash
safe overlapping/reentrant viewport-flash lease refresh
exact clean-border expiry after the final flash lease
```

## Still intentionally deferred

```text
player lethal/death transition
familiar hazard redirection
secondary "It burns!" / "It really burns!!" presentation
pain face / shake / sound
complete mixed movement-tile resource/hazard orchestration
multiple-message UI queue beyond the current one-slot top bar
production SAVEGAME / CHANGEMAP ownership
subtype 4/13 three-goal same-turn monster movement chain
three-shot / multi-loop monster attack presentation
monster projectiles
multiple-live-monster ordering
```

This milestone does not broaden any of those families.

## Merge rule

`e76183adf8245b465d7c398abba88a6c6a86f627` is the locked code boundary tested on
the real CYD. Every commit after it on this branch must remain documentation-only.
After merge, recover the exact new `main` SHA before starting another `agent/*`
branch.
