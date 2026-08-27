# ESP32 native generic regular-door animation — real-CYD hardware PASS

Date: 2026-08-27
Hardware-tested implementation SHA: `ed353c0799520b82464c0066c3a53c731488c168`
Branch at validation: `agent/esp32-native-action-select-exec`
Base main: `33b05385771b45acabff6dcf14d1da2c18d1818f`
Environment: normal `esp32-cyd`

## Boundary

This milestone adds the first hardware-proven native visual interpolation for the
regular Doom RPG door family. It is deliberately map-generic: no Entrance tile,
event, line or map ID is compiled into the animator.

The permanent mutable owner is bounded to 8 active lines and occupies 76 B BSS.
`EspMapRuntime` remains immutable. The renderer obtains a transient compact line
copy, applies recovered legacy x/y displacement, reconstructs the temporary
`Line_t`, and patches only its transient texture-coordinate z values when needed.
No map-wide `Line_t`/`Vertex_t` owner is introduced.

Recovered regular-door defaults:

```text
animFrames = 4
moving frames = 3
animPos / step = 16
max moving displacement = 48
```

The fourth frame is the stable target state. Secret/MOVELINE animation remains
outside this boundary and must continue to fail closed / snap only through its
separate future family.

## Generic OPEN witness — SELECT

Resident `/intro.bsp` data resolved front tile 837 -> event 86 ->
`EV_OPENLINE` -> line 275. The line is a regular door (`flags=0x00000205`).

```text
[DOORANIM] ARM line=275 open=0->1 flags=00000205 frames=4 moving=3 step=16 ownerBytes=76 generic=yes
[DOORANIM] FRAME 1/4 angle=128 lines=1 geometry=moving animatedReads=2 openReads=0 frame=6c5debde render=ok
[DOORANIM] FRAME 2/4 angle=128 lines=1 geometry=moving animatedReads=2 openReads=0 frame=2d05fe08 render=ok
[DOORANIM] FRAME 3/4 angle=128 lines=1 geometry=moving animatedReads=2 openReads=0 frame=a522f925 render=ok
[DOORANIM] FRAME 4/4 angle=128 lines=1 geometry=stable animatedReads=0 openReads=2 frame=7105fa5f render=ok
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
[ACTION] DOOR line=275 opcode=15 status=OK open=0->1 locked=0 removed=0->0 effects=07 sound=5063
```

Wall-pixel progression was also visibly non-instantaneous:

```text
frame 1: 11297
frame 2: 10481
frame 3: 9665
frame 4: 9339
```

The fourth frame matches the already-validated fully-open frame FNV `7105fa5f`.

## Generic CLOSE witness — MOVE source/EXIT event

The player then backed away from tile 838. The generic movement-event path
preflighted and executed resident event 87 -> `EV_CLOSELINE` -> line 275.

```text
[MOVEEVENT] EXIT-PREFLIGHT seq=8 tile=838 flags=00000420 status=DOOR_OK event=87 eligible=1 opcode=16 unsupported=0 line=275 open=1->0 locked=0 removed=0->0 mutation=no
[DOORANIM] ARM line=275 open=1->0 flags=00000205 frames=4 moving=3 step=16 ownerBytes=76 generic=yes
[MOVEEVENT] EXIT seq=8 tile=838 flags=00000420 status=DOOR_OK event=87 eligible=1 opcode=16 unsupported=0 line=275 open=1->0 locked=0 removed=0->0 mutation=yes
[DOORANIM] FRAME 1/4 angle=128 lines=1 geometry=moving animatedReads=2 openReads=0 frame=35e3784d render=ok
[DOORANIM] FRAME 2/4 angle=128 lines=1 geometry=moving animatedReads=2 openReads=0 frame=d005cd93 render=ok
[DOORANIM] FRAME 3/4 angle=128 lines=1 geometry=moving animatedReads=2 openReads=0 frame=808e96c7 render=ok
[DOORANIM] FRAME 4/4 angle=128 lines=1 geometry=stable animatedReads=0 openReads=0 frame=808e96c7 render=ok
[MOVEEVENT] COMMIT seq=8 exitDoor=1 enterDoor=0 render=ok rollbackLease=closed
[DOORANIM] COMPLETE transitions=1 frames=4 state=stable transaction=committed
```

The stable closed frame is the previously validated closed frame FNV
`808e96c7`. The MOVE transaction keeps its rollback lease until the stable
fourth frame is rendered successfully, then closes atomically.

## Hardware conclusions

- SELECT `EV_OPENLINE` and MOVE `EV_CLOSELINE` both arm the same generic animator.
- OPEN and CLOSE both render three moving geometries plus one stable frame.
- The immutable runtime is preserved (`immutableRuntime=yes`).
- World raster and sprite-depth both observe the animated transient geometry.
- Fully-open dynamic line adaptation remains active after the moving frames.
- Final close restores the exact stable closed frame.
- No crash/reboot was reported.
- Stable heartbeat after the sequence: heap `97448`, heap8 `31740`, largest8 `8692`.
- `shapeData` and `mediaTexels` remain outside this native path and the project invariants remain unchanged.

## Performance observation, not a correctness failure

The tester judged the animation correct and visually clear, but "un poil lent",
consistent with navigation also feeling slightly slow. Display presentation itself
is roughly 34 ms in this run; complete gameplay redraws around this area are still
roughly 0.32-0.35 s. The next performance work should therefore target bounded
recomposition/cache/redraw scheduling rather than prematurely changing
`PlatformVideo_present()`.

No timing tweak is made in this hardware-PASS closeout.

## Known deferred families / stale diagnostics

- secret/MOVELINE door animation;
- door sound playback;
- legacy entity relink objects;
- broad MOVE event opcode execution beyond the bounded door family;
- turn advance / monster AI;
- SELECT dialog/UI families not yet wired into production gameplay.

Two production log tokens are now historically stale:

```text
animation=deferred
tileEvents=deferred
```

They do not describe the validated regular-door paths anymore. They are left
unchanged in this closeout so every post-test code commit remains absent; fix the
labels in a later tested implementation milestone.
