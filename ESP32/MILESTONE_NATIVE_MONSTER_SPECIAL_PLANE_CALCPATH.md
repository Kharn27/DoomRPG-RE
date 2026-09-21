# Native monster calcPath special-plane milestone

Status: **CI CANDIDATE / REAL-CYD HARDWARE TEST PENDING**.

Code boundary under test:

```text
branch = agent/esp32-native-gameplay-changemap-transition
code boundary = 4e5244f89d635f851576f304b6d8ce8ebbc7e2c0
```

## Hardware regression witness

A fresh-from-New-Game Entrance run reached the late room with ordinary subtype-1
dogs active in the ordered monster sequence. Some dogs moved normally, while
sprites 205 and 159 repeatedly failed closed before consuming RNG:

```text
[MONSTERMOVE] DEFER ... sprite=205 cause=calcPath-special-plane-corpus-needed mask=ff87 rngCalls=0 mutation=no
[MONSTERMOVE] DEFER ... sprite=159 cause=calcPath-special-plane-corpus-needed mask=ff87 rngCalls=0 mutation=no
```

This is not a save/load restoration issue: the witness was reproduced after a
fresh New Game without loading a checkpoint.

## Legacy behavior recovered

Legacy `Entity_calcPath()` evaluates each one-tile look-ahead through
`Entity_checkLineOfSight()`, which delegates to `Game_trace()`.

For trace entity types 14 and 15, `Game_trace()` does not fail closed merely
because such an entity is present. When sprite-info bit `0x20000` is set, it
counts the entity only when the segment crosses the encoded plane:

```text
Y plane mask = 0x180000
X plane mask = 0x600000
```

The native movement tracer already owns the same bounded rule in
`specialEntityBlocks()` and already uses it for ordinary cardinal movement and
attack LOS. The remaining calcPath call alone still selected the earlier
`strictSpecial=1` milestone guard and therefore returned `-2` before applying the
owned rule.

## Candidate change

The calcPath one-tile look-ahead now selects the same exact special-plane trace
semantics already used by the rest of native movement (`strictSpecial=0`). No
new owner, allocation, RNG consumption, renderer mutation or topology mutation
is introduced. The obsolete `-2` fail-closed path remains in the implementation
as a guard, but the normal calcPath route no longer requests it.

The READY witness changes to:

```text
aiGoal=legacy-cardinal calcPath=2-step+special-plane
```

## Required real-CYD proof

Return to the same late Entrance room and advance turns near the previously
stalled dog. The old line:

```text
cause=calcPath-special-plane-corpus-needed
```

must disappear. The same ordinary dog should either produce the normal
`MONSTERMOVE PROBE` -> `MONSTERMOVELIVE COMMIT` chain when a legal legacy move
exists, or remain blocked for an explicit ordinary collision/no-move reason.

Do not promote this milestone to hardware PASS until that exact real-CYD witness
is observed.
