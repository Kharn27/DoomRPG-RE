# Native soldier-door wall-cache regression

Status: **CANDIDATE — awaiting real-CYD re-test**.

## Hardware failure witness

The Entrance soldier dialog chain itself completed correctly and unlocked the regular door:

```text
[DIALOGCHAIN] RESUME event=60 start=9 handled=4 show=0 hide=1 unlock=1 state=2 removed=1 mutation=1 topologySnapshot=2408B
[DOORANIM] ARM line=352 open=0->1 flags=00000605 frames=4 moving=3 step=16 ownerBytes=76 generic=yes
[ACTION] SELECT ... status=DOOR_OK tile=612 event=62
```

Moving animation frames 1..3 rendered successfully. The final stable-open frame then failed while acquiring a wall-cache lease for the newly visible line 234:

```text
[NATIVEFRAME] FAILED route=gameplay code=7/WALL_LOAD ... line=234 logical=0 actual=0 flags=00000205 source=0
[DOORANIM] FRAME 4/4 ... geometry=stable ... openReads=1 ... render=failed
[RESIDENTGAMEPLAY] RENDER-FAILED reason=SELECT-DOOR angle=0
[RESIDENTGAMEPLAY] SELECT ROLLBACK ... line=352 open=0 restored=yes
```

The transactional rollback was correct; the regression is the render failure which forces that rollback.

The same dialog sequence owns the lazy 2408-byte topology rollback snapshot, and the hardware log shows reduced free 8-bit heap at this point. The native plane cache already degrades its lease count under this kind of pressure, but the three-slot wall cache previously treated every 2048-byte lease as mandatory and also performed `free()+malloc()` on each eviction.

## Candidate fix

`EspNativeFirstFrame` now treats the three 2048-byte wall-cache slots as a performance target rather than a rendering invariant:

- keep every wall lease that can be allocated;
- if a later lease cannot be allocated, continue with an exact narrower LRU;
- reuse an evicted slot's existing 2048-byte buffer instead of allocator churn;
- still fail closed if zero wall slots can be allocated;
- still fail closed on an actual PAK wall read failure.

New failure-only diagnostics distinguish heap pressure from storage failure:

```text
[NATIVEFRAME] WALL-CACHE-FALLBACK slots=N/3 leaseBytes=2048 totalLeaseBytes=... exact=yes
[NATIVEFRAME] WALL-CACHE-FAILED slots=0/3 leaseBytes=2048
[NATIVEFRAME] WALL-CACHE-READ-FAILED logical=... actual=... source=... leaseBytes=2048
```

No line state, script state, immutable BSP data, collision semantics, door transaction semantics, or renderer asset format is changed.

## Required real-CYD test

Repeat the exact Entrance witness:

1. talk to the soldier until event 60 performs `unlock=1`;
2. face/select door line 352;
3. observe all four animation frames;
4. verify the final stable-open frame renders successfully;
5. verify the door remains open and the player can walk through it;
6. confirm no `WALL-CACHE-READ-FAILED`, `WALL_LOAD`, `RENDER-FAILED`, or SELECT rollback occurs.

A `WALL-CACHE-FALLBACK` line is expected and useful if the hardware reaches the same memory-pressure boundary; it is not itself an error.
