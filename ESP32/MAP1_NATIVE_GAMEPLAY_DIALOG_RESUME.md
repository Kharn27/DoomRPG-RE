# ESP32 native gameplay dialog + resume — real CYD hardware PASS

This milestone archives the first production native `EV_DIALOG` Action path on the classic ESP32-2432S028R CYD.

## Hardware-tested boundary

```text
branch = agent/esp32-native-action-dialog-resume
base main = a8e0b64dfd9c790f8896279f70427ce6fb3e9859
base main meaning = merged PR #102 native Action/door execution
hardware-tested implementation SHA = 5c53a9a02bfb7c92e4dccb0b6eba424e7d015a9b
status = REAL-CYD HARDWARE PASS
```

The tester repeatedly opened, paged, fast-forwarded, closed and re-opened the same resident Entrance dialog event. The second state-gated dialog also opened and its continuation returned the event to state 0.

No local PlatformIO build claim is made here. The Serial transcript from the physical CYD is the hardware authority.

## Permanent production boundary

The resident Action executor now owns two bounded SELECT families:

```text
1. exactly one eligible regular-door EV_OPENLINE / EV_CLOSELINE
2. first eligible EV_DIALOG / EV_DIALOGNOBACK pause,
   followed by zero or one preflighted state-only continuation
   already owned by EspMapOpcodeExecutor: 11 / 19 / 20
```

The hardware proof in this archive is specifically `EV_DIALOG` / opcode 8. `EV_DIALOGNOBACK` support exists in the bounded implementation but is not claimed hardware-proven by this run.

The dialog owner is bounded:

```text
text capacity = 384 B
page size = 4 lines
typewriter cadence = 25 ms / character
close provenance owner = 12 B
```

The native PAK remains logically open while the dialog is active so progressive font blits can reuse the resident range cache. Close/cancel closes it before the world renderer resumes.

## Real resident witness

Entrance event 88 on front tile 841:

```text
event = 88
commands = 4
range = 252..256
state 0:
  off 0 global 252 opcode 8  EV_DIALOG     string 88 = ELIGIBLE
  off 1 global 253 opcode 19 EV_NEXTSTATE              = ELIGIBLE
  off 2 global 254 opcode 8  EV_DIALOG     string 89 = STATE_MISMATCH
  off 3 global 255 opcode 11 EV_CHANGESTATE            = STATE_MISMATCH
```

First native dialog open:

```text
[DIALOG] OPEN event=88 cmd=0 resume=1 opcode=8 string=88
bytes=102 lines=7 back=1
runFlags=0x00000500
textCap=384
pack=open
```

The script is paused while the UI owns input. No world mutation occurs when the dialog opens.

## Paging and fast-forward — hardware PASS

The tester exercised both progressive text and Action fast-forward.

While text is still typing, Action completes the current page:

```text
[DIALOG] FASTFORWARD pageStart=0 lines=4
```

A following Action advances to the second page:

```text
[DIALOG] PAGE start=4/7
```

The second page then types normally. Action can also fast-forward that page:

```text
[DIALOG] FASTFORWARD pageStart=4 lines=3
```

This behavior was repeated successfully.

## Close + native continuation — hardware PASS

Closing the first dialog produced:

```text
[DIALOG] CLOSE event=88 resume=1 mode=resume ... packClosed=yes
[DIALOG] RESUME event=88 offset=1 opcode=19 global=253
state=0->1 removed=0->0 mutation=1
```

The generic resident world was then redrawn successfully:

```text
[RESIDENTGAMEPLAY] FRAME reason=DIALOG-RESUME
angle=0
frame=ed061192
sprites=6/6776
walls=3
pixels=7288
totalUs ~= 206700
presented=1
controls=idle-invisible
```

The rollback lease remains available until this redraw succeeds. The supplied run did not require rollback.

## State-gated second dialog — hardware PASS

The next world SELECT re-filtered event 88 at state 1:

```text
state 1:
  off 0 opcode 8  string 88 = STATE_MISMATCH
  off 1 opcode 19           = STATE_MISMATCH
  off 2 opcode 8  string 89 = ELIGIBLE
  off 3 opcode 11           = ELIGIBLE
```

Second dialog:

```text
[DIALOG] OPEN event=88 cmd=2 resume=3 opcode=8 string=89
bytes=10 lines=1 back=1
pack=open
```

Closing it resumed the bounded state command:

```text
[DIALOG] CLOSE event=88 resume=3 mode=resume ... packClosed=yes
[DIALOG] RESUME event=88 offset=3 opcode=11 global=255
state=1->0 removed=0->0 mutation=1
```

The tester repeated the full pair and observed the event cycle back through state 0 -> 1 -> 0 correctly.

## Regression fixed by the tested SHA

An earlier hardware run exposed a historical-probe ownership bug:

```text
SELECT during active dialog
 -> old SELECT front-tile witness ran while dialog legitimately owned pack=open
 -> witness failed its historical precondition
 -> global EspProbeLog blocking flag remained authoritative
 -> [NATIVEBOOT] BLOCKED stopped the production gameplay service
```

The tested SHA fixes both sides of that architecture leak:

1. after `TransitionPreflightFinal` has succeeded, historical startup probes no longer have runtime blocking authority;
2. SELECT consumed while a dialog is active is not sent to the historical world/front-tile SELECT witness.

Production ownership after startup is therefore:

```text
historical probes = regression witnesses only
EspNativeGameplaySession = runtime authority
```

The successful supplied run contains no later `[NATIVEBOOT] BLOCKED` and no dialog SELECT probe precondition failure.

## Memory / ownership evidence

Stable heartbeat during repeated dialog cycles:

```text
heap = 96624
heap8 = 30916
largest8 = 16372
```

Repeated closes report:

```text
packClosed=yes
```

Permanent invariants remain intact:

```text
shapeData = NULL
mediaTexels = NULL
legacy Game.entities = 0
legacy Game.monsters = 0
runtime ZIP is not the migrated gameplay backing store
/DoomRPG-ESP32.pak remains the native backing store
```

## Performance observation — not a correctness blocker

The tester reports that dialog typing and Action response sometimes feel laggy, but acceptable and functionally correct.

Measured scale in the supplied run:

```text
PlatformVideo_present ~= 34.3 ms
large dialog source text = 102 B / 7 lines
large dialog close #1 = paints 22, fontReads 3482, bytes 250626
large dialog close #2 = paints 22, fontReads 3650, bytes 262722
DIALOG-RESUME complete world redraw ~= 206.7 ms
120 ms transient touch feedback is also intentionally preserved
```

The thousands of font reads and roughly 250–263 KB read for a 102-byte dialog are a clear bounded optimization target. Future work should reduce redundant font/resource reads and presentation/recomposition cadence while preserving the recovered 25-ms logical typewriter timeline.

Do not treat this as a reason to prematurely optimize `PlatformVideo_present()` itself. The project remains turn-based and should move toward redraw-on-demand / bounded recomposition.

## Exact validated boundary

```text
EV_DIALOG open = YES
progressive typewriter = YES
Action fast-forward current page = YES
4-line paging = YES
close = YES
PAK lease closes exactly = YES
resume EV_NEXTSTATE 0->1 = YES
state-gated second dialog = YES
resume EV_CHANGESTATE 1->0 = YES
repeated cycle = YES
world redraw after resume = YES
historical probe cannot block live runtime = YES
memory heartbeat stable in supplied run = YES
```

Still intentionally deferred / fail-closed outside this bounded family:

```text
broad Game_executeEvent
unbounded multi-command dialog continuations
EV_FORCEMESSAGE / EV_NOTE production UI semantics
unproven EV_DIALOGNOBACK hardware behavior
PASS_TURN gameplay
menu / automap / weapon gameplay
combat / monsters / generic turn advance
unsupported opcode families
```

## Closeout rule

`5c53a9a02bfb7c92e4dccb0b6eba424e7d015a9b` is the hardware-tested implementation SHA for this milestone. Every commit after it on this branch must remain documentation-only before merge.
