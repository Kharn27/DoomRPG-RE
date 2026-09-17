# Native soldier-door wall-cache regression

Status: **REAL-CYD HARDWARE PASS**.

Hardware-proven code boundary:

```text
3658e8a6edeb9c3d4f44fe1779e671f71374b0df
ESP32: make wall cache adaptive under door memory pressure
```

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

The transactional rollback was correct; the regression was the render failure which forced that rollback.

The same dialog sequence owns the lazy 2408-byte topology rollback snapshot, and the hardware log showed reduced free 8-bit heap at this point. The native plane cache already degraded its lease count under this kind of pressure, but the three-slot wall cache previously treated every 2048-byte lease as mandatory and also performed `free()+malloc()` on each eviction.

## Fix

`EspNativeFirstFrame` now treats the three 2048-byte wall-cache slots as a performance target rather than a rendering invariant:

- keep every wall lease that can be allocated;
- if a later lease cannot be allocated, continue with an exact narrower LRU;
- reuse an evicted slot's existing 2048-byte buffer instead of allocator churn;
- still fail closed if zero wall slots can be allocated;
- still fail closed on an actual PAK wall read failure.

Failure-only diagnostics distinguish heap pressure from storage failure:

```text
[NATIVEFRAME] WALL-CACHE-FALLBACK slots=N/3 leaseBytes=2048 totalLeaseBytes=... exact=yes
[NATIVEFRAME] WALL-CACHE-FAILED slots=0/3 leaseBytes=2048
[NATIVEFRAME] WALL-CACHE-READ-FAILED logical=... actual=... source=... leaseBytes=2048
```

No line state, script state, immutable BSP data, collision semantics, door transaction semantics, or renderer asset format is changed.

## Real-CYD validation

The user repeated the Entrance soldier sequence from normal gameplay, saved a native V3 checkpoint before opening the unlocked door, then selected line 352 under the same post-dialog memory-pressure boundary.

The machine still showed the expected reduced plane-cache width:

```text
[NATIVEPLANE] CACHE-FALLBACK slots=4/6 leaseBytes=2048 totalLeaseBytes=8192
```

All four regular-door animation frames rendered successfully. The decisive stable-open frame was:

```text
[DOORANIM] FRAME 4/4 angle=0 lines=1 geometry=stable animatedReads=0 openReads=2 textureVariants=2 frame=c3999885 render=ok
[DYNAMICLINES] FRAME angle=0 open=1 adaptedReads=2 animatedReads=0 textureVariants=2 render=ok immutableRuntime=yes
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
[RESIDENTGAMEPLAY] FRAME reason=SELECT-DOOR angle=0 frame=c3999885 sprites=17/1172 walls=44 pixels=9612 totalUs=214730 presented=1 controls=idle-invisible
[ACTION] DOOR line=352 opcode=15 status=OK open=0->1 locked=0 removed=0->0 effects=07 sound=5063
[RESIDENTGAMEPLAY] SELECT n=6 seq=71 door=352 committed=yes redraw=yes collision=live animation=regular4frame-live sound=deferred entityRelink=deferred turnAdvance=deferred
```

The door remained open physically. No `WALL_LOAD`, `WALL-CACHE-READ-FAILED`, `RENDER-FAILED`, or SELECT rollback followed.

Therefore the soldier-door wall-cache regression is **REAL-CYD HARDWARE PASS** at `3658e8a6edeb9c3d4f44fe1779e671f71374b0df`.
