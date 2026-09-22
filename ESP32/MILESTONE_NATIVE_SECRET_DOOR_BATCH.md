# Milestone — Native secret-door multi-line SELECT batch

## Scope

This milestone closes one bounded SELECT/event semantic exposed by the hidden door in `/intro.bsp`: one selectable tile event may contain more than one eligible line command that must execute in the same `Game_runEvent()` pass.

It does **not** generalize arbitrary mixed event scripts. Only the pure line family is accepted:

- `EV_MOVELINE` (6)
- `EV_OPENLINE` (15)
- `EV_CLOSELINE` (16)
- `EV_MOVELINE2` (17)

The batch is bounded to eight commands, matching the legacy/native `openDoors[8]` capacity.

## Legacy behavior recovered

Legacy `Game_runEvent()` walks all eligible commands in event order. The `EV_*LINE` cases call `Game_performDoorEvent()`; execution continues to the next command unless a command returns false or another pause boundary is hit.

Therefore an event containing two eligible `EV_OPENLINE` commands is not a “complex unsupported event” in the legacy engine. Both line changes belong to one SELECT transaction.

## Permanent native contract

The native SELECT owner now applies this rule only to a pure door batch:

1. Resolve and filter the tile event using the normal SELECT flags.
2. Reject mixed opcode families or more than eight eligible line commands.
3. Preview every door command without mutating the line overlay.
4. Reject unsupported duplicate target lines that would require sequential intermediate-state planning.
5. Commit each line mutation in event order.
6. Commit each command's remove-if-handled bit when requested.
7. If any command or later redraw fails, restore all earlier line and removed-bit mutations.
8. Let the existing door animator own regular-door visual slots; non-regular line geometry remains a validated snap transition.

No legacy world/entity/render mutation is reintroduced.

## Real-CYD hardware evidence

The hidden door was reached on `/intro.bsp` at tile `195`. The exact event corpus was first recovered as:

```text
[ACTION] SELECT seq=135 status=COMPLEX_EVENT tile=195 event=10 eligible=2 unsupported=0
[ACTIONTRACE] event=10 tile=195 commands=2 firstGlobal=36 raw-sequence off0=id15/a1=000001d7/a2=00000300 off1=id15/a1=000001d6/a2=00000300
```

Thus:

- event = `10`
- tile = `195`
- first global command = `36`
- command 0 = `EV_OPENLINE`, line `0x1d7 = 471`
- command 1 = `EV_OPENLINE`, line `0x1d6 = 470`
- both commands include remove-if-handled

After the bounded batch implementation, the real CYD produced:

```text
[DOORANIM] SNAP line=471 open=0->1 flags=00000928 reason=non-regular-door
[DOORANIM] SNAP line=470 open=0->1 flags=00001110 reason=non-regular-door
[ACTION] SELECT seq=139 status=DOOR_OK tile=195 event=10 eligible=2 unsupported=0
[DYNAMICLINES] FRAME angle=64 open=4 adaptedReads=6 animatedReads=0 textureVariants=0 render=ok immutableRuntime=yes
[ACTION] DOOR-BATCH event=10 count=2 status=OK [0]line=471/op=15/open=0->1/removed=0->1 [1]line=470/op=15/open=0->1/removed=0->1
[RESIDENTGAMEPLAY] SELECT n=10 seq=139 doors=2 firstDoor=471 committed=yes redraw=yes collision=live animation=bounded-batch sound=deferred entityRelink=deferred turnAdvance=deferred
```

The user visually confirmed that the secret door opened.

The player then crossed the opened tile successfully. The event no longer had eligible commands:

```text
[MOVEEVENT] ENTER-PREFLIGHT seq=140 tile=195 flags=10000404 status=NO_ELIGIBLE event=10 eligible=0 ...
[MOVEEVENT] EXIT-PREFLIGHT seq=141 tile=195 flags=00000410 status=NO_ELIGIBLE event=10 eligible=0 ...
```

That proves both remove-if-handled command bits were committed, not merely the visible line open state.

The same hardware log remained resident after traversal:

```text
[ALIVE] uptime=268125 ms heap=78832 heap8=13280 largest8=12276 SD=ready ZIP=ready VIDEO=ready CORE=ready LAYOUT=ready PRERENDER=ready RENDER=ready MAPPINGS=ready MENUBSP=ready touchIRQ=idle
```

## RAM / build boundary

The validated code head retained the canonical no-PSRAM static-RAM boundary in `esp32-cyd` CI:

```text
RAM:   [=         ]  13.7% (used 44832 bytes from 327680 bytes)
Flash: [======    ]  55.3% (used 724577 bytes from 1310720 bytes)
```

No permanent RAM growth was introduced for this transaction owner.

## Result

**PASS on real classic CYD.**

The hidden-door case is now part of the hardware-proven native SELECT/door semantics. Arbitrary mixed events and other deferred families remain fail-closed.
