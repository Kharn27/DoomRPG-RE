# Milestone — Native MOVE EV_SHOW batch / Entrance event43

Status: **REAL-CYD PASS**

Hardware-tested code head:

~~~text
48accf900d486d6633dd83a7568f781458e7685d
~~~

Build reference:

~~~text
esp32-cyd CI #671 = SUCCESS
static RAM = 45224 B / 327680 B
flash = 762357 B / 1310720 B
~~~

## Problem recovered on hardware

Entrance tile 377 blocked forward movement because event 43 contained more than one eligible MOVE command. A temporary raw-sequence probe established the exact event:

~~~text
off=0 global=111 id=7  EV_SHOW      arg1=1   arg2=0000020f
off=1 global=112 id=7  EV_SHOW      arg1=2   arg2=0000020f
off=2 global=113 id=7  EV_SHOW      arg1=3   arg2=0000020f
off=3 global=114 id=7  EV_SHOW      arg1=4   arg2=0000020f
off=4 global=115 id=16 EV_CLOSELINE arg1=102 arg2=000000e0
~~~

The native MOVE route previously supported one isolated SHOW, so the second eligible SHOW caused status=COMPLEX and the move stayed fail-closed.

Legacy Game_executeEvent() confirms EV_SHOW is synchronous: it updates the target sprite visual state, removes up to two same-tile blockers under the legacy rules, links the target entity when present, and continues to the next eligible bytecode. It is not a script pause.

## Permanent bounded implementation

The migrated MOVE family is deliberately narrow:

~~~text
homogeneous eligible EV_SHOW batch only
maximum count = 4
mixed eligible opcode chain = fail closed
batch > 4 = fail closed
one active SHOW batch transaction at a time
allocation = none
~~~

The permanent public MOVE result remains compact:

~~~text
EspNativeGameplayMoveEventResult = 76 B
~~~

The four per-command rollback records live in one static owner:

~~~text
MoveShowBatchOwner = 192 B
stack batch journal = 0 B
~~~

Preflight is stronger than structural inspection:

1. filter the exact event with the current event state/run flags;
2. require all eligible commands to be EV_SHOW and count <= 4;
3. apply the SHOWs sequentially to the compact sprite topology;
4. retain each exact EspMapShowResult;
5. rollback the applied prefix in reverse order;
6. verify removed-command bits are restored;
7. only then allow MOVE commit.

The real commit replays every SHOW in the same order and compares each EspMapShowResult byte-for-byte with its preflight result. Each command's REMOVE bit is then committed. Any mismatch or failure rolls the applied prefix back and fails closed.

The static owner remains leased through destination rendering. A failed MOVE/render can reverse the four removed bits and SHOW effects in reverse order. Successful destination render closes the lease.

## Stack-canary regression and fix

The first implementation stored SHOW x4 directly inside EspNativeGameplayMoveEventResult. commitMove() has multiple preflight and result instances live on the loop task stack. On real hardware, simply opening line 102 then rendering the four door-animation frames triggered:

~~~text
Guru Meditation Error: Core 1 panic'ed (Unhandled debug exception)
Debug exception reason: Stack canary watchpoint triggered (loopTask)
... |<-CORRUPTED
~~~

No event43 SHOW had executed yet. The final fix moved the batch journal to the 192-byte static owner above and restored the public result to 76 bytes.

Real-CYD retest completed all four door frames:

~~~text
[DOORANIM] FRAME 1/4 ... render=ok
[DOORANIM] FRAME 2/4 ... render=ok
[DOORANIM] FRAME 3/4 ... render=ok
[DOORANIM] FRAME 4/4 ... render=ok
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
~~~

The player then shot the Bull Demon normally. Native combat committed damage, ammo consumption, monster movement, attack visual and retaliation; the session remained alive. This is useful regression evidence that moving the batch owner off-stack did not disturb the adjacent door/combat path.

## Event43 hardware proof

On MOVE 345 -> 377:

~~~text
[MOVEEVENT] SHOW-OWNER resultBytes=76 ownerBytes=192 max=4 storage=static stackBatchBytes=0
[MOVEEVENT] SHOW-BATCH-PREFLIGHT event=43 count=4 topologyProbe=apply+reverse-rollback exact=yes mutation=no storage=static
[MOVEEVENT] ENTER-PREFLIGHT seq=6 tile=377 flags=40000401 status=SHOW_OK event=43 eligible=4 opcode=7
~~~

Committed SHOW effects:

~~~text
cmd0 sprite=1 tile=613 visual=128->0 linked=0->1 effects=0005 removed=0->1
cmd1 sprite=2 tile=619 visual=128->0 linked=0->1 effects=0005 removed=0->1
cmd2 sprite=3 tile=455 visual=128->0 linked=0->1 effects=0005 removed=0->1
cmd3 sprite=4 tile=457 visual=128->0 linked=0->1 effects=0005 removed=0->1
~~~

Then:

~~~text
[MOVEEVENT] SHOW-BATCH event=43 count=4 eligible=4 mutation=yes removedCommands=4 rollback=1 storage=static
[MOVEEVENT] COMMIT seq=6 exitEffect=0 enterEffect=1 render=ok rollbackLease=closed
[RESIDENTGAMEPLAY] MOVE ... tile=345->377 ... committed=yes
~~~

The renderer emitted its existing compact legacy guard while first rendering the newly visible room:

~~~text
[NATIVEFRAME] LEGACY_GUARD ...
[NATIVEFRAME] RETRY ...
[NATIVEFRAME] RECOVERED legacy compact guard ...
~~~

Recovery completed and the destination frame committed. This milestone does not widen or remove that renderer guard.

## Suffix/REMOVE proof

The next forward move from tile 377 proves the SHOW REMOVE bits changed event43 eligibility. The four SHOWs no longer replayed; only the original command 4 remained eligible on EXIT:

~~~text
[MOVEEVENT] EXIT-PREFLIGHT seq=7 tile=377 ... status=DOOR_OK event=43 eligible=1 opcode=16 line=102 open=1->0
[MOVEEVENT] EXIT seq=7 ... status=DOOR_OK ... mutation=yes rollback=1
~~~

The normal bounded door animation completed four frames and MOVE 377 -> 409 committed. This proves the chain is not merely traversable once: its persistent script-command semantics are correct for the observed event.

## Hardware steady witness

After the successful entry/exit sequence:

~~~text
heap=81692
heap8=16140
largest8=8692
shapeData == NULL
mediaTexels == NULL
~~~

## Remaining boundaries

This milestone does **not** generalize arbitrary multi-command MOVE events. Only homogeneous SHOW batches up to four commands are owned. Mixed chains, larger batches and other unsupported MOVE semantics remain fail-closed.

The next known regression is cold main-menu V8 Load: a valid V8 checkpoint can be reported as No Save immediately after boot while loading correctly through HUB/STAT after gameplay initialization. That issue must preserve strict corrupt-save validation and be solved at the cold read/validation/startup boundary.

## Post-pass review fix on rebased candidate

After the hardware PASS above, code review identified a separate transaction
corner not exercised by event43 itself: an EXIT-side EV_SHOW can retain the
static SHOW rollback lease while the destination ENTER side opens a dialog.
The destination frame intentionally keeps the transaction alive until the
dialog presenter succeeds. The old `finishPendingDialog()` then cleared only
`transaction`, leaving `showBatchOwner.active=1`; the next MOVE would fail its
owner-active guard.

Rebased candidate `e60211b56edc8ce8188f326b2b20b42e0661466e` factors SHOW
lease release into one shared helper and calls it from both successful commit
paths:

~~~text
normal rendered MOVE commit -> releaseShowBatchOwnerForTransaction()
pending dialog successfully opened -> releaseShowBatchOwnerForTransaction()
~~~

`esp32-cyd` CI #675 passes at 47784 B static RAM and 764129 B flash. This
review fix is code/CI validated only; the original event43 hardware PASS remains
anchored to `48accf900d486d6633dd83a7568f781458e7685d`.


## 2026-09-24 post-merge stack-headroom regression — REAL-CYD PASS

A later integration run re-exposed a loopTask stack canary while opening the same
line 102 door, this time during frame 2/4 before the SHOW event executed. The
SHOW journal itself was already static; the remaining deep renderer peak came
from two automatic `Scratch` snapshots in the native sprite renderer.

The fix keeps no new large BSS owner:

```text
saved Scratch  -> bounded heap owner for one render
after Scratch  -> removed
restoration    -> compared directly against saved owner
stack Scratch  -> 0 B
```

Runtime witness:

```text
[SPRITEPROFILE] STACK-OWNER scratchBytes=764 storage=heap
                stackScratchBytes=0 workspaceBytes=4404
                reason=loopTask-headroom
```

Real CYD then completed the full door and event43 sequence:

```text
[DOORANIM] FRAME 1/4 ... render=ok
[DOORANIM] FRAME 2/4 ... render=ok
[DOORANIM] FRAME 3/4 ... render=ok
[DOORANIM] FRAME 4/4 ... render=ok
[DOORANIM] COMPLETE ... transaction=committed

[MOVEEVENT] ENTER ... tile=377 ... status=SHOW_OK event=43 eligible=4 opcode=7
[MOVEEVENT] SHOW-BATCH event=43 count=4 ... mutation=yes
[MOVEEVENT] SHOW-LEASE RELEASE ... reason=frame-commit ... active=1->0

[MOVEEVENT] EXIT ... tile=377 ... status=DOOR_OK event=43 eligible=1 opcode=16 line=102
[DOORANIM] FRAME 1/4 ... render=ok
...
[DOORANIM] FRAME 4/4 ... render=ok
[MOVEEVENT] COMMIT ... render=ok rollbackLease=closed
```

Hardware-tested stack-fix code head:
`30be906f949bc05d4d9dfc899b1de3581dc95e10`.

The Bull Demon / Lost Soul line102 room is not the separate review corner: it
is `ENTER SHOW event43`, followed on the next move by `EXIT CLOSELINE`.

## EXIT SHOW -> ENTER DIALOG reachability census — REAL-CYD PASS

Rather than continuing to search Entrance manually, a temporary read-only census
walked all 93 event tiles and all four cardinal adjacencies. For each direction
it applied the same native event-filter rules twice: once against immutable
initial BSP state with no removed commands, and once against the current
restored script state. It looked specifically for:

```text
source EXIT  = homogeneous eligible EV_SHOW batch, count <= 4
destination ENTER = first eligible EV_DIALOG or EV_DIALOGNOBACK
```

It performed no SHOW preflight, no topology mutation, no script mutation and no
allocation.

Real-CYD witness:

```text
[MOVEEVENTCENSUS] SUMMARY events=93 candidates=0 mode=initial+current mutation=no allocation=no
```

Conclusion: **Entrance has no reachable movement pair of this exact shape** in
either its initial script state or the tested checkpoint state. The review fix
remains retained as defensive transaction cleanup, but there is no Entrance
hardware route available to exercise it directly.

The diagnostic commit was then removed. GitHub comparison from the pre-probe
tree to the post-removal tree reports zero changed files, so no census code is
left in the merge candidate.
