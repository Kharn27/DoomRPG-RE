# Native rotation no-turn parity

Status: **REAL-CYD HARDWARE PASS**

## Recovery point

```text
main = 23bdd1dfe92f860b62d5d8cede517122ac589464
branch = agent/esp32-native-rotate-no-turn
hardware-tested code = b548321f477626777800371f0f82a9f3c2375bd9
CI = esp32-cyd run #313 / 35581250636 SUCCESS
docs-only head before PASS record = b004a681cbfbe38d51b1df02fc14436d696f2552
docs-only head CI = esp32-cyd run #318 / 35581419960 SUCCESS
```

Run #313 built the normal `esp32-cyd` environment at head `908beb445dd2d025bd6533c6c5545e0c585a54c9`, whose only commit after the code boundary is this milestone document. Run #318 later passed on docs-only head `b004a681cbfbe38d51b1df02fc14436d696f2552`. The real classic CYD then validated the behavior. The hardware-tested code boundary remains `b548321f477626777800371f0f82a9f3c2375bd9`; every commit after it up to the PASS record is documentation-only.

This branch was created from the exact post-PR-139 `main`.

Checkpoint V4 remains hardware-proven at `2efb9634ffc1c2fb433c4c3340ff9c722c5c7b4d`.
The mixed physical/touch SAVE/LOAD cursor fix is merged in `main` but still lacks its dedicated real-CYD mixed-input retest; do not silently promote it to hardware-proven.

## Legacy/J2ME behavior

The reference behavior is explicit in `src/DoomCanvas.c`.

Movement completion performs:

```text
DoomCanvas_finishMovement()
 -> Game_executeTile(...)
 -> DoomCanvas_checkFacingEntity(...)
 -> DoomCanvas_uncoverAutomap(...)
 -> Game_touchTile(...)
 -> Game_advanceTurn(...)
```

Rotation completion performs:

```text
DoomCanvas_finishRotation()
 -> update viewSin/viewCos/viewStep
 -> Game_executeTile(... facing flags ...)
 -> DoomCanvas_checkFacingEntity(...)
 -> no Game_advanceTurn()
```

The TURNLEFT/TURNRIGHT input path changes only `destAngle`, updates the view and later settles through `DoomCanvas_finishRotation()`.

The J2ME runtime was also manually checked: rotating in place does not let monsters take a turn.

## Native regression

The native monster-turn observer previously treated any settled angle delta as:

```text
ESP_NATIVE_GAMEPLAY_MONSTER_TURN_ROTATE
```

which produced:

```text
[MONSTERTURN] SCHEDULE ... reason=ROTATE ...
```

and allowed the normal monster movement/attack sequence to run. This made TURN_LEFT/TURN_RIGHT equivalent to PASS_TURN for monster AI.

## Candidate fix

The observer still refreshes its angle baseline after a settled rotation, but no longer creates a monster-turn reason from an angle-only change.

A dedicated witness is emitted:

```text
[MONSTERTURN] ROTATE-NO-TURN angle=A->B scheduled=N
              mutation=no rngConsumed=0 legacyAdvance=no
```

The normal turn reasons remain:

```text
MOVE
PLAYER_ATTACK
PASS_TURN
```

No new owner, allocation, RNG call, topology mutation, renderer mutation or player mutation is added.

The existing ROTATE enum value is retained for source/history compatibility but is no longer produced by the observer.

## Real-CYD hardware proof

The user exercised the correction on the real classic CYD and confirmed the rotation behavior is now correct: turning in place no longer lets monsters take a gameplay turn.

The decisive behavioral result is:

```text
TURN_LEFT / TURN_RIGHT
 -> view angle changes normally
 -> monster position/state does not advance from the rotation
 -> no monster retaliation from the rotation
 -> no gameplay-turn advancement from the rotation
```

The provided Serial excerpt also reconfirms that the legitimate turn-producing actions still schedule the existing native monster-turn pipeline after the fix.

Player attack remains a turn source:

```text
[MONSTERTURN] SCHEDULE n=32 reason=PLAYER_ATTACK passSeq=0
[MONSTERTURN] COMPLETE reason=PLAYER_ATTACK ...
[MONSTERACTIVESEQ] COMPLETE turn=32 reason=3 ...
```

Movement remains a turn source on consecutive committed steps:

```text
[RESIDENTGAMEPLAY] MOVE n=32 seq=97 ... committed=yes
[MONSTERTURN] SCHEDULE n=33 reason=MOVE ...

[RESIDENTGAMEPLAY] MOVE n=33 seq=98 ... committed=yes
[MONSTERTURN] SCHEDULE n=34 reason=MOVE ...

[RESIDENTGAMEPLAY] MOVE n=34 seq=99 ... committed=yes
[MONSTERTURN] SCHEDULE n=35 reason=MOVE ...
```

The same session preserved normal player combat and world mutation: the Hellhound at sprite 179 was killed through the native monster-combat path, the following movement/event sequence committed correctly, and a later Armor Shard pickup committed normally. No regression was observed in rendering, collision, combat, resource pickup or monster-turn scheduling for real turn-producing actions.

Runtime memory in the provided witness remained stable:

```text
heap=79808
heap8=14076
largest8=12788
```

No new allocation or persistent owner was added by this correction.

## Merge boundary

`b548321f477626777800371f0f82a9f3c2375bd9` is the real-CYD hardware-tested code boundary for rotation no-turn parity.

Commits after that SHA are documentation-only. The branch may be treated as merge-ready for this bounded milestone once the PASS documentation tail is green in CI.

The merged mixed physical/touch SAVE/LOAD cursor fix remains a separate pending hardware regression check and is not promoted by this milestone.
