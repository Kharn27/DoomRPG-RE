# Native rotation no-turn parity

Status: **CANDIDATE — awaiting real-CYD validation**

## Recovery point

```text
main = 23bdd1dfe92f860b62d5d8cede517122ac589464
branch = agent/esp32-native-rotate-no-turn
candidate code = b548321f477626777800371f0f82a9f3c2375bd9
CI = esp32-cyd run #313 / 35581250636 SUCCESS
```

Run #313 built the normal `esp32-cyd` environment at head `908beb445dd2d025bd6533c6c5545e0c585a54c9`, whose only commit after the candidate code boundary is this milestone document. Therefore the candidate code itself is CI-proven; hardware behavior remains pending.

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

## Required real-CYD validation

Use a room with at least one already-active mobile/attack-capable monster.

1. Capture the monster position/state and a stable `MONSTERTURN` scheduled count.
2. TURN_LEFT / TURN_RIGHT repeatedly without changing tile.
3. Confirm each rotation logs `ROTATE-NO-TURN`.
4. Confirm there is **no** `[MONSTERTURN] SCHEDULE ... reason=ROTATE`.
5. Confirm monsters neither move nor attack and gameplay RNG is not consumed by rotation.
6. Then MOVE one tile and confirm a normal `reason=MOVE` turn still occurs.
7. PASS_TURN and confirm `reason=PASS_TURN` still occurs.
8. Attack a monster and confirm the existing `reason=PLAYER_ATTACK` path still occurs.

Only after this witness is observed on the classic CYD may this milestone be marked hardware PASS.
