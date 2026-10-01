# Native ordered monster turn + player death closure

Status: **REAL-CYD PASS — final review closure 2026-10-02**

Hardware-tested code boundary:
`717e7bd980ff7110c227055d940d01c980a5683d`

Base main:
`88a5d3fa5bfe96fe16e213e78933493394264dcc`

Branch:
`agent/esp32-native-player-death-core`

## Purpose

This milestone closes the interaction between two previously hardware-proven
owners that still had an artificial gap:

1. the active-monster sequence could move multiple monsters in order, but only
   one undelivered attack probe was tolerated;
2. PlayerDeath was native for lethal PASS_TURN hazards, but monster retaliation
   still rolled lethal attacks back.

The permanent result is one ordered active-list transaction with at most one
attack probe in flight at any instant.

## Ordered active-list contract

Legacy `Game_monsterAI()` iterates the circular active-monster list and calls
`Entity_aiThink()` for each member. The ESP32 native path now mirrors that
semantic order.

For each snapshot-prefix member:

```text
probe immediate attack gate
 -> if attack: publish one probe and pause
 -> AttackVisual completes
 -> Retaliation resolves and publishes lastResolvedProbe
 -> resume at next ordinal

otherwise
 -> selected-member movement transaction
 -> possible post-move/three-goal attack probe
 -> if attack: pause/resume at same boundary
 -> otherwise continue immediately
```

No second attack probe is published while the previous one is unresolved.
Unlike the old fail-closed guard, this no longer starves or freezes the rest of
the active list.

## Real-CYD ordered-turn proof

Four active monsters are serviced in one MOVE/PASS_TURN sequence. Ordinary
movement, subtype-4 three-goal continuation, miss resolution and later attacks
all preserve order.

Representative PASS_TURN:

```text
[MONSTERACTIVESEQ] BEGIN turn=3 reason=4 activeCount=4 ...
[MONSTERACTIVESEQ] MEMBER ... ordinal=1/4 ... publication=closed-before-next
[MONSTERACTIVESEQ] MEMBER ... ordinal=2/4 ... attackProbe=1->2 publication=probe-before-next
[MONSTERACTIVESEQ] PAUSE ... probe=2 ... nextOrdinal=3
[MONSTERRETAL] MISS-COMMIT probe=2 ... gameplayRngCommitted=yes ...
[MONSTERACTIVESEQ] RESUME ... resolvedProbe=2 ...
[MONSTERACTIVESEQ] MEMBER ... ordinal=3/4 ...
[MONSTERACTIVESEQ] MEMBER ... ordinal=4/4 ... attackProbe=2->3 ...
[MONSTERACTIVESEQ] COMPLETE ... ordered=yes publication=serialized-per-member multiAttack=one-probe-at-a-time
```

Later player-attack turns prove multiple attack pauses in one semantic turn:
sprite 237 resolves, ordinal 3 resumes, sprite 0 publishes a three-loop attack,
then ordinal 4 resumes only after that probe commits.

## Native lethal monster retaliation

A real subtype-5 retaliation reaches lethal damage from HP 4.

The probe remains prospective until its visual lease finishes:

```text
[MONSTERTURN] MEMBER-ATTACK-PROBE ... sprite=237 ... producerProbe=10 ... playerHP=4->0 ... rngRollback=yes playerExact=yes
[MONSTERATKVIS] COMPLETE probe=10 ...
```

Resolution then commits the attack, PlayerState death, and exactly one recovered
legacy death RNG byte:

```text
[GAMEPLAYHUD] REPAINT health=0/38 armor=0/28 weapon=0 ...
[PLAYERDEATH] ARM seq=10 tile=506 ... hp=0 ... rngByte=87 deathSound=5058-deferred ...
[MONSTERRETAL] LETHAL-COMMIT probe=10 ... playerHP=4->0 ... rng=41a9a848->f4415a47 attackRngCommitted=yes deathRngCommitted=yes deathOwner=armed ... turn=terminal
```

The ordered sequencer immediately discards the unprocessed suffix:

```text
[MONSTERACTIVESEQ] TERMINAL turn=7 ordinal=2/4 probe=10 cause=player-death remaining=discarded sameMonsterTurn=yes
```

The already-proven PlayerDeath presentation then reaches the native menu
boundary:

```text
[PLAYERDEATH] PHASE ... elapsedMs=752 fall=complete viewZ=9 fade=begin input=blocked
[PLAYERDEATH] READY ... elapsedMs=3012 phase=death-menu-ready viewZ=9 fade=0 frames=51 input=death-menu load=available ...
```

## Final code-review P1: raw pending-probe input gate

A post-PASS code review identified a real scheduling window in the gameplay
session order:

```text
MonsterTurn producer
 -> MonsterActivation_serviceTurn() copies producer state
 -> AttackVisual
 -> Retaliation
 -> MovementProbe / ActiveSequence
      -> member movement
      -> possible post-move attack publication
```

A movement/post-move attack can therefore increment the raw producer
`attackProbes` after Activation has already copied the filtered view for that
tick. The old input busy gate observed only the filtered view. During that one
tick, a tap could be accepted as another world action; its new `runProbe()`
would clear `lastAttackerSpriteIndex` before Activation delivered the older
pending attack, risking a stranded sequencer.

Commit `717e7bd980ff7110c227055d940d01c980a5683d` closes the race at the
correct ownership boundary: `MonsterAttackVisual_isBusy()` now observes both
the activation-delivered view and the raw MonsterTurn producer. Any raw
`attackProbes > observedAttackProbes` closes world input immediately. The
filtered activation view remains the normal delivery contract on the next
service tick.

The change is isolated to
`ESP32/src/esp_native_gameplay_monster_attack_visual.c`; no movement, RNG,
retaliation, animation cadence, player state or renderer transaction changes.

Real-CYD regression behavior:

- ordered four-monster movement remains live;
- no input accumulation was observed while deliberately tapping through the
  combat boundary;
- the user confirmed taps are not stacked;
- live memory remains `heap=116232 heap8=50308 largest8=38900`.

The supplied serial excerpt is a non-regression witness for the ordered movement
path; the no-stacking observation is physical hardware behavior rather than a
claim that the narrow race was deterministically hit in that excerpt.

Normal `esp32-cyd` CI #1290 succeeds:

```text
static RAM   = 44992 B
linked Flash = 769357 B
artifact     = 11196908002
digest       = sha256:c250af278aa1d18add1bbd87071e9d7f96d0b575199fb644d269ccc1cc3af5f5
```

## Death menu boundary

The native death menu owns touch classification. Current route status:

```text
LOAD      = live, native V9 checkpoint session replacement
JUNCTION  = fail-closed
RETRY     = fail-closed
MAIN      = fail-closed
```

The disabled routes classify and log without mutating the session. LOAD was
hardware-proven earlier on this same branch to rebuild the Sector 1 native
session from `/DoomRPG-ESP32.sav`. The final code head only changes the visual
distinction so the live LOAD row is clearly active while unowned rows are
dimmed; the user accepted the resulting hardware behavior.

## Memory / CI

Repeated real-CYD samples after movement, multiple serialized attacks, player
combat and death remain:

```text
heap=116232
heap8=50308
largest8=38900
```

Normal GitHub Actions `esp32-cyd` run #1290 / run id 36933308681: SUCCESS.

```text
static RAM  = 44992 B
linked Flash = 769357 B
artifact = 11196908002
digest = sha256:c250af278aa1d18add1bbd87071e9d7f96d0b575199fb644d269ccc1cc3af5f5
```

## Merge boundary

`717e7bd9...` is the final hardware-tested code commit for this branch.
The documentation commit following it must be Markdown-only. After merge, the
next implementation branch must be created from the exact new GitHub `main`
SHA.

The next project direction is no longer "make death work". It is structural:
continue retiring desktop-derived runtime ownership from the ESP32 build.
Audio may deliberately be promoted earlier than originally planned if current
legacy menu/UI teardown shows that sound ownership is a blocker; that decision
must come from a fresh main/ELF/source audit rather than assumption.
