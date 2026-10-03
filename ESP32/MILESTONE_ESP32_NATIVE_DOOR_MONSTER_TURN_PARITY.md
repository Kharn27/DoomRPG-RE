# ESP32 native door / monster-turn parity milestone

Date: 2026-10-03

Branch: `agent/esp32-monster-drop-checkpoint-v11`

Hardware-tested code boundary:
`69ee31a17f2e4900c825cc73ba1fee9400fc8ac5`

## Scope

This milestone closes the remaining native door/monster interaction differences
observed on the real classic CYD. It deliberately does not replace the native
door, movement or monster owners with desktop `Entity_t` state.

The relevant legacy behaviors are:

1. `Game_performDoorEvent()` changes the line state without checking whether a
   monster occupies the line.
2. `Entity_checkLineOfSight()` has a special source-side exception: when the
   trace contains exactly one type-0 line and the source lies on one of the
   line's endpoint axes, the monster may leave that source cell.
3. A successful SELECT tile event returns to `DoomCanvas`, and unless the
   script set `skipAdvanceTurn` (dialog/password families),
   `Game_advanceTurn()` runs. Opening a door therefore consumes the player
   turn and monsters revealed by it may act immediately.

The native implementation keeps those semantics split across compact owners.

## Closed-door source escape

Commit `73131d9afe3b2adc021a2913732ec8ca07ebba11` restores only the
legacy source-side exception. Normal destination closed-line collision remains
unchanged.

The source scan accepts the escape only when exactly one matching closed line is
present and the source X/Y matches one endpoint axis. The diagnostic boundary is:

```text
[MONSTERMOVE] TRACE-PASS side=source kind=line ... reason=legacy-type0-source-axis
```

A real-CYD run confirmed the monster no longer remains permanently trapped when
a door closes on its source tile.

## Logical close now, visual close after monster turn

The source escape fixed gameplay but initially looked wrong: the regular door
closed visibly first, other monsters acted, then the monster caught by the door
moved.

Commits `b56ee0c2f32dcb3029cb1a4f036f8fb990b866ee` and
`d886d6a7eaaeefec34d068648b0451a6ad827bcf` separate semantic and
presentation timing:

- MOVE door close immediately commits `EspMapLineState=open=0`;
- collision/path planning therefore sees the door as closed for the whole
  monster turn;
- `EspNativeDoorAnimator` holds a regular closing line at its fully-open visual
  displacement;
- after the ordered monster sequence (and any final attack resolution), the hold
  is released and the existing bounded four-frame close animation runs.

The accepted hardware transcript includes:

```text
[DOORANIM] HOLD line=234 logical=closed visual=open position=3 release=after-monster-turn collision=closed-now
[DOORANIM] HELD-FRAME lines=1 logical=closed visual=open ...
[MONSTERTURN] ORDERED-DISPATCH reason=MOVE turnToken=28 activeCount=0 ...
[DOORANIM] RELEASE deferredClose=1 phase=after-monster-turn logical=closed animation=resume-from-open
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
[DOORANIM] POST-MONSTER-COMPLETE ... logical=closed visual=closed
```

No gameplay rollback is tied to this post-turn presentation. Once the MOVE
transaction rendered successfully, world state is committed; later animation is
presentation-only.

## SELECT door advances the monster turn

Legacy verification showed a successful line event does not set
`skipAdvanceTurn`. The original SELECT handler therefore calls
`Game_advanceTurn()`.

Commit `5aa58ab902b67afbabbf1ce0ddd47be52a2b413b` adds a dedicated
native reason `SELECT_DOOR` rather than disguising the action as PASS_TURN or
PLAYER_ATTACK. It is requested only after the door batch has rendered
successfully and rollback is closed.

Transition/CHANGEMAP doors are excluded because their continuation has a separate
owner.

The first real-CYD attempt reached the new request but crashed in the following
success log because the newly added `turnAdvance=%s` format specifier lacked
its corresponding argument. The panic was a diagnostic-only varargs bug:

```text
[MONSTERTURN] DOOR-REQUEST seq=6 ...
[RESIDENTGAMEPLAY] SELECT ... secret
Guru Meditation Error: Core 1 panic'ed (LoadProhibited)
EXCVADDR: 0x00000000
```

Commit `69ee31a17f2e4900c825cc73ba1fee9400fc8ac5` fixes only that format
argument. CI and the real CYD then pass.

## Real-CYD acceptance

### Ordinary door, no active monsters

```text
[DOORANIM] ARM line=234 open=0->1 ...
[ACTION] SELECT seq=83 status=DOOR_OK ...
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
[MONSTERTURN] DOOR-REQUEST seq=83 source=select-door legacyAdvance=yes activation=post-door-render rollback=closed
[RESIDENTGAMEPLAY] SELECT ... turnAdvance=SELECT_DOOR-requested
[MONSTERTURN] ORDERED-DISPATCH reason=SELECT_DOOR turnToken=25 activeCount=0 ...
```

This proves opening still advances exactly one semantic monster turn even when
there is nobody active to act.

### Door reveal, immediate attack, ordered second monster

Opening event 71 mutates two non-regular secret-door lines. The render makes two
enemies visible and activates them before the new turn is dispatched:

```text
[ACTION] DOOR-BATCH event=71 count=2 status=OK ...
[MONSTERACT] ACTIVE sprite=220 subtype=0 tile=658 ... activeCount=1 activationOrder=0 ...
[MONSTERACT] ACTIVE sprite=264 subtype=3 tile=694 ... activeCount=2 activationOrder=1 ...
[MONSTERTURN] DOOR-REQUEST seq=92 ...
[MONSTERTURN] ORDERED-DISPATCH reason=SELECT_DOOR turnToken=31 activeCount=2 ...
```

The first activated monster immediately attacks:

```text
[MONSTERTURN] MEMBER-ATTACK-PROBE reason=SELECT_DOOR sprite=220 ... totalDamage=3 armorDamage=2 ...
[MONSTERACTIVESEQ] MEMBER ... ordinal=1/2 ... decision=immediate-attack ...
[MONSTERACTIVESEQ] PAUSE ... probe=1 reason=attack-in-flight ...
[MONSTERRETAL] COMMIT probe=1 ... playerHP=30->27 armor=8->6 ...
```

After retaliation resolves, the same turn resumes at ordinal 2. The second
monster moves and the sequence closes normally:

```text
[MONSTERACTIVESEQ] RESUME turn=31 nextOrdinal=2/2 resolvedProbe=1 ...
[MONSTERMOVELIVE] COMMIT ... sprite=264 tile=694->693 ... randomCommitted=yes ...
[MONSTERPOSTMOVE] COMPLETE reason=SELECT_DOOR ...
[MONSTERACTIVESEQ] COMPLETE turn=31 reason=5 activeCount=2 delivered=2 sameMonsterTurn=yes ordered=yes ...
```

This is the required parity witness: the opening SELECT itself consumes the
player turn, enemies revealed by that opening participate immediately, attacks
remain serialized through the existing visual/retaliation owner, and later
members still receive their place in the same ordered turn.

## Architecture and invariants

The milestone preserves the native architecture:

- immutable BSP geometry remains the source map; mutable open/locked bits stay in
  `EspMapLineState`;
- door visual timing stays in the fixed-size `EspNativeDoorAnimator`;
- monster movement/collision reads the logical line state, not the held visual;
- no legacy `Entity_t` ownership is introduced;
- no runtime ZIP access or map-wide decompression is introduced;
- gameplay RNG ordering remains in the existing monster sequencer;
- `shapeData == NULL`;
- `mediaTexels == NULL`.

Static RAM at the final code boundary is unchanged from the prior door timing
build: 45496 B.

## Build

GitHub Actions normal `esp32-cyd` CI #1556: SUCCESS.

```text
RAM:   13.9% (used 45496 bytes from 327680 bytes)
Flash: 59.9% (used 784621 bytes from 1310720 bytes)
artifact id: 11266949372
artifact sha256: 59d5e1ea7848495f6a936e8c03364f545b8129b116be696160d48fe48302c85c
```

No local PlatformIO build is claimed.

## Closure

Hardware-tested code boundary:
`69ee31a17f2e4900c825cc73ba1fee9400fc8ac5`.

Post-test closure is documentation-only. The runtime code needs no further
change for this door/monster-turn parity milestone.
