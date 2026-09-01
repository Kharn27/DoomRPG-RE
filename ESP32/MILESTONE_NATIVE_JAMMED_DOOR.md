# ESP32 native jammed-door destructible milestone

Hardware authority: classic ESP32-2432S028R CYD, normal `esp32-cyd` firmware.

## Git boundary

```text
main = 6e07187f60a27e197189a47f2cbc7ff4e338cfec
branch = agent/esp32-native-full-gameplay
hardware-tested code = feae39c768105b8851a77dab1afa4b52bec231dd
parent candidate = 19fce71bc1f55d9151eab867076c79ba488b20ff
status = REAL-CYD PASS
```

The parent candidate already executed the bounded jammed-door death route correctly,
but incorrectly reused the regular four-frame sliding-door animator. The tested
`feae39c...` correction keeps the transaction unchanged and bypasses only that
presentation wrapper for destructible subtype 3.

## Legacy behavior recovered

For legacy `Entity_died()` with `eType=12` and `eSubType=3` (jammed door / weak
wall), the game:

```text
shows "Door cleared!"
adds 1 XP
removes the destructible
```

Unlike barrel/crate/power-coupling cases, this subtype does not allocate a
`Game_gsprite` destruction animation. Therefore the native route must not arm the
regular sliding-door animator merely because the backing line carries the regular
door geometry flag.

## Hardware PASS witness

Real-CYD sequence:

```text
[ACTIONENGINE] TRACE ... tile=686 target=line line=201 type=12 subtype=3 route=JAMMED_DOOR_CLEARED
[DESTRUCTIBLE] ARM ... event=72 global=201 weapon=0 distance=1 runFlags=00000100 hitCalc=259
[DESTRUCTIBLE] HIT ... rand=0 calc=259 guaranteed=yes open=0->1 rngConsumed=1
[DYNAMICLINES] FRAME ... open=1 adaptedReads=2 animatedReads=0 ...
[DESTRUCTIBLE] COMMIT ... line=201 event=72 open=0->1 message="Door cleared!" xp=1-deferred ... rollback=closed
```

Critical negative witness:

```text
no [DOORANIM] ARM line=201
no [DOORANIM] FRAME for line=201
```

The player then traversed the cleared blocker successfully:

```text
MOVE tile=654->686 committed=yes
MOVE tile=686->718 committed=yes
```

The second move also executed the destination tile script transaction correctly:

```text
ENTER tile=718 status=SCRIPT_STATE_OK event=78 opcode=11
stateEvent=54 state=0->1 mutation=yes rollback=1
MOVEEVENT COMMIT ... rollbackLease=closed
```

This proves the destructible presentation fix did not regress collision, line
state, event routing, or post-clear movement.

## Current monster-combat frontier

Immediately behind the cleared blocker, the dog is resolved correctly by the
native action tracer:

```text
[ACTIONENGINE] TRACE ... tile=750 target=sprite index=179 type=1 subtype=1 route=ENEMY_COMBAT_DEFERRED
[ACTIONENGINE] BACKEND-DEFER ... family=monster-combat reason=native-monster-hp+attack-state-not-owned mutation=no
```

This is expected fail-closed behavior. No monster HP, attack state, death state,
ammo, XP, sound, or turn-advance mutation is currently permitted by this route.

The next bounded milestone should recover the exact legacy first-monster combat
semantics and introduce a small explicit native mutable owner rather than mutating
legacy `Game.entities` / `Game.monsters`. It must preserve the existing hard
invariants:

```text
shapeData == NULL
mediaTexels == NULL
legacy Game.entities == 0 for migrated world
legacy Game.monsters == 0 for migrated world
```

Do not fold monster combat into the already hardware-validated jammed-door code
boundary.
