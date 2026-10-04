# ESP32 native extinguisher turn parity milestone

Date: 2026-10-03

Branch:
`agent/esp32-player-action-turn-parity`

Base main:
`fe739c97eb30f115cdfb85f37b972b1d17eb9b29`

Hardware-tested code boundary:
`a36edddd5b74b1737991f8fcfecb0ac43da727b8`

## Recovered legacy behavior

The original player utility-attack path is still ordinary player combat.

`Player_fireWeapon()` consumes ammo and calls
`Combat_performAttack(NULL, target)`. That enters `ST_COMBAT`. When the
player combat completes, `DoomCanvas_combatState()` performs:

```text
if (combat->curAttacker == NULL) {
    Game_advanceTurn(...);
}
```

This applies to successful extinguisher attacks against fire as well as other
player-owned combat interactions.

The pre-milestone native fire path instead logged:

```text
[ACTIONENGINE] FIRE-COMMIT ... turnAdvance=deferred
```

and had no explicit native monster-turn publication tied to the successful
fire-clear transaction.

## Native correction

Commit `61e42446fc2eb2dfc2a2dc44f204e56325ce3132` first wired both
fire-clear and jammed-door utility attacks to the existing
`EspNativeGameplayMonsterTurn_requestPlayerAttack()` producer.

Real-CYD experience then established that jammed/destructible doors were already
functionally correct: destroying one with the axe already allowed the monster
behind it to act afterward, unlike the separately-fixed ordinary SELECT-door
gap. To avoid creating a duplicate turn through a path whose existing behavior
was already hardware-good, commit
`a36edddd5b74b1737991f8fcfecb0ac43da727b8` narrowed the permanent
change to `ACTION_ROUTE_FIRE_CLEARED` only.

The final fire path now:

1. captures PlayerState and consumes extinguisher ammo;
2. stages/removes the fire through the existing bounded overlay;
3. requests the existing `PLAYER_ATTACK` turn producer;
4. renders the transactional world frame;
5. commits with
   `turnAdvance=PLAYER_ATTACK-requested`.

If that world render fails, the code first cancels the pending player-attack turn,
then restores the fire-removal overlay and PlayerState/ammo before rendering the
rollback frame. The action therefore cannot leave a scheduled monster turn
behind after a failed world transaction.

## Scope deliberately unchanged

The following behavior is not modified by the final code boundary:

- jammed/destructible-door turn behavior;
- ordinary SELECT-door handling;
- barrel/crate player-attack turn publication;
- monster activation/order/RNG;
- fire XP and sound deferrals;
- weapon animation cadence;
- save format;
- line-state ownership.

No new allocation or owner is introduced.

## Hardware acceptance

The user continued normal real-CYD level traversal and combat on the final build
and reported it as tested OK.

A later real-CYD run on the merged code provides a direct producer/dispatch
witness for two separate fire clears:

```text
[MONSTERTURN] ATTACK-REQUEST seq=64 source=explicit-native-player-attack rollback=available-until-cancel
[ACTIONENGINE] FIRE-COMMIT seq=64 ... turnAdvance=PLAYER_ATTACK-requested rollback=closed
[MONSTERTURN] ORDERED-DISPATCH reason=PLAYER_ATTACK turnToken=19 activeCount=0 ...
[MONSTERACTIVESEQ] BEGIN turn=19 reason=3 activeCount=0 ...

[MONSTERTURN] ATTACK-REQUEST seq=68 source=explicit-native-player-attack rollback=available-until-cancel
[ACTIONENGINE] FIRE-COMMIT seq=68 ... turnAdvance=PLAYER_ATTACK-requested rollback=closed
[MONSTERTURN] ORDERED-DISPATCH reason=PLAYER_ATTACK turnToken=23 activeCount=0 ...
[MONSTERACTIVESEQ] BEGIN turn=23 reason=3 activeCount=0 ...
```

This directly proves that a successful native extinguisher action publishes and
dispatches the semantic `PLAYER_ATTACK` turn. Both observed fires had
`activeCount=0`; the milestone therefore still does not claim a captured
scenario where an already-active enemy visibly attacks immediately after the
fire clear.

## CI

Normal `esp32-cyd` CI #1563: SUCCESS.

```text
RAM:   13.9% (45496 / 327680 B)
Flash: 59.9% (784877 / 1310720 B)
artifact id: 11268746232
artifact sha256: a5ba9d656fcd0c468a7c4b489abdc4f6314b900301d41281b45f379fc3d53ed2
```

Static RAM is unchanged from the merged door-turn boundary.

No local PlatformIO build is claimed.

## Architecture invariants

The milestone preserves:

```text
shapeData == NULL
mediaTexels == NULL
runtime assets = DoomRPG-ESP32.pak
no runtime ZIP fallback
no legacy Entity_t ownership added
no map-wide decompression added
```

## Closure

Hardware-tested code boundary:
`a36edddd5b74b1737991f8fcfecb0ac43da727b8`.

Post-test closure is documentation-only.
